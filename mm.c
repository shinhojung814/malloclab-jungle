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
​
#include "mm.h"
#include "memlib.h"
​
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "4조",
    /* First member's full name */
    "이채영",
    /* First member's email address */
    "stat_chae0@",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
​
/* single word (4) or double word (8) alignment */
/*
ALIGNMENT 상수를 정의하고 있다. 더블 워드 크기(8bytes)를 정의하고 있다.
이는 메모리 구조를 효율적으로 관리하기 위해서, 메모리 할당을 8의 배수로 할 수 있도록 한다.
*/
#define ALIGNMENT 8
​
/* rounds up to the nearest multiple of ALIGNMENT */
/*
ALIGN(size)는 주어진 size의 수에서 가장 가까운 ALIGNMENT 배수 값으로 반올림 해준다. 
즉, size의 수를 ALIGNMENT 배수로 바꿔 준다. 
하위 3bit와 일치하지 않는 부분만을 반환해서 ALIGNMENT 크기를 맞춘다.
*/
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
​
/*
주어진 size_t의 크기만큼 정렬(ALIGN)하고, 이 값을 SIZE_T_SIZE로 정의한다.
*/
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
​
//가용리스트 조직을 위한 기본 상수 및 매크로
#define WSIZE 4 //word와 헤더풋터사이즈
#define DSIZE 8 //더블워드 사이즈
#define CHUNKSIZE (1<<6) // 초기 힙 사이즈
​
#define MAX(x,y) ((x) > (y) ? (x) : (y))
​
#define PACK(size, alloc) ((size) | (alloc)) //size와 alloc(a)의 값을 한 word로 묶는다, 이를 이용하여 header와 footer에 쉽게 저장할 수 있다.
                                            //블록은 8사이즈 단위이기 떄문에 가능
​
#define GET(p) (*(unsigned int *)(p))  //포인터 p가 가리키는 곳에서 한 word값을 읽어온다
#define PUT(p, val) (*(unsigned int *)(p)=(val)) //포인터 p가 가리키는 곳의 한 word의 값에 val를 기록한다
​
#define GET_SIZE(p) (GET(p) & ~0x7) //포인터 p가 가리키는 곳에서 한 word를 읽은 다음 하위 3bit를 버린다. 즉 header에서 block사이즈를 읽는다
#define GET_ALLOC(p) (GET(p) & 0x1)   //포인터 p가 가리키는 곳에서 한 word를 읽은 다음 최하위 1bit를 읽는다. 0이면 블록이 할당되어 있지 않고, 1이면 할당되었다는 의미
​
#define HDRP(bp) ((char *)(bp) - WSIZE) //주어진 포인터 bp의 header의 주소를 계산한다
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //주어진 bp의 footer의 주소를 계산한다
​
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //주어진 포인터 bp를 이용해서 다음 block의 주소를 얻어온다
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //주어진 포인터 bp를 이용하서 이전의 block의 주소를 얻어온다
​
#define PRED_LOC(bp) ((char *)HDRP(bp) + WSIZE) //주소
#define SUCC_LOC(bp) ((char *)HDRP(bp) + DSIZE)
​
#define PUT_ADDRESS(p, adress) (*(char **)(p) = (adress)) //주소 넣기
​
#define PRED(bp) GET(PRED_LOC(bp)) //값
#define SUCC(bp) GET(SUCC_LOC(bp))
​
/* 
 * mm_init - initialize the malloc package.
 */
/*
초기 힙 영역을 할당하는 것과 같은 필요한 초기화를 수행해라. 
초기화를 수행하는데 문제가 있으면 1을 반환해야 한다. 다른 경우에는 0을 반환한다.
driver가 새로운 trace를 실행할 때마다, mm_init 함수를 호출함으로써 heap을 빈 heap으로 리셋시킨다.
*/
static char *heap_listp;
static char *next_bp;
​
//내가 빠지고 둘이 연결
static void change(void *bp){
    PUT_ADDRESS(SUCC_LOC(PRED(bp)), SUCC(bp));
    if(SUCC(bp) != NULL){
        PUT_ADDRESS(PRED_LOC(SUCC(bp)), PRED(bp));
    }
}
​
//루트에 연결
static void connect_root(void *bp){
    PUT_ADDRESS(PRED_LOC(bp), heap_listp);
    PUT_ADDRESS(SUCC_LOC(bp), SUCC(heap_listp));
​
    if((void *)SUCC(heap_listp) != NULL){ //(void*) 해줘야할까?
        PUT_ADDRESS(PRED_LOC(SUCC(heap_listp)), bp);
    }
     PUT_ADDRESS(SUCC_LOC(heap_listp), bp);
}
​
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 할당 여부 0 = no, 1 = yes
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //다음 블록의 할당여부
    size_t size = GET_SIZE(HDRP(bp)); //현재블럭의 사이즈
    
    if((PREV_BLKP(bp)< bp) && (bp <NEXT_BLKP(bp))){
        next_bp = (PREV_BLKP(bp));
    } //next fit을 위해 필요함
​
    //case 1 : 이전블럭, 다음 블럭 최하위 bit 둘다 1 할당
    //블럭 병합없이 bp return
    if (prev_alloc && next_alloc){
        connect_root(bp);
        return bp;
    }
​
    //case 2 : 이전 블럭 최하위 bit 1(할당), 다음 블럭 최하위 bit 0(비할당)
    //다음 블럭과 병합한 뒤 bp return
    else if (prev_alloc && !next_alloc){
        char *nxt_blkp = NEXT_BLKP(bp);
        connect_root(bp);//나 연결해주고
        change(nxt_blkp);//뒤에꺼 빼주기
​
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
    }
​
    //case 3 : 아전 블럭 최하위 bit0, 다음 블럭 최하위 bit 1
    //이전 블럭과 병합한 뒤 새로운 bp return 병합한 뒤 새로운 
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        change(bp);//앞에꺼 빼고
        connect_root(bp);//다시 넣기
    }
    
    //case 4 : 이전 블럭 최하위 bit 0, 다음 블럭 최하위 bit 0 
    //이전 블럭, 현재블럭, 다음블럭 모두 병합한 뒤 새로운 bp return
    else{
        char *old_next = NEXT_BLKP(bp);
​
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
​
        change(bp);//앞에 빼고
        change(old_next);//뒤에 빼고
        connect_root(bp);//새로 넣기
    }
    return bp;
}
​
//힙이 초기화 될 때, mm_malloc이 적당한 fit을 찾지 못했을 때
static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    //요청한 크기를 인접 2워드의 배수로 반올림하며,
    //그 후에 메모리 시스템으로부터 추가적인 힙 공간 요청
    size = (words % 2) ? (words + 1) * WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
​
    PUT(HDRP(bp), PACK(size, 0));//free block header
    PUT(FTRP(bp), PACK(size, 0));//free bolck footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));//new epilogue header
​
    return coalesce(bp);
}
​
int mm_init(void)
{   
    //메모리 시스템에서 4워드를 가져와서 빈 가용리스트를 만들 수 있도록 초기화
    //+succ+pred  = 6
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
​
    PUT(heap_listp, 0); 
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); // prologue header
    PUT_ADDRESS(heap_listp + (2 * WSIZE), NULL); //succ
    PUT_ADDRESS(heap_listp + (3 * WSIZE), NULL); //pred
    PUT(heap_listp + (4 * WSIZE), PACK(2 *DSIZE, 1)); // prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1)); //epilogue header
    heap_listp += (2*WSIZE);
​
    //힙을 chunksize바이트로 확장하고, 초기 가용블록을 생성
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
​
    next_bp = heap_listp; //next fit을 위한 것
​
    PUT_ADDRESS(SUCC_LOC(heap_listp), NEXT_BLKP(heap_listp));
​
    return 0;
}
​
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
​
    //배치 후에 이 블록의 나머지가 최소블록 크기와 같거나 크다면,
    //진행해서 블록을 분할해야한다.
​
    if((csize - asize) >= (2 * DSIZE)){
        //새롭게 할당 된 블록을 배치
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
​
        //자기꺼를 뒤에 분리된거에 옮겨줌
        PUT_ADDRESS(SUCC_LOC(NEXT_BLKP(bp)), SUCC(bp));
        PUT_ADDRESS(PRED_LOC(NEXT_BLKP(bp)), PRED(bp));
        PUT_ADDRESS(SUCC_LOC((PRED(bp))), NEXT_BLKP(bp));
        if (SUCC(bp) != NULL)
            PUT_ADDRESS(PRED_LOC(SUCC(bp)), NEXT_BLKP(bp));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else{//같은 경우에는 전체를 할당
        change(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
​
static char *first_fit(size_t asize){
    char *bp;
    
    //1. first fit
    for (bp = heap_listp ; GET_SIZE(HDRP(bp))>0 ; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;
    }
​
    return NULL; //맞는게 없음
}
​
static char *next_fit(size_t asize){
    char *bp = next_bp;
   
    while(1){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){//할당가능
            next_bp = bp;
            return next_bp;
        }
        bp = NEXT_BLKP(bp);
        if(GET_SIZE(HDRP(bp)) == 0){//에필로그 만나면 처음으로
            bp = heap_listp;
        }
        if(bp == next_bp){//존재 안함
            return NULL;
        }
    }
    
    return NULL;
}
​
//best+first
static char *bestfirst_fit(size_t asize){
    char *bp = next_bp;
    char *best;
    size_t best_size = NULL;
    int i = 0;
​
    for (; GET_SIZE(HDRP(bp))>0 ; bp = NEXT_BLKP(bp)){
        size_t new_size = GET_SIZE(HDRP(bp));
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            if (GET_SIZE(HDRP(bp))*0.9 <= asize){
                next_bp = bp;
                return bp;
            }
            if (best_size == NULL){
                best_size = new_size;
                best = bp;
            }
            else if (best_size > new_size){
                best_size = new_size;
                best = bp;
                i++;
                if (i>3){
                    next_bp = best;
                    return best;
                }
            }
        }
    }
    if (best_size != NULL){
        next_bp = best;
        return best;
    }
​
    for (bp = heap_listp ; bp != next_bp ; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            next_bp = bp;
            return bp;
        }
    }
​
    return NULL;
}
static char *best_fit(size_t asize){
    char *bp;
    char *best;
    int i = 0;
    size_t best_size = NULL;
    for (bp = heap_listp ; GET_SIZE(HDRP(bp))>0 ; bp = NEXT_BLKP(bp)){
        size_t new_size = GET_SIZE(HDRP(bp));
        if(!GET_ALLOC(HDRP(bp)) && (asize <= new_size)){
            if (best_size == NULL){
                best_size = new_size;
                best = bp;
            }
            else if (best_size > new_size){
                best_size = new_size;
                best = bp;
                i++;
                if(i>3) return best;
            }
        }
    }
    if (best_size == NULL){
        return NULL;
    }else    return best;
}
​
static char *worst_fit(size_t asize){
    char *bp;
    char *worst;
    size_t worst_size = NULL;
    for (bp = heap_listp ; GET_SIZE(HDRP(bp))>0 ; bp = NEXT_BLKP(bp)){
        size_t new_size = GET_SIZE(HDRP(bp));
        if(!GET_ALLOC(HDRP(bp)) && (asize <= new_size)){
            if (worst_size == NULL){
                worst_size = new_size;
                worst = bp;
            }
            else if (worst_size > new_size){
                worst_size = new_size;
                worst = bp;
            }
        }
    }
    if (worst_size == NULL){
        return NULL;
    }else    return worst;
}
​
static char *explicit_fit(size_t asize){
    char *bp = heap_listp;
    while (1)
    {
        bp = SUCC(bp);
        if (bp == NULL)
            break;
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp)))
        {
            return bp;
        }
    }
    return NULL;
}
​
//explicit+best
static char *exbest_fit(size_t asize){
    char *bp = heap_listp;
    char *best;
    size_t best_size = NULL;
​
    for (bp = SUCC(bp); bp!=NULL ; bp = SUCC(bp)){
        size_t new_size = GET_SIZE(HDRP(bp));
        if(!GET_ALLOC(HDRP(bp)) && (asize <= new_size)){
            if (best_size == NULL){
                best_size = new_size;
                best = bp;
            }
            else if (best_size > new_size){
                best_size = new_size;
                best = bp;
            }
        }
    }
    if (best_size == NULL){
        return NULL;
    }else    return best;
    return NULL;
}
​
​
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
/*
malloc 함수는 최소의 데이터 크기가 할당된 블록의 포인터를 반환한다. 
할당된 블록 전체는 heap 영역에 있어야 하며, 할당된 다른 공간과 overlap 되어서는 안 된다.
표준 c 라이브러리 malloc 은 항상 8바이트로 정렬된 payload 포인터를 반환하므로, 
마찬가지로 구현된 malloc도 항상 8바이트로 정렬된 포인터를 반환해야 한다.
*/
void *mm_malloc(size_t size){
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }
    size_t asize;
    size_t extendsize;
    char *bp;
​
    if (size == 0)
        return NULL;
    
    //최소 16바이트 크기의 블록 구성
    //8바이트는 정렬요건 만족시키기 위해
    //추가적인 8바이트는 헤더와 풋터 오버헤드를 위해
    if(size <= DSIZE)
        asize = 2 * DSIZE;
    else//8바이트 넘는 요청 : 오버헤드 바이트 내에 더해주고 인접 8의 배수로 반올림
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
​
    //적절한 가용블록을 가용리스트에서 검색
    if((bp = exbest_fit(asize)) != NULL){
        place(bp, asize);
        //맞는 블록 찾으면 할당기는 요청한 블록 배치하고, 옵셥으로 초과부분을 분할
        return bp;
    }
    // if((bp = first_fit(asize)) != NULL){
    //     place(bp, asize);
    //     return bp;
    // }
    // if((bp = next_fit(asize)) != NULL){
    //     place(bp, asize);
    //     return bp;
    // }
    // if((bp = best_fit(asize)) != NULL){
    //     place(bp, asize);
    //     return bp;
    // }
    // if((bp = worst_fit(asize)) != NULL){
    //     place(bp, asize);
    //     return bp;
    // }
    // if((bp = bestfirst_fit(asize)) != NULL){
    //     place(bp, asize);
    //     return bp;
    // }
    // if((bp = explicit_fit(asize)) != NULL){
    //     place(bp, asize);
    //     //맞는 블록 찾으면 할당기는 요청한 블록 배치하고, 옵셥으로 초과부분을 분할
    //     return bp;
    // }
    
    // 힙을 새로운 가용부분으로 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    // next_bp = bp;
    return bp;
}
​
/*
 * mm_free - Freeing a block does nothing.
 */
/*
free 함수는 ptr이 가리키는 블록을 가용공간으로 만든다. 
이 함수는 아무것도 반환하지 않는다. 
malloc, calloc, realloc 함수에 의해 반환되어진 포인터이면서 
free에 의해 호출되어지지 않은 인자를 넘겨받아야만 free함수는 함수 기능을 실행한다.
free(NULL)은 아무런 기능도 하지 않는다.
*/
void mm_free(void *ptr){
​
    if(!ptr) return;
​
    size_t size = GET_SIZE(HDRP(ptr)); // ptr헤더에서 block사이즈를 읽어옴
​
    //실제로 데이터를 지우는 것이 아니라 헤더와 풋터의 최하위 1(할당된상태)만을 수정
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    // 주위에 빈 블럭이 있을 시 병합
    coalesce(ptr);
}
​
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
/*
realloc 함수는 다음의 제약들을 가지는 최소 바이트로 할당된 공간의 포인터를 반환한다.
	- if ptr is NULL, malloc(size)와 동일하다.
	- if size is equal to zero, free(ptr)과 동일하며 NULL을 반환해야 한다.
	- if ptr is not NULL, 이것은 malloc 또는 realloc에 의해 반환되어진 값이며,
     아직 free함수가 호출된 경우가 아니다. realloc 함수의 호출은 ptr에 의해 가리켜지는 	
     메모리 블록의 사이즈를 size 바이트로 바꾸고, 새로운 블록의 주소를 반환한다. 
     구현하는 방법, old block의 내부 단편화의 양, 요구되는 크기에 따라, 
     새로운 블록의 주소공간은 old block 과 같을 수도 있고, 다를 수도 있다. 
     new block의 내용은 old size와 new size 중 작은 크기 까지는 old block의 내용과 같다. 
     다른 것들은 초기화 되어 있지 않다.
*/
void *mm_realloc(void *ptr, size_t size){
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
