#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

/* Word and header/footer size (bytes) */
/* 워드와 헤더/풋터의 크기 */
#define WSIZE 4
/* Double word size (bytes) */
/* 더블 워드의 크기 */
#define DSIZE 8
/* Extend heap by this amount (bytes) */
/* 초기 가용 블록과 힙 확장을 위한 기본 크기 */
#define CHUNKSIZE (1 << 12)

/* 아하! */
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

/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;
