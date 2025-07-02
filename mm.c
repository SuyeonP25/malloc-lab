/*
 * 정글 9기 9번 박수연
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    " ", " ", " ", "", ""};

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define UINT_MAX 0xffffffff      /* uint 최댓값 (best-fit에 사용) */
#define MIN_BLK_SIZE (2 * DSIZE) /* 블록 사이즈 최솟값 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *first_bp = NULL; /* 첫번째 블록 포인터 */
static char *nf_bp = NULL;    /* next-fit 탐색 재개 블록 포인터 */

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);

/* 메모리 초기화 함수
 * 성공 시 0 반환  : first_blk는 첫 번째 블록의 포인터
 * 실패 시 -1 반환 : mem_sbrk 실패 시 first_bp = (void*) -1
 */
int mm_init(void)
{
    first_bp = mem_sbrk(4 * WSIZE);

    if (first_bp == (void *)-1)
        return -1;

    PUT(first_bp, 0);
    PUT(first_bp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(first_bp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(first_bp + (3 * WSIZE), PACK(0, 1));
    first_bp += (2 * WSIZE);

    nf_bp = first_bp;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/* 메모리 할당 함수
 * 성공 시 할당된 블록의 포인터 반환
 * 실패 시 NULL 반환 : size = 0 또는 extend_heap 실패
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    /* 초기화 필요 */
    if (first_bp == NULL)
        mm_init();

    size_t asize;

    if (size <= DSIZE)
        asize = MIN_BLK_SIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    char *bp;
    /*---------- 가용 블록 검색 함수 변경 시 이곳을 수정 ---------*/
    bp = next_fit(asize);
    /*---------- 가용 블록 검색 함수 변경 시 이곳을 수정 ---------*/

    /* 할당 가능 블록 있음 */
    if (bp != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* 할당 가능 블록 없음 - 사이즈 증가 필요 */
    size_t extendsize;
    extendsize = MAX(asize, CHUNKSIZE);

    bp = extend_heap(extendsize / WSIZE);

    if (bp == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/* 메모리 반환 함수
 * 인자 포인터에 해당하는 블록을 미할당으로 표시
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;

    /* 초기화 필요 */
    if (first_bp == NULL)
        mm_init();

    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    /* 블록 연결이 필요한지 확인 */
    coalesce(ptr);
}

/* 블록 연결 함수
 * 직전, 직후 블록과 연결 후 결과 블록의 포인터 반환
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* 1. 직전, 직후 블록이 모두 할당됨 (Case 1) */
    if (prev_alloc && next_alloc)
        return bp;
    /* 2. 직전 블록만 할당됨 (Case 2) */
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* 3. 직후 블록만 할당됨 (Case 3) */
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* 4. 앞 뒤 블록이 모두 미할당됨 (Case 4) */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* 블록 연결 후 next-fit 탐색 재개 블럭 포인터 수정 */
    if ((nf_bp > (char *)bp) && (nf_bp < NEXT_BLKP(bp)))
        nf_bp = bp;

    return bp;
}

/* 재할당 함수
 * 성공 시 재할당 된 블록 포인터 반환
 * 실패 시 NULL 반환 (mm_malloc 실패)
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        mm_free(ptr);
        return 0;
    }

    if (ptr == NULL)
        return mm_malloc(size);

    void *next_bp, *bp;
    size_t bsize, asize; /* bsize : 기존 블록 크기 asize : 요구 사이즈  */
    bsize = GET_SIZE(HDRP(ptr));

    if (size <= DSIZE)
        asize = MIN_BLK_SIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Case 1 : 재할당 크기가 원래 크기보다 작음 - 기존 블록 사이즈 수정 */
    if (asize <= bsize)
    {
        size_t dsize;
        dsize = bsize - asize;

        /* Case 1 - 1 : 크기 차가 블록 최소 사이즈보다 크거나 같음 */
        if (dsize >= MIN_BLK_SIZE)
        {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            next_bp = NEXT_BLKP(ptr);
            PUT(HDRP(next_bp), PACK(dsize, 0));
            PUT(FTRP(next_bp), PACK(dsize, 0));
            coalesce(next_bp);
        }

        return ptr;
    }

    /* Case 2 : 재할당 크기가 원래 크기보다 큼 */
    next_bp = NEXT_BLKP(ptr);

    if (!GET_ALLOC(HDRP(next_bp)))
    {
        size_t ssize;
        ssize = bsize + GET_SIZE(HDRP(next_bp));

        /* Case 2 - 1 : 다음 블록이 미할당이고 크기 합이 요구 사이즈보다 크거나 같음 - 기존 블록 사이즈 수정 */
        if (ssize >= asize)
        {
            PUT(HDRP(ptr), PACK(ssize, 1));
            PUT(FTRP(ptr), PACK(ssize, 1));
            return ptr;
        }
    }

    /* Case 2 - 2 : 기존 블록에서 재할당 불가 - 새 블록 할당 및 메모리 복사 */
    void *newptr;
    newptr = mm_malloc(size);

    /* mm_malloc 실패 */
    if (newptr == NULL)
        return NULL;

    memcpy(newptr, ptr, size);
    mm_free(ptr);

    return newptr;
}

/* 힙 확장 함수
 * 성공 시 확장된 메모리를 포함하는 블록 포인터 반환
 * 실패 시 NULL 반환 (mem_sbrk 실패)
 */
static void *extend_heap(size_t words)
{
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    char *bp;
    bp = mem_sbrk(size);

    if (bp == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* 블록 연결이 필요한지 확인 */
    bp = coalesce(bp);
    return bp;
}

/* 블록 배치 함수
 * 미할당 블록 포인터 위치에 할당된 블록 배치
 * 남은 블록의 사이즈가 블록 최소 사이즈(2 * DSIZE) 보다 큰 경우 분할
 */
static void place(void *bp, size_t asize)
{
    size_t bsize, dsize;
    bsize = GET_SIZE(HDRP(bp));
    dsize = bsize - asize;

    /* 분할 필요 */
    if (dsize >= MIN_BLK_SIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        /* 분할된 블록 */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(dsize, 0));
        PUT(FTRP(bp), PACK(dsize, 0));
        coalesce(bp);
    }
    /* 분할 불필요 */
    else
    {
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
    }
}

/* 가용 블록 검색 함수
 * 사이즈를 만족하는 가용블록이 있을 시 해당 블록의 포인터 반환, 없을 시 NULL 반환
 */

/* 가용 블록 검색 함수 : first-fit
 */
static void *first_fit(size_t asize)
{
    void *bp;

    /* 첫 블록부터 탐색 시작 - 블록의 끝까지 */
    for (bp = first_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        /* 미할당 & 필요 사이즈 만족 */
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    /* 사이즈를 만족하는 가용블록 없음 */
    return NULL;
}

/* 가용 블록 검색 함수 : next-fit
 */
static void *next_fit(size_t asize)
{
    char *old_nf_bp = nf_bp;

    /* nf_bp부터 탐색 시작 - 블록의 끝까지 */
    for (; GET_SIZE(HDRP(nf_bp)) > 0; nf_bp = NEXT_BLKP(nf_bp))
        if (!GET_ALLOC(HDRP(nf_bp)) && (asize <= GET_SIZE(HDRP(nf_bp))))
            return nf_bp;

    /* 첫 블록부터 탐색 시작 - old_nf_bp까지 */
    for (nf_bp = first_bp; nf_bp < old_nf_bp; nf_bp = NEXT_BLKP(nf_bp))
        if (!GET_ALLOC(HDRP(nf_bp)) && (asize <= GET_SIZE(HDRP(nf_bp))))
            return nf_bp;

    /* 사이즈를 만족하는 가용블록 없음 */
    return NULL;
}

/* 가용 블록 검색 함수 : bext-fit
 */
static void *best_fit(size_t asize)
{
    char *bp, *best_bp;
    best_bp = NULL;
    unsigned int best_size = UINT_MAX;

    /* 첫 블록부터 탐색 시작 - 블록의 끝까지 */
    for (bp = first_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        /* 미할당 & 필요 사이즈 만족 & 이전 최선 블록보다 작음 */
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))) && (GET_SIZE(HDRP(bp)) < best_size))
        {
            best_bp = bp;
            best_size = GET_SIZE(HDRP(bp));
        }

    return (void *)best_bp;
}
