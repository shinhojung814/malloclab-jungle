/* Basic constants and macros */

/* Word and header/footer size (bytes) */
/* 워드와 헤더/풋터의 크기 */
#define WSIZE 4
/* Double word size (bytes) */
/* 더블 워드의 크기 */
#define DSIZE 8
/* Extend heap by this amount (bytes) */
/* 초기 가용 블록과 힙 확장을 위한 기본 크기 */
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
/* 크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값을 리턴 */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
/* 인자 p가 참조하는 워드를 읽어서 리턴 */
#define GET(p) (*(unsigned int *)(p))
/* 인자 p가 가리키는 워드에 val을 저장 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
/* 주소 p에 있는 헤더 또는 풋터의 크기를 리턴 */
#define GET_SIZE(p) (GET(p) & ~0x7)
/* 주소 p에 있는 할당 비트를 리턴 */
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
/* 블록 포인터 bp가 주어지면 블록 헤더를 가리키는 포인터를 리턴 */
#define HDRP(pb) ((char *)(bp)-WSIZE)
/* 블록 포인터 bp가 주어지면 블록 풋터를 가리키는 포인터를 리턴 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp) - DSIZE))

/* Given block ptr pb, compute address of next and previous blocks */
/* 다음 블록의 블록 포인터를 리턴 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
/* 이전 블록의 블록 포인터를 리턴 */
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* 현재 블록의 포인터 bp가 주어지면 메모리 내 다음 블록의 크기를 결정 */
// size_t size = GET_SIZE(HDRP(NEXT_BLKP(bp)));

/* 최초 가용 블록으로 힙 생성 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
        
    /* Alignment padding */
    PUT(heap_listp, 0);
    /* Prologue header */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    /* Prologue footer */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    /* Epilogue header */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 새 가용 블록으로 힙 확장 */
static void *extend_heap(size_t words)
{
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
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1))

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 블록을 반환하고 경계 태그 연결을 사용해서 상수 시간에 인접 가용 블록들과 통합 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalsece(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1 */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    /* Case 2 */
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* Case 3 */
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* Case 4 */
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

/* 가용 리스트에서 블록 할당 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
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
