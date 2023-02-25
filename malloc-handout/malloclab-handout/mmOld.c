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

struct node{  
  void* next;
  void* prev;
  void* pFreeBlock;
};

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
struct node* findEmptyBlock(size_t oldSize);
void addFreeBlockToEndOFList(void* ptr);
void removeFreeBlockFromLinkedList(struct node nodeToRemove);

void *current_avail = NULL;
int remainingPageSize = 0;

struct node* pLastFree = NULL;

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

//size is how much 
void *mm_malloc(size_t size)
{
  //if no empty block w/ the right size are found, create a new page.
  struct node* foundBlock;
  // printf("\nsearching for a free block...\n");
  foundBlock = findEmptyBlock(size);
  //  printf("\nsearch done... continuing...\n");



  if(foundBlock == NULL){
  //  printf("\nno free block found.\n");

    void *pNewBlock =  initializeNewPage(size*2);

    if(pNewBlock == NULL){
      return NULL;
    }
    PUT(HDRP(pNewBlock), PACK(size,1));
    PUT(FTRP(pNewBlock), PACK(size,1));
    // printf("\nmaking space for new block.\n");
    return pNewBlock; 
  }

  // printf("\nfree block found!\n");
  removeFreeBlockFromLinkedList(*foundBlock);
  PUT(HDRP(foundBlock->pFreeBlock), PACK(size,1));
  PUT(FTRP(foundBlock->pFreeBlock), PACK(size,1));
  return foundBlock->pFreeBlock;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
  printf("\nsearch done... continuing...\n");
  //adjust the header info in the blockHeader
  size_t headerInfo = PACK(GET_SIZE(HDRP(ptr)), 0);
  PUT(HDRP(ptr), headerInfo);
  PUT(FTRP(ptr), headerInfo);
  addFreeBlockToEndOFList(ptr);

  printf("\nreturning from free!\n");

  return;
}

// ******Recommended helper functions******
/* These functions will provide a high-level recommended structure to your 
program.
* Fill them in as needed, and create additional helper functions 
depending on your design.
*/

/* Coalesce a free block if applicable
* Returns pointer to new coalesced block
*/
//static void* coalesce(void *bp);

/**
 * @brief alligns the size to fit a page, then creates page headers/footers w/ size 0 marked as allocated to indicate the start/end of the the page.
 * 
 * @param size 
 * @return void* returns the BLOCK pointer!!!! NOT THE PAGE POINTER!  
 */
void* initializeNewPage(size_t size){
  //16 represents the page overhead, while OVERHEAD is block overhead.
  int allignedSize = PAGE_ALIGN(16 + ALIGN(size+OVERHEAD));

  void* p = mem_map(allignedSize);
  if(p == NULL){
    return NULL;
  }

  //we can have an allocated block header w/ size 0 to mark the start of the block. 
  //we can check for these when we coallesce to see if the block is completely free.
  //this removes the need for a linked list of pages. 
  block_header* newPH = p;
  PUT(newPH, PACK(0,1));
  //let's do the same for the footer. 
  block_footer* newPF = p+allignedSize-8;
  PUT(newPF, PACK(0,1));

  block_header* newBH = p+8;
  PUT(newBH ,PACK(allignedSize-32,0));

  block_footer* newBF = p +allignedSize -16;
  PUT(newBF ,PACK(allignedSize-32,0));

  //we should "free" the block and add it to the linked list here. 

  void* pBlock = (p+16);
  return pBlock;
}

struct node* findEmptyBlock(size_t oldSize){

  //no sense going on if there's no free blocks!
  if(pLastFree == NULL){
    // printf("\nreturning null from free.\n");
    return NULL;
  }

  //adjust the size to consider header/footers
  size_t size = ALIGN(oldSize+OVERHEAD);

  struct node* currNode;
  currNode = pLastFree;

  while(GET_SIZE(HDRP(currNode->pFreeBlock)) < size ){
    struct node* prevNode = currNode->prev;
    if(prevNode == NULL || prevNode->pFreeBlock == NULL){
      // printf("\nreturning null from free.\n");
      return NULL;
    }
    currNode = prevNode;
    // printf("\n...\n");
    // printf("\nsize of prev node is: %lu \n", GET_SIZE(HDRP(currNode->pFreeBlock)) );
  }

  // printf("\nfound an empty block.\n");

  return currNode;
}


void removeFreeBlockFromLinkedList(struct node nodeToRemove){
  printf("\nremoving block from free list...\n");
    if(nodeToRemove.next == NULL){
      if(nodeToRemove.prev == NULL){
        printf("\nboth next and prev are null...\n");
        pLastFree = NULL;
      }
      else{
        printf("\njust next was null.\n");
        struct node* prevNode;
        prevNode = nodeToRemove.prev;
        prevNode->next = NULL;
        pLastFree = nodeToRemove.prev;
      }

      return;
    }
  else{
    struct node* nextNode = nodeToRemove.next;
    if(nodeToRemove.prev == NULL){
        printf("\njust prev was null.\n");
        nextNode->prev = NULL;
    }
    else{
        printf("\nno null surrounding nodes.\n");
      struct node* prevNode = nodeToRemove.prev;
      prevNode->next = nodeToRemove.next;
      nextNode->prev = nodeToRemove.prev;
    }
  }

  //make sure the node to remove is sent to the shadow realm jimbo.
  nodeToRemove.next= NULL;
  nodeToRemove.prev = NULL;
  nodeToRemove.pFreeBlock = NULL;

}

void addFreeBlockToEndOFList(void* ptr){
  printf("\nadding block to end of free list!  \n");
  struct node newNode;
  newNode.pFreeBlock = ptr;
  
  if(pLastFree != NULL){
    pLastFree->next = &newNode;
    newNode.prev = pLastFree;
  }

  pLastFree = &newNode;
  printf("\ndone adding free block to list!  \n");
}

