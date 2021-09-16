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
    "14",
    /* First member's full name */
    "Shinho Jung",
    /* First member's email address */
    "shinhojung814@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
    };

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Word and header/footer size (bytes) */
#define WSIZE 4
/* Double word size (bytes) */
#define DSIZE 8
/* Extend heap by this amount (bytes) */
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr pb, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* PRED와 SUCC이 저장된 워드 */
#define PRED_LOC(bp) ((char *)HDRP(bp) + WSIZE)
#define SUCC_LOC(bp) ((char *)HDRP(bp) + DSIZE)

#define PRED(bp) GET(PRED_LOC(bp))
#define SUCC(bp) GET(SUCC_LOC(bp))

#define PUT_ADDRESS(p, address) (*(char **)(p) = (address))

int mm_init(void);

void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void change(void *bp);
static void connect_root(void *bp);
static size_t make_size(size_t size);

static char *heap_listp;

/* mm_init - initialize the malloc package. */
int mm_init(void) {
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) - 1)
        return -1;

    /* Alignment padding */
    PUT(heap_listp, 0);
    /* Prologue header */
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1));

    PUT_ADDRESS(heap_listp + (2 * WSIZE), NULL);
    PUT_ADDRESS(heap_listp + (3 * WSIZE), NULL);

    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1));

    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    
    PUT_ADDRESS(SUCC_LOC(heap_listp), NEXT_BLKP(heap_listp));

    return 0;
}

/* mm_malloc - Allocate a block by incrementing the brk pointer.
Always allocate a block whose size is a multiple of the alignment. */

void *mm_malloc(size_t size) {
    char *bp;
    size_t asize;
    size_t extendsize;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);

    return bp;
}

/* mm_free - Freeing a block does nothing. */
void mm_free(void *bp) {
    size_t size;

    size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);

    if (newptr == NULL)
        return NULL;

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
        copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    /* Free block header */
    PUT(HDRP(bp), PACK(size, 0));
    /* Free block footer */
    PUT(FTRP(bp), PACK(size, 0));
    /* New epilogue header */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1 */
    if (prev_alloc && next_alloc) {
        connect_root(bp);
        return bp;
    }

    /* Case 2 */
    else if (prev_alloc && !next_alloc) {
        connect_root(bp);
        change(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* Case 3 */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        bp = PREV_BLKP(bp);

        change(bp);
        connect_root(bp);
    }

    /* Case 4 */
    else {
        char *next = NEXT_BLKP(bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        bp = PREV_BLKP(bp);

        change(bp);
        change(next);
        connect_root(bp);
    }

    return bp;
}

static void *find_fit(size_t asize) {
    char *bp = heap_listp;
    char *best_bp;
    size_t best_size = NULL;

    for (bp = SUCC(bp); bp != NULL; bp = SUCC(bp)) {

        size_t new_size = GET_SIZE(HDRP(bp));

        if(!GET_ALLOC(HDRP(bp)) && (asize <= new_size)) {
            if (best_size == NULL) {
                best_size = new_size;
                best_bp = bp;
            }
            
            else if (best_size > new_size) {
                best_size = new_size;
                best_bp = bp;
            }
        }
    }
    if (best_size == NULL) {
        return NULL;

    } else
        return best_bp;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        coalesce(bp);

        PUT_ADDRESS(PRED_LOC(NEXT_BLKP(bp)), PRED(bp));
        PUT_ADDRESS(SUCC_LOC(NEXT_BLKP(bp)), SUCC(bp));

        PUT_ADDRESS(SUCC_LOC((PRED(bp))), NEXT_BLKP(bp));

        if (SUCC(bp) != NULL)
            PUT_ADDRESS(PRED_LOC(SUCC(bp)), NEXT_BLKP(bp));
        
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    
    else {
        change(bp);

        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void change(void *bp) {
    PUT_ADDRESS(SUCC_LOC(PRED(bp)), SUCC(bp));

    if (SUCC(bp) != NULL)
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), PRED(bp));
}

static void connect_root(void *bp) {
    PUT_ADDRESS(PRED_LOC(bp), heap_listp);
    PUT_ADDRESS(SUCC_LOC(bp), SUCC(heap_listp));

    if ((void *)SUCC(heap_listp) != NULL) {
        PUT_ADDRESS(PRED_LOC(SUCC(heap_listp)), bp);
    }

    PUT_ADDRESS(SUCC_LOC(heap_listp), bp);
}

static size_t make_size(size_t size) {
    size_t asize;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    return asize;
}
