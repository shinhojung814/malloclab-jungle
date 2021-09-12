#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 가용 리스트 조작을 위한 기본 상수와 매크로 */

/* 워드와 헤더/풋터의 크기 */
#define WSIZE 4
/* 더블 워드의 크기 */
#define DSIZE 8
/* 초기 가용 블록과 힙 확장을 위한 기본 크기 */
#define CHUNKSIZE (1 << 12)

/* x가 y보다 크면 x, 그렇지 않으면 y */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* 크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값을 리턴 */
#define PACK(size, alloc) ((size) | (alloc))

/* 인자 p가 참조하는 워드를 읽어서 리턴 */
#define GET(p) (*(unsigned int *)(p))
/* 인자 p가 가리키는 워드에 val을 저장 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* 주소 p에 있는 헤더 또는 풋터의 크기를 리턴 */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* 주소 p에 있는 할당 비트를 리턴 */
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 포인터 bp가 주어지면 블록 헤더를 가리키는 포인터를 리턴 */
#define HDRP(bp) ((char *)(bp)-WSIZE)
/* 블록 포인터 bp가 주어지면 블록 풋터를 가리키는 포인터를 리턴 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp) - DSIZE))

/* 다음 블록의 블록 포인터를 리턴 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
/* 이전 블록의 블록 포인터를 리턴 */
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

int mm_init(void);

void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static char *heap_listp;

/* 최초 가용 블록으로 힙 생성 */
/* mm_init 함수는 할당기를 초기화하고 성공이면 0을 아니면 -1을 리턴 */
/* 메모리 시스템에서 4워드를 가져와서 빈 가용 리스트를 만들 수 있도록 초기화 */

int mm_init(void)
{
    /* 힙 초기화 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    /* 더블 워드 경계로 정렬된 미사용 패딩 워드 */
    PUT(heap_listp, 0);
    /* 프롤로그 블록의 헤더 */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    /* 프롤로그 블록의 풋터 */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    /* 에필로그 블록의 헤더 */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    /* 힙을 CHUNKSIZE 바이트로 확장 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 가용 리스트에서 블록 할당 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    /* 최소 16바이트 크기의 블록을 구성 (8바이트는 정렬 요건, 추가적인 8바이트는 헤더와 풋터 오버헤드를 위해) */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    /* 8바이트를 넘으면 오버헤드 바이트를 추가하고, 인접 8의 배수로 반올림 */
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* 요청한 크기를 조정한 후에 적절한 가용 블록을 가용 리스트에서 검색 */
    /* 맞는 블록을 찾으면 할당기는 요청한 블록을 배치하고 할당한 블록을 리턴 */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* 할당기가 맞는 블록을 찾지 못하면 힙을 새로운 가용 블록으로 확장 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    /* 요청한 블록을 새 가용 블록에 배치하고 할당한 블록의 포인터를 리턴 */
    place(bp, asize);
    return bp;
}

/* 블록을 반환하고 경계 태그 연결을 사용해서 상수 시간에 인접 가용 블록들과 통합 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
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

/* 힙을 CHUNKSIZE 바이트로 확장하고 초기 가용 블록을 생성 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 요청한 크기를 인접 2워드의 배수(8바이트)로 올림 후 메모리 시스템으로부터 추가적인 힙 공간 요청 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* 이전 힙이 가용 블록으로 끝났다면, 두 개의 가용 블록을 통합한 후 블록의 블록 포인터를 리턴 */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* 이전과 다음 블록이 모두 할당되어 있는 경우 */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    /* 이전 블록은 할당 상태, 다음 블록은 가용 상태인 경우 */
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* 이전 블록은 가용 상태, 다음 블록은 할당 상태인 경우 */
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* 이전 블록과 다음 블록 모두 가용 상태인 경우 */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }

    /* No fit */
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
