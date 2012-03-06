/*
 * mm-awesome.c - The fastest, awesomest malloc package.
 * 
 * This package uses segmented free lists to keep track of
 * free blocks of memory. Coalescing blocks is delayed until
 * no free blocks are availble. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

//#define DEBUG 0

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name (id1+id2)*/
    "bpan+andylei",
    /* First member's full name */
    "Bohao Pan",
    /* First member's email address */
    "bpan@fas.harvard.edu",
    /* Second member's full name (leave blank if none) */
    "Andy Lei",
    /* Second member's email address (leave blank if none) */
    "andylei@fas.harvard.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* initially distribute this many blocks to lists */
#define INIT_SBRK ALIGN(8 * 1024)

/* cutoff for exactly sized lists */
#define MIN_APPROX_SIZE (0x100)
#define SMALL_LIST_SIZE (MIN_APPROX_SIZE / ALIGNMENT)
#define LARGE_LIST_SIZE 24

/* minimum size for a block */
#define MIN_BLOCK_SIZE (SIZE_T_SIZE * 3)

// block traversal macros
#define PAYLOAD(block_header) ((header **)((char *) block_header + SIZE_T_SIZE))
#define HEADER(payload_ptr) ((header *)((char *) payload_ptr - SIZE_T_SIZE))
#define NEXT_FREE(block_header) (*PAYLOAD(block_header))
#define PREV_FREE(block_header) (*(PAYLOAD(block_header)+1))

#define GET_SIZE(block_header) (*block_header & ~0x1)
#define NEXT(block_header) ((header *)((char *) block_header + GET_SIZE(block_header)))


#define MARK_ALLOC(block_header) (*block_header = *block_header | 0x1)
#define MARK_FREE(block_header) (*block_header = (*block_header | 0x1) - 1)

/* return false if its a free block */
#define IS_ALLOCED(block_header) (*block_header % 2)
#define IS_FREE(block_header) (~IS_ALLOCED(block_header))

typedef size_t header;

void print_heap();
void print_block();
void print_all();
void print_all_free();
void add_to_llist(header * node, header * after);
void add_to_freelist(header * freeblock);
int mm_check();

/* =========  Global Variables  ========= */

/*
 * small items ( <= 256 bytes) are stored in small_free 
 * large items are stored in large_free.  large items are binned 
 *  exponentially. large_free[n] contains items with size greater than
 *  0x100 << n and less than or equal to 0x100 << (n + 1).
 */
header ** small_free;
header ** large_free;

/* 
 * arrays which tell you how many items are in each list
 */
int * small_free_n;
int * large_free_n;

header * heap_lo;
header * heap_hi;
/* how much memory to sbrk (we increase by a factor of 2 every time */
int sbrkCounter = 4*1024*1024;
/*
 * the beginning of the region of usable memory (since we use the beginning
 * of the heap for internally used arrays).
 */
header * mem_start;
header * end;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
#ifdef DEBUG
    printf("MM_INIT\n");
#endif

        int i;
        
        heap_lo = mem_sbrk(sbrkCounter);
    heap_hi = mem_heap_hi();
        /* set up our original arrays */
        small_free = (header **) heap_lo;
        large_free = small_free + SMALL_LIST_SIZE;
    small_free_n = (int *) large_free + LARGE_LIST_SIZE;
    large_free_n = small_free_n + SMALL_LIST_SIZE;
        
        header * cur = (header *) (large_free_n + LARGE_LIST_SIZE);
    mem_start = cur;
    
#ifdef DEBUG
    printf("\n******\nHeap: %p to %p\n", heap_lo, heap_hi);
    printf("Small list size: %d\n", SMALL_LIST_SIZE);
    printf("Dummies start at %p\n", cur);
#endif
        // setup the exact segments
        for(i = 0; i < SMALL_LIST_SIZE; i++)
        {
                small_free_n[i] = 0;
                if( (i+1) * ALIGNMENT < MIN_BLOCK_SIZE)
                {
                        small_free[i] = NULL;
                } else 
                {
                        // the first element of the list is a dummy block
                        small_free[i] = (header *) cur;
                        *cur = MIN_BLOCK_SIZE;
                        MARK_ALLOC(small_free[i]);
                        NEXT_FREE(small_free[i]) = NULL;
                        PREV_FREE(small_free[i]) = NULL;
                        cur = (size_t *) NEXT(small_free[i]);
                }
        }

        // setup the approx segments
        for(i = 0; i < LARGE_LIST_SIZE; i++)
        {
                large_free_n[i] = 0;
                large_free[i] = (header *) cur;
                *cur = MIN_BLOCK_SIZE;
                MARK_ALLOC(large_free[i]);
                NEXT_FREE(large_free[i]) = NULL;
                PREV_FREE(large_free[i]) = NULL;
                cur = (header *) NEXT(large_free[i]);
        }
        
        // the initial, huge, honking block
        *cur = (header) heap_hi - (header) cur + 1;
        add_to_freelist((header *) cur);
    
#ifdef DEBUG
        if(!mm_check())
                assert(0);
#endif
        return 0;
}

/*
 * pre-break up memory into blocks and add them to our lists
 *  size is the size of the area to split
 */
void init_split(void * to_split, size_t size)
{
        
}

/*
 * finds which segment to add the free block to.  assumes that 
 *  free_block is a valid block
 */
void add_to_freelist(header * free_block)
{
        int i;
        size_t size = (size_t) GET_SIZE(free_block);
#ifdef DEBUG
    printf("Adding to free list %p of size %d\n", free_block, size);
#endif
        if(size <= MIN_APPROX_SIZE)
        {
                i = size / ALIGNMENT - 1;
                add_to_llist(free_block, small_free[i]);
        small_free_n[i]++;
        } else 
        {
                for(i = 0; i < LARGE_LIST_SIZE; i ++)
                {
                        if( (unsigned long) MIN_APPROX_SIZE << (i + 1) > size )
                        {
                                add_to_llist(free_block, large_free[i]);
                large_free_n[i]++;
                                return;
                        }
                }
                assert(0);
        }
}

/************  Doubly Linked List Functions  *************/
/*
 * Remove node from doubly linked list
 */
void remove_from_llist(header * node)
{
    header * nextfree = NEXT_FREE(node);
    header * prevfree = PREV_FREE(node);
    
        if(prevfree != NULL)
                NEXT_FREE(prevfree) = nextfree;
        if(nextfree != NULL)
                PREV_FREE(nextfree) = prevfree;
}

/*
 * Add node to a doubly linked list after after
 */
void add_to_llist(header * node, header * after)
{
        header * next = NEXT_FREE(after);
        
        NEXT_FREE(after) = node;
        PREV_FREE(node) = after;
        
        NEXT_FREE(node) = next;
        if(next != NULL)
                PREV_FREE(next) = node;
}

/*
 * Mark the block as allocated, possibly split the block, remove
 * the allocated block from the free list, and return a pointer to
 * the payload region of the allocated block.
 * Assumes that block is at least of size size.
 */
void * allocate(header * block, size_t size)
{
#ifdef DEBUG
        printf("Allocating %p of size %d, for size %lu", block, (size_t)*block, (long unsigned int) size);
#endif

    // split the block
    remove_from_llist(block);
    if(*block < size) {
        printf("ERROR!! BLOCK SIZE (%d) < ALLOCATE SIZE (%d)", *block, size); 
        assert(0);
    }
    
        size_t split_size = (size_t)*block - size;
    
    if(split_size >= MIN_BLOCK_SIZE)
    {
#ifdef DEBUG
                printf(", split size: %lu.\n", (long unsigned int) split_size);
#endif
                // split the block in two
        *block = size;
                header * split = NEXT(block); //second half of the split block
                *split = split_size;
        
        // add split block to free list
        MARK_FREE(split);
        add_to_freelist(split);
    } else 
        {
#ifdef DEBUG
                printf(", no splitting.\n");
#endif
                remove_from_llist(block);
        }
        
    //printf("back to allocate...\n");
    
    // mark the block as allocated
    MARK_ALLOC(block);
    
        // pointer to the payload
    return (void *) PAYLOAD(block);
}

/* 
 * coalesce() - Coalesces free memory until memory of size size is created
 *  at which point coalesce returns a pointer to a block with size at least
 *  size.  Assumes that size is already aligned.  Returns NULL if colaescing
 *  finishes without finding such a block.  If size is 0, then finishes
 *  coalescing and returns NULL.
 */
header * coalesce(size_t size)
{
#ifdef DEBUG
        printf("Running coalesce...\n");
#endif
    int i = MIN_BLOCK_SIZE / ALIGNMENT - 1;
    for (; i < SMALL_LIST_SIZE + LARGE_LIST_SIZE - 1; i++) {
        //printf("Trying to coalesce size %d...\n", i);
        header * cur = NEXT_FREE(small_free[i]);
        header * next;
        
        while (cur != NULL) {
            //printf("foo\n");
            next = NEXT(cur);
            //printf("%p %d\n", next, (size_t)IS_FREE(next));
            if (next != NULL && next < heap_hi && GET_SIZE(next) >= MIN_BLOCK_SIZE && (long unsigned int) IS_ALLOCED(next) == 0) {
                
                #ifdef DEBUG
                printf("    coalescing %p (%d) and %p (%d)...\n", cur, *cur, next, *next);                print_block(next);
                #endif
                
                remove_from_llist(cur);                
                remove_from_llist(next);
                
                *cur += *next;
                add_to_freelist(cur);
                
                if (size && *cur >= size) {
                    return cur;
                }
            }
            else {
                cur = NEXT_FREE(cur);
            }
        }
    }

        return NULL;
}

header * find_free(size_t size)
{
#ifdef DEBUG
    if(!mm_check())
                assert(0);
    printf("    finding free block of size %lu... ", (unsigned long) size);
#endif
    int i;
    
    // try to fit into fixed size buckets first
    if (size <= MIN_APPROX_SIZE)
        {
        for (i = size/ALIGNMENT-1; 
                         i < SMALL_LIST_SIZE && NEXT_FREE(small_free[i]) == NULL;
                         i++){}
        
        if (i < SMALL_LIST_SIZE) 
                {
#ifdef DEBUG
            printf("%d, %p (%lu) found in small free list!\n", 
                                   i, NEXT_FREE(small_free[i]), 
                                   (unsigned long)*NEXT_FREE(small_free[i]));
#endif
            small_free_n[i]--;
            return NEXT_FREE(small_free[i]);
        }
    }
    
    // either no small buckets exist, or size is too large    
    if (size <= MIN_APPROX_SIZE)
        {
        i = 0;
    } else
        {
        for(i = 0; i < LARGE_LIST_SIZE; i ++)
                {
                        if( MIN_APPROX_SIZE << (i + 1) > size )
                                break;
                }
    }
    
    while(i < LARGE_LIST_SIZE)
        {
        if (NEXT_FREE(large_free[i]) != NULL)
                {
            header * curr = NEXT_FREE(large_free[i]);
            while (curr != NULL)
                        {
                if (GET_SIZE(curr) >= size)
                                {
                    #ifdef DEBUG
                    printf("%d, %p (%x) found in large free list!\n", 
                                                   i, curr, GET_SIZE(curr));
                    print_block(curr);
                    #endif
                    large_free_n[i]--;
                    return curr;
                }
                curr = NEXT_FREE(curr);
            }
        }
                i++;
    }
    
    // NEED TO COALESCE OR SBRK MORE MEMORY'
#ifdef DEBUG
    printf("\n\nNEED MORE MEMORY!! size = %d\n", size);
    //print_all_free();
    //print_heap();
#endif
    return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
#ifdef DEBUG
    printf("MM_MALLOC %d\n", size);
    
    if(!mm_check())
                assert(0);
#endif
    size_t block_size = ALIGN(size + SIZE_T_SIZE);
    if (block_size < MIN_BLOCK_SIZE) {block_size = MIN_BLOCK_SIZE;}
    
    /* find a free block in the lists */
    header * block = find_free(block_size);
    if(block == NULL)
    {
                // coalesce, find a free block
                block = coalesce(block_size);
                
        if (block != NULL)
                {
            #ifdef DEBUG
            printf("coalesce returning: ");
            print_block(block);
            #endif
            return allocate(block, block_size);
        }
        
        // coalesce didn't work, need to sbrk
        else
                {
            #ifdef DEBUG        
            printf("Coalesce did not work... need to SBRK!\n");
            #endif
            
            for( ; block_size > sbrkCounter; sbrkCounter *= 2) {}
            
            block = mem_sbrk(sbrkCounter);
            heap_hi = mem_heap_hi();
            *block = (header) heap_hi - (header) block + 1;
            MARK_FREE(block);
            add_to_freelist(block);
            
            #ifdef DEBUG
            printf("SBROKE %d...\n", sbrkCounter);
            #endif
            
            return allocate(block, block_size);
        }
                return NULL;
    }
    
    void * ptr = allocate(block, block_size);
#ifdef DEBUG
        if(!mm_check())
                assert(0);
#endif
    return ptr;
}

/*
 * mm_free - Freeing a block
 */
void mm_free(void *ptr)
{
    header * block = HEADER(ptr);
    
#ifdef DEBUG
    printf("MM_FREE %p\n", ptr);
        printf("Freeing %p, w/ size %lu\n", ptr, (long unsigned int) GET_SIZE(block));
#endif

        MARK_FREE(block);
        add_to_freelist(block);
        
#ifdef DEBUG
        if(!mm_check())
                assert(0);
#endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


/*
 * returns 1 if things are consistent.  consistent means
 *  1) all blocks are at least size MIN_BLOCK_SIZE
 */
int mm_check()
{
        printf("\nChecking consistency of the heap...\n");
        header * cur = mem_start;
        while(cur < heap_hi - 7)
        {
                size_t s = GET_SIZE(cur);
                if(*cur < MIN_BLOCK_SIZE || ALIGN(s) != s)
                {
            //print_all_free();
            //print_heap();
                        printf("ERROR: block at %p has size %lu\n", 
                                   cur, (unsigned long) s);
                        
            printf("ERROR: block at %p has size %lu\n", 
                                   cur, (unsigned long) s);
            print_block(cur);
            return 0;
                }
                cur = NEXT(cur);
        }
        return 1;    
}
    
void print_heap() 
{
    printf("***********\nCURRENT MEMORY: \nHeap %p:%p\n************\n", heap_lo, heap_hi);
    header * cur = mem_start;
    while (cur < heap_hi-7) {
        print_block(cur);
        cur = NEXT(cur);
    }
    printf("\n\n");
}

void print_block(header * block)
{
        printf("%p | prev: %8p | next: %8p | size: %6u %x | allocated: %1lu\n", 
                   block, PREV_FREE(block), NEXT_FREE(block),
                   GET_SIZE(block), GET_SIZE(block), (long unsigned int) IS_ALLOCED(block)
        );
}

/*
 * print_all() - Prints every block in the memory structure
 */
void print_all()
{
    header * cur = heap_lo;
    while(cur < end)
    {
        print_block(cur);
        cur = NEXT(cur);
    }
        printf("\n");
}

/*
 * print_free() - Prints all the free memory, in order
 */
void print_free(header * head)
{
        header * cur = NEXT_FREE(head);
        while(cur != NULL)
        {
                print_block(cur);
                cur = NEXT_FREE(cur);
        }
        //printf("\n");
}

/*
 * print_all_free() - Prints all free memory blocks
 */
void print_all_free() {
    int i; header * cur;
    printf("****\n");
    
    for(i = MIN_BLOCK_SIZE / ALIGNMENT - 1; i < SMALL_LIST_SIZE; i++)
        {
        printf("small free blocks of size %d * 8: ", i+1);
        
        for(cur = NEXT_FREE(small_free[i]); cur != NULL; cur = NEXT_FREE(cur))
            printf("%p ", cur);
            
                //printf("%d", small_free_n[i]);
        printf("\n");
        }
        
        for(i = 0; i < LARGE_LIST_SIZE; i++)
        {
        printf("large free blocks of size > %d: ", 256 * (0x1 << i));
                for(cur = NEXT_FREE(large_free[i]); cur != NULL; cur = NEXT_FREE(cur))
            printf("%p ", cur);
        //printf("%d", large_free_n[i]);
        printf("\n");
        }
}
