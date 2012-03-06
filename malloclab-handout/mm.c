/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"Prostatus",
	/* First member's full name */
	"Andrew Flockhart",
	/* First member's NYU NetID*/
	"abf277@nyu.edu",
	/* Second member's full name (leave blank if none) */
	"Hursh Agrawal",
	/* Second member's email address (leave blank if none) */
	"ha470@nyu.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

//MACROS FROM BOOK - TEXT pg 830
#define WSIZE       4       //header/footer size
#define DSIZE       8       //total overhead size
#define CHUNKSIZE  (1<<12)  //amnt to extend heap by

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc)) //puts size and allocated byte into 4 bytes

#define GET(p)       (*(unsigned int *)(p)) //read word at address p
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) //write word at address p

#define GET_SIZE(p)  (GET(p) & ~0x7) //extracts size from 4 byte header/footer
#define GET_ALLOC(p) (GET(p) & 0x1) //extracts allocated byte from 4 byte header/footer

#define HEADER(ptr)       ((char *)(ptr) - WSIZE) //get ptr's header address
#define FOOTER(ptr)       ((char *)(ptr) + GET_SIZE(HEADER(ptr)) - DSIZE) //get ptr's footer address

#define NEXT(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE))) //next block
#define PREVIOUS(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE))) //prev block

#define INIT_ARRAY_SIZE

/* whether or not array free list is implemented*/
#define ARRAY_IMPLEMENTATION 0

int freeArraySize = 10;
size_t* FREE_ARRAY[10];

static char *firstBlock = 0;  //ptr to first block in list
int test = 0;

/* Function prototypes for internal helper routines */
static void *enlarge(size_t size);

static int mm_check(void);
static void expandArray(void);
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	void* ptr;
	size_t size;
	int i;

	for (i=0; i<freeArraySize; i++) {
		FREE_ARRAY[i] = (size_t)NULL;
	}

	firstBlock = mem_sbrk(4*WSIZE); //sbreaks out the first few bytes - to create prologue/epilogue

    if (firstBlock == (void *)-1) {
		return -1;
	}

	FREE_ARRAY[0] = (size_t*)firstBlock;

	//FROM TEXT - pg 831
    PUT(firstBlock, 0); //for alignment
    PUT(firstBlock + (1*WSIZE), PACK(DSIZE, 1)); //header for prologue entry/node (8 bytes)
    PUT(firstBlock + (2*WSIZE), PACK(DSIZE, 1)); //footer for prologue entry/node (8 bytes)
    PUT(firstBlock + (3*WSIZE), PACK(0, 1)); //header for epilogue node (only 4 bytes)
    firstBlock += (2*WSIZE); //moves the pointer up to between prologue/epilogue



	/* sbreak out a heap for free space */
	if ((CHUNKSIZE/WSIZE)%2) { //ensures we sbrk an even number of words (WSIZEs) to make sure heap is aligned by 8
		size = ((CHUNKSIZE/WSIZE) + 1) * WSIZE;
	} else {
		size = CHUNKSIZE;
	}

	ptr = mem_sbrk(size);
	if ((long)ptr == -1) { //if mem_sbrk didn't work
		return -1;
	}

	FREE_ARRAY[1] = (size_t*)ptr;

	//mark header/footer/epilogue header for new, gigantic free heap
    PUT(HEADER(ptr), PACK(size, 0)); //free-block header
    PUT(FOOTER(ptr), PACK(size, 0)); //free-block footer
    PUT(HEADER(NEXT(ptr)), PACK(0, 1)); //New epilogue header

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    mm_check();

    size_t asize; //size including overhead
	size_t slotSize; //size of empty slot found
    char* ptr;
	void* findPtr; //pointer for finding the next empty slot

	//BASED ON BOOK MALLOC - pg 834
    if (firstBlock == 0) { //checks to see if init has even been called
		mm_init();
    }

    if (size == 0) { //check for ignoring if no size given
		return NULL;
	}

	asize = ALIGN(size) + DSIZE; //aligns and adds overhead

    /* searches the free list for a free block */
	ptr = NULL;

    for (int i = 0; i <freeArraySize; i++) {
		if  (asize <= GET_SIZE(HEADER(FREE_ARRAY[i]))) { //if a big enough block is found
			ptr = FREE_ARRAY[i];
			break;
		}
    }
	/* Convert free block to used */
    if (ptr != NULL) {
		slotSize = GET_SIZE(HEADER(ptr));

		//ensures the remainder of free slot is big enough to be its own free slot
	    if ((slotSize - asize) >= (2*DSIZE)) {
			PUT(HEADER(ptr), PACK(asize, 1));
			PUT(FOOTER(ptr), PACK(asize, 1));
			ptr = NEXT(ptr);
			PUT(HEADER(ptr), PACK(slotSize-asize, 0));
			PUT(FOOTER(ptr), PACK(slotSize-asize, 0));
			ptr = PREVIOUS(ptr);
	    }
	    else {
			PUT(HEADER(ptr), PACK(slotSize, 1));
			PUT(FOOTER(ptr), PACK(slotSize, 1));
	    }

		return ptr;
    }

	ptr = enlarge(MAX(asize,CHUNKSIZE)); //make the heap bigger

    if (ptr == NULL)
		return NULL;

    slotSize = GET_SIZE(HEADER(ptr));

	//ensures the remainder of free slot is big enough to be its own free slot
    if ((slotSize - asize) >= (2*DSIZE)) {
		PUT(HEADER(ptr), PACK(asize, 1));
		PUT(FOOTER(ptr), PACK(asize, 1));
		ptr = NEXT(ptr);
		PUT(HEADER(ptr), PACK(slotSize-asize, 0));
		PUT(FOOTER(ptr), PACK(slotSize-asize, 0));
		ptr = PREVIOUS(ptr);
    }
    else {
		PUT(HEADER(ptr), PACK(slotSize, 1));
		PUT(FOOTER(ptr), PACK(slotSize, 1));
    }

    return ptr;
}

/*
 * mm_free - frees blocks
 */
void mm_free(void *ptr)
{
	mm_check();

	size_t prevBlock;
	size_t nextBlock;
	size_t size;
        int i;
        int inserted = 0;


	if (firstBlock == 0){ //checks to see if init has even been called
		mm_init();
    }

    if (ptr == 0) //checks to see if ptr is valid
		return;

    size = GET_SIZE(HEADER(ptr));

    PUT(HEADER(ptr), PACK(size, 0));
    PUT(FOOTER(ptr), PACK(size, 0));

    prevBlock = GET_ALLOC(FOOTER(PREVIOUS(ptr)));
    nextBlock = GET_ALLOC(HEADER(NEXT(ptr)));
	//start coalescing - TEXT PG 833
    if (prevBlock && nextBlock) {  /* if both prev/next are allocated */
		//do nothing
    } else if (prevBlock && !nextBlock) { /* if only next block is free */
		size += GET_SIZE(HEADER(NEXT(ptr)));
		PUT(HEADER(ptr), PACK(size, 0));  //same header pos, new size
		PUT(FOOTER(ptr), PACK(size,0)); //new footer pos, new size
    } else if (!prevBlock && nextBlock) { /* if only prev block is free */
		size += GET_SIZE(HEADER(PREVIOUS(ptr)));
		PUT(HEADER(PREVIOUS(ptr)), PACK(size, 0)); //new header pos, new size
		PUT(FOOTER(ptr), PACK(size, 0)); //same footer pos, new size
		ptr = PREVIOUS(ptr);
    } else {   /* If both prev/next are free */
		size += GET_SIZE(HEADER(PREVIOUS(ptr))) + GET_SIZE(FOOTER(NEXT(ptr))); //size is sum of all three (prev, current, next)
		PUT(HEADER(PREVIOUS(ptr)), PACK(size, 0)); //new header pos, new size
		PUT(FOOTER(NEXT(ptr)), PACK(size, 0)); //new footer pos, new size
		ptr = PREVIOUS(ptr);
    }

    //now put ptr in free list (expand if necessary)

    for(i = 0; i < freeArraySize; i++;){
        if (FREE_ARRAY[i] == NULL){//have found a slot
            FREE_ARRAY[i] = ptr;
            inserted = 1;
            break;
        }
    }

    if (!inserted){
        expandArray();
        for(i = 0; i < freeArraySize; i++;){
            if (FREE_ARRAY[i] == NULL){//have found a slot
                FREE_ARRAY[i] = ptr;
                break;
            }
        }

    }

}

/*
 * expandArray - expands the array holding the list of frees if it becomes too short
 *
 */
void expandArray(void) {
	size_t* oldArray = FREE_ARRAY;
	int newArray[freeArraySize*2];
	int i = 0;

	FREE_ARRAY = newArray;

	for (i=0; i<freeArraySize; i++) {
		FREE_ARRAY[i] = oldArray[i];
	}

	for (i=freeArraySize; i<(freeArraySize*2); i++) {
		FREE_ARRAY[i] = (size_t)NULL;
	}

	freeArraySize = freeArraySize*2;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	size_t oldSize;
	void* newptr;

	newptr = mm_malloc(size);

	if (!newptr && size != 0) //if newPtr is null, mm_malloc failed
		return NULL;

	if (ptr == NULL) //if ptr is null, no need to free
		return newptr;

	if (size != 0) { //if size == 0, simply free
		oldSize = GET_SIZE(HEADER(ptr));
		if (size < oldSize)
			oldSize = size;
		memcpy(newptr, ptr, oldSize);
	} else {
		newptr = 0;
	}

	mm_free(ptr);

	return newptr;
}


/*
 * enlarge - sbreaks more free space to be malloc'd
 */
static void *enlarge(size_t size)
{
    char *ptr;
    size_t adjustedSize;
	size_t prevBlock;
	size_t nextBlock;

	if ((size/WSIZE)%2) { //ensures we sbrk an even number of words (WSIZEs) to make sure heap is aligned by 8
		adjustedSize = ((size/WSIZE) + 1) * WSIZE;
	} else {
		adjustedSize = size;
	}


	ptr = mem_sbrk(adjustedSize);
	if ((long)ptr == -1)  //if mem_sbrk didn't work
		return NULL;

	//mark header/footer/epilogue header for new, gigantic free heap
    PUT(HEADER(ptr), PACK(adjustedSize, 0)); //free-block header
    PUT(FOOTER(ptr), PACK(adjustedSize, 0)); //free-block footer
    PUT(HEADER(NEXT(ptr)), PACK(0, 1)); //New epilogue header

    prevBlock = GET_ALLOC(FOOTER(PREVIOUS(ptr)));
    nextBlock = GET_ALLOC(HEADER(NEXT(ptr)));
    adjustedSize = GET_SIZE(HEADER(ptr));

    if (prevBlock && nextBlock) {  /* if both prev/next are allocated */
		//do nothing
    } else if (prevBlock && !nextBlock) { /* if only next block is free */
		adjustedSize += GET_SIZE(HEADER(NEXT(ptr)));
		PUT(HEADER(ptr), PACK(adjustedSize, 0));  //same header pos, new size
		PUT(FOOTER(ptr), PACK(adjustedSize,0)); //new footer pos, new size
    } else if (!prevBlock && nextBlock) { /* if only prev block is free */
		adjustedSize += GET_SIZE(HEADER(PREVIOUS(ptr)));
		PUT(HEADER(PREVIOUS(ptr)), PACK(adjustedSize, 0)); //new header pos, new size
		PUT(FOOTER(ptr), PACK(adjustedSize, 0)); //same footer pos, new size
		ptr = PREVIOUS(ptr);
    } else {   /* If both prev/next are free */
		adjustedSize += GET_SIZE(HEADER(PREVIOUS(ptr))) + GET_SIZE(FOOTER(NEXT(ptr))); //size is sum of all three (prev, current, next)
		PUT(HEADER(PREVIOUS(ptr)), PACK(adjustedSize, 0)); //new header pos, new size
		PUT(FOOTER(NEXT(ptr)), PACK(adjustedSize, 0)); //new footer pos, new size
		ptr = PREVIOUS(ptr);
    }

    return ptr;
}


/*
 * mm_check
 *an extended version of the mm_check suggested in the pdf
 */
int check_block(void *ptr){
    if ((size_t)ptr%8){
        printf("ERROR, %p is not aligned correctly\n", ptr);
        return 1;
    }
    if (GET(HEADER(ptr)) != GET(FOOTER(ptr))){
        printf("ERROR, %p has inconsistent header/footer\n", ptr);
        return 2;
    }
    return 0;
}

int mm_check(void){

    char *ptr;
    int cont = 1;
    int size;
    int found = 0;

    size_t* start_heap =  mem_heap_lo();
    size_t* end_heap =  mem_heap_hi();

    size_t* curr_block = start_heap;

    for(ptr = start_heap; GET_SIZE(HEADER(ptr)) > 0; ptr = NEXT(ptr)) {
        printf(check_block(ptr));
        if (ptr > end_heap || ptr < start_heap)
            printf("Error: pointer %p out of heap bounds\n", ptr);
        if (GET_ALLOC(ptr) == 0 && GET_ALLOC(NEXT(ptr))==0)
            printf("ERROR: contiguous free blocks %p and %p not coalesced\n", ptr, NEXT(ptr));
        // if(ARRAY_IMPLEMENTATION){//need to check if it is in the free array
        //
        //             for(int i = 0; i < freeArraySize; i++){
        //                 if(FREE_ARRAY[i] == ptr){
        //                     found = 1;
        //                     break;
        //                 }
        //             }
        //             if(!found)
        //                 printf("ERROR: pointer %p marked as free in heap but not found in free list", ptr);
        //         }
    }

    // if(ARRAY_IMPLEMENTATION){
    //     for(int i = 0; i < sizeof(FREE_ARRAY); i++){
    //         if( FREE_ARRAY[i] != NULL ){//will only examine true entries in the array
    //             if(GET_ALLOC(FREE_ARRAY[i] != 0))
    //                 printf("ERROR: entry %d in free list not marked as free\n", i);
    //             if(size = GET_SIZE(FREE_ARRAY[i]) == 0)
    //                 printf("ERROR: size is %d", i);
    //         }
    //     }
    // }

    return 0;
}
