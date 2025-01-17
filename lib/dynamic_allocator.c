/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================
void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//cprintf("dynamic %u\n",daStart);
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0)
			return ;
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #04] [3] DYNAMIC ALLOCATOR - initialize_dynamic_allocator
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("initialize_dynamic_allocator is not implemented yet");
	//Your Code is Here...
	        uint32* daBeg = (uint32*) daStart;
	        *daBeg = 1;
	        uint32* daEnd = (uint32*) (daStart + initSizeOfAllocatedSpace - sizeof(int));
	        *daEnd = 1;
	        uint32 freeBlockSize = initSizeOfAllocatedSpace - 2 * sizeof(int);
	        uint32* freeBlockHeader = (uint32*) (daStart + sizeof(int));
	        *freeBlockHeader = freeBlockSize;
	        uint32* freeBlockFooter = (uint32*) (daStart + initSizeOfAllocatedSpace - 2 * sizeof(int));
	        *freeBlockFooter = freeBlockSize;
	        LIST_INIT(&freeBlocksList);
	        struct BlockElement* freeBlock = (struct BlockElement*) (freeBlockHeader + 1);
	        LIST_INSERT_HEAD(&freeBlocksList, freeBlock);
}

//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	//TODO: [PROJECT'24.MS1 - #05] [3] DYNAMIC ALLOCATOR - set_block_data
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("set_block_data is not implemented yet");
	//Your Code is Here...

	uint32 metaData = totalSize;
		if (isAllocated) {
			metaData |= 1;
		}

		uint32* header = (uint32*)va - 1;
		*header = metaData;
		uint32* footer = (uint32*)((char*)va + totalSize - (2 * sizeof(uint32)));
		*footer = metaData;
}

//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{

			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}

	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #06] [3] DYNAMIC ALLOCATOR - alloc_block_FF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("alloc_block_FF is not implemented yet");
	//Your Code is Here...
	if (size == 0) {return NULL;}

						struct BlockElement* element = NULL;
						struct BlockElement* old_blk;
						uint32 required_size =size + 2*sizeof(int);
						//print_blocks_list(*(struct MemBlock_LIST*)&freeBlocksList);
						LIST_FOREACH(element, &freeBlocksList)
						{

							uint32 block_size = get_block_size(element);
							//cprintf("element is at %x and it's size is %d\n",element,block_size);
							uint32 net_size = block_size - required_size;
							int8 is_alloc = is_free_block(element);
							if (is_alloc && block_size >= required_size && net_size < 16)
							{

								LIST_REMOVE(&(freeBlocksList), element);
								set_block_data(element,block_size, 1);
								//cprintf("no splittin%x\n",element);
								//cprintf("size is %d\n",required_size);
								//cprintf("alloc block is %x\n", element);
								return element;
							}
							else if (is_alloc && block_size > required_size && net_size >= 16)
							{
								//cprintf("splitting\n");
								//cprintf("size is %d\n",required_size);
								struct BlockElement* temp = (struct BlockElement *)((char *)element + required_size);

								set_block_data(temp, net_size, 0);
								//cprintf("free block is %x\n",temp);
								set_block_data(element, required_size, 1);

								LIST_INSERT_AFTER(&freeBlocksList, element, temp);
								LIST_REMOVE(&freeBlocksList, element);
								//cprintf("alloc block is %x\n", element);
								return element;
							}
							old_blk = element;
						}
						uint32 noofpages=ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE;
						//cprintf("noofpages in alloc %d\n",noofpages);
						//cprintf("before\n");
						//print_blocks_list(*(struct MemBlock_LIST*)&freeBlocksList);

						void* sbrkret=sbrk(noofpages);
						//cprintf("after\n");
						//print_blocks_list(*(struct MemBlock_LIST*)&freeBlocksList);

						//cprintf("sbrkret: %x\n", sbrkret);
								if(sbrkret==(void*)-1){return NULL;}
								else
									{
										//cprintf("1\n");
//										uint32* endblock =(uint32*)(char*)sbrkret+(noofpages * PAGE_SIZE)-sizeof(int);
										uint32* endblock =sbrk(0)-sizeof(int);
										*endblock |= 1;
     									 //cprintf("sbrk: END block set at %x with value %u\n", endblock, *endblock);
     									struct BlockElement *footer_of_prevBlock =
     									    (struct BlockElement *)((char *)sbrkret - (2 * sizeof(uint32)));

										//cprintf("2\n");
										uint32 footer_metadata = *(uint32 *)footer_of_prevBlock;										//cprintf("3\n");
										uint32 prev_block_size = footer_metadata & ~1;
										//cprintf("4\n");
										uint32 allocORfree =footer_metadata&1;
										//cprintf("5\n");
										uint32 finalsize=noofpages * PAGE_SIZE /*+ (2*sizeof(int))*/;
										//cprintf("FOOTER ADDRES: %x STATE: %u SIZE: %u\n",footer_of_prevBlock,allocORfree,prev_block_size);
										//cprintf("6\n");
										bool merge = 0;
										if(!allocORfree && prev_block_size > 0) //not 1 means free
											{

												//cprintf("merge\n");
												merge = 1;
												//cprintf("element size %d\n",prev_block_size);
												finalsize+= prev_block_size ; //merge
												//cprintf("element is %x\n",element);
												//print_blocks_list(*(struct MemBlock_LIST*)&freeBlocksList);
												//struct BlockElement* oldblk = (struct BlockElement*)((uint32 *)element - (uint32 *)prev_block_size);
												LIST_REMOVE(&freeBlocksList,old_blk);
											}
										struct BlockElement* new_block;
										if(merge == 0)
										 new_block=(struct BlockElement*)sbrkret;
										else
											new_block= old_blk;
										set_block_data(new_block, finalsize, 0);
										LIST_INSERT_TAIL(&freeBlocksList, new_block);
										//cprintf("size : %d , address: %x\n",finalsize,new_block);
										return alloc_block_FF(size);
									}



  return NULL;
}

//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
uint32 prepare_allocation_size(uint32 size) {
    // Ensure size is even and meets minimum requirements
    if (size % 2 != 0) size++;
    if (size < DYN_ALLOC_MIN_BLOCK_SIZE) size = DYN_ALLOC_MIN_BLOCK_SIZE;

    if (!is_initialized) {
        uint32 required_size = size + 2 * sizeof(int) + 2 * sizeof(int); // includes headers, footers, begin/end markers
        uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE) / PAGE_SIZE);
        uint32 da_break = (uint32)sbrk(0);
        initialize_dynamic_allocator(da_start, da_break - da_start);
    }
    return size;
}

void *alloc_block_BF(uint32 size)
{
	//TODO: [PROJECT'24.MS1 - BONUS] [3] DYNAMIC ALLOCATOR - alloc_block_BF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("alloc_block_BF is not implemented yet");
	//Your Code is Here...
    if (size == 0) return NULL;

    size = prepare_allocation_size(size);

    struct BlockElement *element, *best_fit = NULL;
    uint32 best_fit_size = 0xFFFFFFFF;
    uint32 required_size = size + 2 * sizeof(int); // Size including metadata

    // Traverse free list to find the best fit block
    LIST_FOREACH(element, &freeBlocksList)
    {
        uint32 block_size = get_block_size(element);
        int8 is_alloc = is_free_block(element);

        if (is_alloc && block_size >= required_size && block_size < best_fit_size)
        {
            best_fit = element;
            best_fit_size = block_size;
        }
    }

    // No suitable block found; request additional memory
    if (!best_fit)
    {
        sbrk(required_size);
        return NULL;
    }

    uint32 net_size = best_fit_size - required_size;

    // Check if block should be split
    if (net_size >= 16)
    {
        // Split the block into allocated part and new free block
        struct BlockElement *new_free_block = (struct BlockElement *)((char *)best_fit + required_size);
        set_block_data(new_free_block, net_size, 0);
        set_block_data(best_fit, required_size, 1);

        // Insert the new free block in the free list and remove allocated block
        LIST_INSERT_AFTER(&freeBlocksList, best_fit, new_free_block);
        LIST_REMOVE(&freeBlocksList, best_fit);
    }
    else
    {
        // Allocate entire block without splitting
        set_block_data(best_fit, best_fit_size, 1);
        LIST_REMOVE(&freeBlocksList, best_fit);
    }

    return best_fit;
}


//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
	//TODO: [PROJECT'24.MS1 - #07] [3] DYNAMIC ALLOCATOR - free_block
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("free_block is not implemented yet");
	//Your Code is Here...

    uint32 state = is_free_block(va);
    if (state != 0)
    {
        //cprintf("Block is already free\n");
        return;
    }

    uint32 init_size = get_block_size(va);
    struct BlockElement* current_element = (struct BlockElement*)va;
    uint32* nxt_block = (uint32*)(va + init_size);
    uint32 nxt_block_size = get_block_size(nxt_block);
    uint32 * footer_of_prevBlock = va - (2 * sizeof(uint32));
    uint32 footer_metadata = *footer_of_prevBlock;
    uint32 prev_block_size = footer_metadata & ~1;
    uint32* prev_block = va - prev_block_size;
    uint32 nxt_block_state = 0;
    if (nxt_block_size != 0) {
        nxt_block_state = is_free_block(nxt_block);
    }

    uint32 prev_block_state = 0;
    if (prev_block_size > 0) {
        prev_block_state = is_free_block(prev_block);
    }

    uint32 total_size = init_size;
    struct BlockElement* new_start = current_element;



    // Coalesce with previous block if it's free
    if (prev_block_state == 1 && prev_block_size > 0 && nxt_block_state == 0)
    {
    	//cprintf("coalesce with prev\n");
        total_size += prev_block_size;
        new_start = (struct BlockElement*)(va - prev_block_size);
        LIST_REMOVE(&freeBlocksList, (struct BlockElement*)prev_block);
    }

    // Coalesce with next block if it's free
    if (nxt_block_state == 1 && nxt_block_size > 0 && prev_block_state == 0)
    {
    	//cprintf("coalesce with next\n");
        total_size += nxt_block_size;
        LIST_REMOVE(&freeBlocksList, (struct BlockElement*)nxt_block);
    }

    // Coalesce with both previous and next blocks
    if (prev_block_state == 1 && nxt_block_state == 1)
    {
    	//cprintf("coalesce with both\n");
        total_size = init_size + prev_block_size + nxt_block_size;
        new_start = (struct BlockElement *)(va - prev_block_size);
        LIST_REMOVE(&freeBlocksList, (struct BlockElement*)prev_block);
        LIST_REMOVE(&freeBlocksList, (struct BlockElement*)nxt_block);
    }

    set_block_data(new_start, total_size, 0);

    if (LIST_SIZE(&freeBlocksList) == 0)
    {
    	//cprintf("free list is empty \n");
        LIST_INSERT_HEAD(&freeBlocksList, new_start);
    }
    else
    {
        insert(new_start);
    	//cprintf("free list size : %d\n", LIST_SIZE(&freeBlocksList));
    }
}

void insert(struct BlockElement* current_element)
{
    struct BlockElement* elementt;

    LIST_FOREACH(elementt, &freeBlocksList)
    {
    	//cprintf("inside loop\n");
        struct BlockElement* nxt_block = elementt->prev_next_info.le_next;

        // Insert before the current block in the list
        if (current_element < elementt)
        {
        	//cprintf("Insert before the current block\n");
            LIST_INSERT_BEFORE(&freeBlocksList, elementt, current_element);
            return;
        }

        // Insert after the current block but before the next
        else if (nxt_block != NULL && current_element < nxt_block)
        {
        	//cprintf("Insert after the current block\n");
            LIST_INSERT_AFTER(&freeBlocksList, elementt, current_element);
            return;
        }
    }

	//cprintf("Insert at tail\n");
    LIST_INSERT_TAIL(&freeBlocksList, current_element);
}



//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	//TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("realloc_block_FF is not implemented yet");

	uint32 NewSize = new_size + 2*sizeof(uint32);
    struct BlockElement* current_element = (struct BlockElement*)va;
	uint32 current_block_size = get_block_size(va);
	uint32 net_size = NewSize - current_block_size;
	uint32* next_block = (uint32*)(va + current_block_size);
	uint32 next_block_size = get_block_size(next_block);
	uint32 next_is_free = is_free_block(next_block);

	if(new_size == 0 && va != NULL)  { free_block(va); return NULL; }

	if(va == NULL && new_size > 0 )  return alloc_block_FF(new_size);

	if(new_size == 0 && va == NULL)  return NULL;

    if(is_free_block(va) == 1)  return alloc_block_FF(new_size);

	if(NewSize == current_block_size)  return va;


	if(NewSize > current_block_size)
	{
		if(next_is_free == 1 && next_block_size >= net_size)
		{
		    //cprintf("resize\n");
		    uint32 remaining_free_size = next_block_size - net_size;
            uint32* remaining_free_block = va + NewSize;

            //cprintf("next block size %d\n",next_block_size);
  //          cprintf("net size %d\n",net_size);
//            cprintf("remaining_free_size %d\n",remaining_free_size);

            if (remaining_free_size >= 16)
            {
//            	cprintf("remaining_free_size >= 16\n");
                set_block_data(current_element,NewSize,1);
                set_block_data(remaining_free_block,remaining_free_size,0);
                LIST_REMOVE(&freeBlocksList, (struct BlockElement*)next_block);
                insert((struct BlockElement*)remaining_free_block);
             }
    	     else
             {
//    		    cprintf("remaining_free_size < 16\n");
 		        set_block_data(current_element,NewSize,1);
                LIST_REMOVE(&freeBlocksList, (struct BlockElement*)next_block);
             }

            return current_element;
		}

		else if((next_block_size == 0) || (next_is_free == 1 && next_block_size < net_size) || (next_is_free == 0))
		{
//			cprintf("relocate \n");
			uint32* new_block = alloc_block_FF(new_size);
			uint32 current_data_size = current_block_size - 2 * sizeof(uint32);
			memcpy(new_block, current_element, current_data_size);
			free_block(current_element);

			return (struct BlockElement*)new_block;
		}
	}



	else if (NewSize < current_block_size)
	{
//		cprintf("split \n");
	    uint32 remaining_size = current_block_size - NewSize;
        uint32* remaining_free_block = va + NewSize;

	    if (remaining_size >= 16)
	    {
	//		cprintf("remaining_size >= 16 \n");
	    	set_block_data(remaining_free_block, remaining_size, 0);
	        set_block_data(current_element, NewSize, 1);
	        insert((struct BlockElement*)remaining_free_block);
	    }
	    else
	    {
		//	cprintf("remaining_size < 16 \n");
	        set_block_data(current_element,current_block_size , 1);
	    }
	    return current_element;
	}


	sbrk(NewSize);
	return NULL;
}


/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
