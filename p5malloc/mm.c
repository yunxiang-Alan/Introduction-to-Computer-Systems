#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

#ifdef DRIVER
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

#define WSIZE       4
#define DSIZE       8
#define OVERHEAD    8
#define ALIGNMENT   8
#define NUMLIST 	15
#define CHUNKSIZE   (1<<8)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define ALIGN(size) (((size_t)(size) + (ALIGNMENT - 1)) & ~0x7)

#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)       		(*(unsigned int *)(p))
#define PUT(p, val)  		(*(unsigned int *)(p) = (val))
#define GET_SIZE(p)			(GET(p) & ~0x7)
#define GET_ALLOC(p)		(GET(p) & 0x1)

#define HDPT(bp)			((char *)(bp) - WSIZE)
#define FTPT(bp)       		((char *)(bp) + GET_SIZE(HDPT(bp)) - DSIZE)
#define NEXT_BLKP(bp)  		((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  		((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define SIZE(bp)      		(GET_SIZE(HDPT(bp)))
#define PREV_SIZE(bp) 		(GET_SIZE((char *)(bp) - DSIZE)) //size of previous block
#define NEXT_SIZE(bp) 		(GET_SIZE((char *)(bp) + SIZE(bp) - WSIZE))
#define ALLOC(bp)     		(GET_ALLOC(HDPT(bp)))
#define PREV(bp)      		((char *)(bp) - GET(bp))
#define NEXT(bp)      		((char *)(bp) + GET((char *)(bp) + WSIZE))
#define PUT_PREV(bp, pre)	PUT(bp, (unsigned int)((char *)(bp) - (char *)(pre)))
#define PUT_NEXT(bp, suc)	PUT((char *)(bp) + WSIZE, (unsigned int)((char *)(suc) - (char *)(bp)))

// global variables
static char *heap_listp = NULL;  
static size_t *free_head;  /* point to every list head */
static size_t *free_tail;  /* point to every list tail */

// helper functions
static void *extend_heap(size_t asize);
static void *place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static int  get_index(size_t size);
static void *insert(void *bp);
static void delete(void *bp);

int mm_init(void)
{
    if ((heap_listp = mem_sbrk((4 * (NUMLIST + 1)) * WSIZE)) == NULL) return -1;
    free_head = (size_t *)heap_listp;
    free_tail = (size_t *)(heap_listp + (2 * NUMLIST * WSIZE));
    int i;

    for (i = 0; i < NUMLIST; i++){
        free_head[i] = (size_t)NULL;
        free_tail[i] = (size_t)NULL;
    }
    heap_listp += (4 * NUMLIST) * WSIZE;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));    /* Prologue header */ 
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));    /* Prologue footer */ 
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));        /* Epilogue header */  
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE) == NULL) return -1;

    return 0;
}

void *mm_malloc(size_t size) 
{
    size_t asize, extendsize;      /* Adjusted block size */
    char *bp;      
    /* Adjust block size to include overhead and alignment reqs. */
    if(size <= DSIZE) {
    	asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + OVERHEAD + (DSIZE-1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize);
        return bp;
    }

    // If there is no fit free block, extend the heap and place the block.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize)) == NULL) {
            return NULL;
    }
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    if(bp == NULL) return;
    size_t size = SIZE(bp);
    PUT(HDPT(bp), PACK(size, 0));
    PUT(FTPT(bp), PACK(size, 0));
    bp = insert(bp);
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    size_t old_size;
    size_t asize;
    void *newptr;

    if(size == 0){
        mm_free(ptr);
        return NULL;
    }
    if(ptr == NULL)
        return mm_malloc(size);
    old_size = SIZE(ptr);
    asize = ALIGN(size + DSIZE);
    if(old_size >= asize){
        PUT(HDPT(ptr), PACK(old_size, 1));
        PUT(FTPT(ptr), PACK(old_size, 1));
        return ptr;
    } else {
        newptr = mm_malloc(size);
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
    }
}

void *calloc(size_t nmemb, size_t size){
        void *newptr;

        newptr = malloc(nmemb * size);
        memset(newptr, 0, nmemb * size);

        return newptr;
}

static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTPT(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDPT(NEXT_BLKP(bp)));
    size_t size = SIZE(bp);

    if (prev_alloc && next_alloc) {            //if prev and next blocks are both allocated
        return bp;
    } else if (prev_alloc && !next_alloc) {    //if prev is allocated but next is free
        size += SIZE(NEXT_BLKP(bp));
        delete(NEXT_BLKP(bp));
        delete(bp);
        PUT(HDPT(bp), PACK(size, 0));
        PUT(FTPT(bp), PACK(size,0));
        insert(bp);
    } else if (!prev_alloc && next_alloc) {    //if prev is free but next is allocated
        size += SIZE(PREV_BLKP(bp));
        delete(PREV_BLKP(bp));
        delete(bp);
        PUT(FTPT(bp), PACK(size, 0));
        PUT(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert(bp);
    } else {                                     //if both are free
        size += GET_SIZE(HDPT(PREV_BLKP(bp))) + GET_SIZE(FTPT(NEXT_BLKP(bp)));
        delete(bp);
        delete(NEXT_BLKP(bp));
        delete(PREV_BLKP(bp));
        PUT(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTPT(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert(bp);
    }

    return bp;
}

static void* place(void *bp, size_t asize)
{
    size_t csize = SIZE(bp);
    
    delete(bp);
    if ((csize - asize) >= 2 * DSIZE) {
        PUT(HDPT(bp), PACK(asize, 1));
        PUT(FTPT(bp), PACK(asize, 1));
        void* next_free_p = NEXT_BLKP(bp);
        PUT(HDPT(next_free_p), PACK(csize - asize, 0));
        PUT(FTPT(next_free_p), PACK(csize - asize, 0));
        insert(next_free_p);
    } else { 
        PUT(HDPT(bp), PACK(csize, 1));
        PUT(FTPT(bp), PACK(csize, 1));
    }

    return bp;
}

static int get_index(size_t size)
{
    int index = 0;
    int bound = (1<<4);

    while ((index < NUMLIST - 1)){
    	if (size < bound)
    		return index;
    	bound <<= 1;
    	index++;
    }

    return NUMLIST-1;
}

// first-fist policy
static void* find_fit(size_t asize)
{
    void *curr;
    int index;

    for (index = get_index(asize); index < NUMLIST; index++){
        if ((void*)free_head[index] == NULL)
            continue;
        for (curr = (char*)free_head[index]; curr != (void*)free_tail[index]; curr = NEXT(curr)){
            if (SIZE(curr) >= asize)
                return curr;
        }
        if (SIZE(curr) >= asize)
            return curr;
    }
    return NULL;
}

static void* insert(void* bp)
{
    int index = get_index(SIZE(bp));
    char *curr = (char *)free_head[index];

    if ((void*)free_head[index] == NULL){
        free_head[index] = (size_t)bp;
        free_tail[index] = (size_t)bp;
    } else{
        if (free_head[index] > (size_t)bp){
            PUT_NEXT(bp, free_head[index]);
            PUT_PREV(free_head[index], bp);
            free_head[index] = (size_t)bp;
        } else if (free_tail[index] < (size_t)bp){
            PUT_NEXT(free_tail[index],bp);
            PUT_PREV(bp,free_tail[index]);
            free_tail[index] = (size_t)bp;
        } else{
            while (NEXT(curr) < (char*)bp){
                curr = NEXT(curr);
            }
            char* nextBp = NEXT(curr);
            PUT_PREV(nextBp,bp);
            PUT_NEXT(curr,bp);
            PUT_NEXT(bp,nextBp);
            PUT_PREV(bp,curr);
        }
    }
    return bp;
}

static void delete(void *bp)
{
    int index = get_index(SIZE(bp));

    if (free_head[index] == free_tail[index])
    	free_head[index] = free_tail[index] = (size_t)NULL;
    else if (free_head[index] == (size_t)(bp))
    	free_head[index] = (size_t)NEXT(bp);
    else if (free_tail[index] == (size_t)(bp))
    	free_tail[index] = (size_t)PREV(bp);
    else {
    	PUT_PREV(NEXT(bp), PREV(bp));
    	PUT_NEXT(PREV(bp), NEXT(bp));
    }
}

static void *extend_heap(size_t words) 
{
    char *bp;

    /* Allocate an even number of words to maintain alignment */
    words = (words % 2) ? (words+1): words; 
    if ((long)(bp = mem_sbrk(words)) == -1)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDPT(bp), PACK(words, 0));         /* Free block header */   
    PUT(FTPT(bp), PACK(words, 0));         /* Free block footer */   
    PUT(HDPT(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* Coalesce if the previous block was free */
    return insert(bp);
}

// below are potentially useful debugging functions
/* static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
} */

void mm_checkheap(int verbose)
{
    /* char* curr;
    int i=1;
    printf("free list:\n");
    for (; i<NUMLIST; i++)
    {
        printf("size %d\n",i);
        if (free_head[i] == (size_t)NULL){
            printf("NULL\n");
            continue;
        } 
        for (curr = (char*)free_head[i]; curr != (char*)free_tail[i]; curr = NEXT(curr)){
            printf("SIZE :%u , ALLOC: %d\n", SIZE(curr), ALLOC(curr));
        }
        printf("SIZE :%u , ALLOC: %d\n\n", SIZE(curr), ALLOC(curr));
    }
    printf("\nheap block:\n");
    for (curr = heap_listp; curr != mem_heap_hi(); curr = NEXT_BLKP(curr)){
        printf("SIZE :%u , ALLOC: %d\n\n", SIZE(curr), ALLOC(curr));
    } */
}