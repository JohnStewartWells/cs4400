/*
 * mm-naive.c - The least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by allocating a
 * new page as needed.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused.
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

//small headers/footers increase efficency!!! :)
typedef size_t block_header; 
typedef size_t block_footer;

typedef struct {  
  void* next;
  void* prev;
}node;


/**************************************************************************/
// ******These macros assume you are using a struct for headers and 
//footers*******
// Given a header pointer, get the alloc or size
// #define GET_ALLOC(p) ((block_header *)(p))->allocated
// #define GET_SIZE(p) ((block_header *)(p))->size
/********************************************************************************/
// This assumes you have a struct or typedef called "block_header" and 
//"block_footer"
#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))
// Given a payload pointer, get the header or footer pointer
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)
// Given a payload pointer, get the next or previous payload pointer
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))
// ******These macros assume you are using a size_t for headers and 
//footers ******
// Given a pointer to a header, get or set its value
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))
// Combine a size and alloc bit
#define PACK(size, alloc) ((size) | (alloc))
// Given a header pointer, get the alloc or size
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p) (GET(p) & ~0xF)
//get next header or prev header.
#define PREV_FTRP(bp) (FTRP(PREV_BLKP(bp)))
#define NEXT_HDRP(bp) (HDRP(NEXT_BLKP(bp)))

/**************************************************************************/

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

void* initializeNewPage(size_t size);
void* allocateBlock(void* ptr, size_t size);
void addNodeToFreeList(void* ptr);
void* findFreeBlockAndRemoveFromFreeList(size_t size);
static void* coalesce(void *bp);
void addRemainingSpaceAsFree(void* ptr, int size);

void *current_avail = NULL;
int remainingPageSize = 0;
node* pLastFree= NULL;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  pLastFree = NULL;
  current_avail = NULL;
  remainingPageSize = 0;
  return 0;
}

/* 
 * mm_malloc - Allocate a block by using bytes from current_avail,
 *     grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{

  int newsize = ALIGN(size + OVERHEAD);
  void *p;
  
  p = findFreeBlockAndRemoveFromFreeList(newsize);
  if(p != NULL){
    //change the block to allocated
    PUT(HDRP(p), PACK(GET_SIZE(HDRP(p)), 1));
    PUT(FTRP(p), PACK(GET_SIZE(HDRP(p)), 1));
    return p;
  }

  if (remainingPageSize < newsize) {
    
    if(remainingPageSize!= 0){
      addRemainingSpaceAsFree(current_avail, remainingPageSize);
    }

    //doing some preliminary testing before implementing anything I found 32 this to be the optimal size to call memMap with.
    remainingPageSize = PAGE_ALIGN((newsize*32)+24);
    //int pageSize = 45056;
     //pageSize  = pageSize< newsize ? 524288 : 65536;
    //remainingPageSize = PAGE_ALIGN(pageSize);
    current_avail = mem_map(remainingPageSize);

    block_footer* prolog = current_avail;
    PUT(prolog, PACK(0,1));
    block_header* terminator = current_avail + remainingPageSize -16;
    PUT(terminator, PACK(0,1));
    block_footer* terminatorWithPageSize = current_avail + remainingPageSize-8;
    PUT(terminatorWithPageSize, PACK(remainingPageSize, 1));

    remainingPageSize -=24;
    current_avail +=8;

    if (current_avail == NULL)
      return NULL;
  }

  //adjust remaining size.
  remainingPageSize -= newsize;
  //if the remaining page size is too small to fit another block, add it as filler to the block we're allocating now. 
  if(remainingPageSize < 32){
    newsize += remainingPageSize;
    remainingPageSize -= remainingPageSize;
  }

  block_header* newHeader = current_avail;
  PUT(newHeader, PACK(newsize, 1));

  block_footer* newFooter = current_avail+ newsize -8;
  PUT(newFooter, PACK(newsize, 1));

  p = current_avail+8;

  current_avail += newsize;
  
  return p;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
  //make note that the block is unallocated.

 // printf("\nsize of freed block = %lu\n", size);

  // ptr = coalesce(ptr);
  // if(ptr == NULL){
  //   return;
  // }

  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
 // printf("done packing header and footer.\n");
  addNodeToFreeList(ptr);
 // printf("done adding node to free list\n");
}

void addNodeToFreeList(void* ptr){
  ((node*)ptr)->prev = pLastFree;
  
  if(pLastFree != NULL){
    pLastFree->next = ptr;
  }
  pLastFree = ptr;
  pLastFree->next = NULL;
}

void removeNodeFromFreeList(node* currNode){
  if(currNode == pLastFree){
    //shrink the list by one, 
    pLastFree = currNode->prev;
    //then adjust the pointer of the new last free block as needed.
    if(pLastFree != NULL){
      pLastFree->next = NULL;
    }
    return;
  }
  //if currNode is not the last, we're guaranteed to have a next node, adjust it accordingly
  ((node*)currNode->next)->prev = currNode->prev;
  //then if we have a previous node, adjust it 
  if(currNode->prev != NULL){
    ((node*)currNode->prev)->next = currNode->next;
  }
  return;
}

node* findNodeInFreeList(void* ptr){
  node* currNode = pLastFree;
  while(ptr != pLastFree){
    if(pLastFree == NULL){
      return NULL;
    }
    currNode = pLastFree->prev;
  }
  return currNode;
}


void* findFreeBlockAndRemoveFromFreeList(size_t size){
  
  if(pLastFree == NULL){
    return NULL;
  }

  node* currNode = pLastFree;
  while(GET_SIZE(HDRP(currNode)) < size){
    
    //if there's no previous blocks we've reached the end of the free block list.
    if(currNode->prev == NULL){
      return NULL;
    }

    currNode = currNode->prev;
  }

  removeNodeFromFreeList(currNode);

  return currNode;
}

/**
 * @brief alligns the size to fit a page, then creates page headers/footers w/ size 0 marked as allocated to indicate the start/end of the the page.
 * 
 * @param size 
 * @return void* returns the BLOCK pointer!!!! NOT THE PAGE POINTER!  
 */
void* initializeNewPage(size_t size){
  
  size_t allignedSize = PAGE_ALIGN((ALIGN(size)*4)+32);

  void* pPage = mem_map(allignedSize);

  //we need our pages to be 16 byte alligned, so we'll have nothing in the first 8 bytes, and a prolog in the next 8 bytes.
  block_footer* prologFooter = pPage +8;
  PUT(prologFooter, PACK(0,1));

  //terminator block
  block_footer* terminatorHeader = pPage +allignedSize -16;
  PUT(terminatorHeader, PACK(0,1));

  remainingPageSize = allignedSize -= 32;

  return pPage +16;
}

void addRemainingSpaceAsFree(void* ptr, int size){

  PUT(ptr, PACK(size, 0));
  PUT(ptr+size-8, PACK(size, 0));
  addNodeToFreeList(ptr+8);
}


// ******Recommended helper functions******
/* These functions will provide a high-level recommended structure to your 
program.
* Fill them in as needed, and create additional helper functions 
depending on your design.
*/

/* 
* returns a pointer only if a block needs to be removed from 
*/
static void* coalesce(void *bp){

  printf("got to coalesce\n");

  void* nextHeader = FTRP(bp)+8;
  void* prevFooter = HDRP(bp)-8;
  int nextAlloc = GET_ALLOC(nextHeader);
  int nextSize = GET_SIZE(nextHeader);
  int prevAlloc = GET_ALLOC(prevFooter);
  int prevSize = GET_SIZE(prevFooter);

  if(nextAlloc && prevAlloc){
    if(nextSize == 0 && prevSize == 0){
      //unmap
      printf("got to unmap\n");
      mem_unmap(prevFooter, GET_SIZE(bp)+24);
      return NULL;
    }
    printf("both were alloc'd\n");
    return bp;
  }
  else if(nextAlloc){
    printf("only next was alloc'd\n");
    PUT(prevFooter-prevSize+8, PACK(prevSize + GET_SIZE(bp),0));
    PUT(prevFooter, PACK(prevSize + GET_SIZE(bp),0));
    return prevFooter-prevSize+8;
  }
  else if(prevAlloc){
    printf("only prev was alloc'd\n");
    PUT(HDRP(bp), PACK(GET_SIZE(bp)+nextSize,0));
    PUT(nextHeader+nextSize-8, PACK(GET_SIZE(bp)+nextSize,0));
    printf("success on packing headers\n");
    node* currNode = findNodeInFreeList;
    printf("found node\n");
    if(currNode == NULL){
      printf("found node was NULL!\n");
    }
    removeNodeFromFreeList(currNode);
    printf("removed node from list\n");
    return bp; 
  }

  printf("deet doot.\n");  
  return NULL;
}


void unmapIfNeeded(void* ptr){
  
  void* nextHeader = FTRP(ptr)+8;
  void* prevFooter = HDRP(ptr)-8;

  //if we hit this condition we know we can unmap the page.
  printf("\nsize of prev block: %lu \n", GET_SIZE(prevFooter));
  printf("size of next block: %lu \n", GET_SIZE(nextHeader));
  if(GET_SIZE(prevFooter) == 0 && GET_SIZE(nextHeader) == 0){
    printf("we freed a whole page!\n");
    size_t pageSize = GET_SIZE(nextHeader+8);
    printf("\ngot page size, it was: %lu\n", pageSize);
    mem_unmap(prevFooter, pageSize);
  }
}

