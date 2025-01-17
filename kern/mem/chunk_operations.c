/*
 * chunk_operations.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include <kern/trap/fault_handler.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/proc/user_environment.h>
#include "kheap.h"
#include "memory_manager.h"
#include <inc/queue.h>

//extern void inctst();

/******************************/
/*[1] RAM CHUNKS MANIPULATION */
/******************************/

//===============================
// 1) CUT-PASTE PAGES IN RAM:
//===============================
//This function should cut-paste the given number of pages from source_va to dest_va on the given page_directory
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, cut-paste the number of pages and return 0
//	ALL 12 permission bits of the destination should be TYPICAL to those of the source
//	The given addresses may be not aligned on 4 KB
int cut_paste_pages(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 num_of_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] cut_paste_pages
	// Write your code here, remove the panic and write your code
	panic("cut_paste_pages() is not implemented yet...!!");
}

//===============================
// 2) COPY-PASTE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	If ANY of the destination pages exists with READ ONLY permission, deny the entire process and return -1.
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages doesn't exist, create it with the following permissions then copy.
//	Otherwise, just copy!
//		1. WRITABLE permission
//		2. USER/SUPERVISOR permission must be SAME as the one of the source
//	The given range(s) may be not aligned on 4 KB
int copy_paste_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] copy_paste_chunk
	// Write your code here, remove the //panic and write your code
	panic("copy_paste_chunk() is not implemented yet...!!");
}

//===============================
// 3) SHARE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	It should set the permissions of the second range by the given perms
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, share the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	The given range(s) may be not aligned on 4 KB
int share_chunk(uint32* page_directory, uint32 source_va,uint32 dest_va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] share_chunk
	// Write your code here, remove the //panic and write your code
	panic("share_chunk() is not implemented yet...!!");
}

//===============================
// 4) ALLOCATE CHUNK IN RAM:
//===============================
//This function should allocate the given virtual range [<va>, <va> + <size>) in the given address space  <page_directory> with the given permissions <perms>.
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, allocate the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	Allocation should be aligned on page boundary. However, the given range may be not aligned.
int allocate_chunk(uint32* page_directory, uint32 va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] allocate_chunk
	// Write your code here, remove the //panic and write your code
	panic("allocate_chunk() is not implemented yet...!!");
}

//=====================================
// 5) CALCULATE ALLOCATED SPACE IN RAM:
//=====================================
void calculate_allocated_space(uint32* page_directory, uint32 sva, uint32 eva, uint32 *num_tables, uint32 *num_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_allocated_space
	// Write your code here, remove the panic and write your code
	panic("calculate_allocated_space() is not implemented yet...!!");
}

//=====================================
// 6) CALCULATE REQUIRED FRAMES IN RAM:
//=====================================
//This function should calculate the required number of pages for allocating and mapping the given range [start va, start va + size) (either for the pages themselves or for the page tables required for mapping)
//	Pages and/or page tables that are already exist in the range SHOULD NOT be counted.
//	The given range(s) may be not aligned on 4 KB
uint32 calculate_required_frames(uint32* page_directory, uint32 sva, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_required_frames
	// Write your code here, remove the panic and write your code
	panic("calculate_required_frames() is not implemented yet...!!");
}

//=================================================================================//
//===========================END RAM CHUNKS MANIPULATION ==========================//
//=================================================================================//

/*******************************/
/*[2] USER CHUNKS MANIPULATION */
/*******************************/

//======================================================
/// functions used for USER HEAP (malloc, free, ...)
//======================================================

//=====================================
/* DYNAMIC ALLOCATOR SYSTEM CALLS */
//=====================================
void* sys_sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the current user program to increase the size of its heap
	 * 				by the given number of pages. You should allocate NOTHING,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) As in real OS, allocate pages lazily. While sbrk moves the segment break, pages are not allocated
	 * 		until the user program actually tries to access data in its heap (i.e. will be allocated via the fault handler).
	 * 	2) Allocating additional pages for a process’ heap will fail if, for example, the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sys_sbrk fails, the net effect should
	 * 		be that sys_sbrk returns (void*) -1 and that the segment break and the process heap are unaffected.
	 * 		You might have to undo any operations you have done so far in this case.
	 */

	//TODO: [PROJECT'24.MS2 - #11] [3] USER HEAP - sys_sbrk
	/*====================================*/
	/*Remove this line before start coding*/
	 struct Env* env = get_cpu_proc(); //the current running Environment to adjust its break limit
	 if (env == NULL){
		 return (void*)-1;
	 }

    uint32 old_break = env->u_break;

    if (numOfPages > 0) {
        if (ROUNDUP((env->u_break + (numOfPages*PAGE_SIZE)), PAGE_SIZE) > env->u_hard_limit) {
            return (void*)-1; // Exceeds the hard limit
        }

        env->u_break = ROUNDUP((env->u_break + (numOfPages*PAGE_SIZE)), PAGE_SIZE);

        for (uint32 i = 0; i < numOfPages; i++)
        {
            uint32 currentAddr = old_break + (i * PAGE_SIZE);
            uint32* pageTablePtr = NULL;

            // for debugging purpose
            uint32 pageDirIndex = PDX(currentAddr);
            uint32 pageTableIndex = PTX(currentAddr);
            int tableStatus = get_page_table(env->env_page_directory, currentAddr, &pageTablePtr);

            if (tableStatus == TABLE_NOT_EXIST)
            {
                create_page_table(env->env_page_directory, currentAddr);
            }
            pt_set_page_permissions(env->env_page_directory, currentAddr, PERM_AVAILABLE, 0);
        }
        return (void*)old_break;
    }
    else if (numOfPages == 0) {
            return (void*)env->u_break;
    }

    return (void*)-1;
	/*====================================*/
}



//=====================================
// 1) ALLOCATE USER MEMORY:
//=====================================
void allocate_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{
    // Start and end of the range
    uint32 startAddr = ROUNDDOWN(virtual_address, PAGE_SIZE);
    uint32 endAddr = ROUNDUP(virtual_address + size, PAGE_SIZE);

    // Traverse the range and allocate pages
    for (uint32 currentAddr = startAddr; currentAddr < endAddr; currentAddr += PAGE_SIZE)
    {
        uint32* pageTablePtr = NULL;


        // for debugging purpose
        uint32 pageDirIndex = PDX(currentAddr);
        uint32 pageTableIndex = PTX(currentAddr);

        // Ensure the page table exists for the given address
        if (get_page_table(e->env_page_directory, currentAddr, &pageTablePtr) == TABLE_NOT_EXIST)
        {
            create_page_table(e->env_page_directory, currentAddr); // Create a new page table
        }

        // Set permissions for the page: mark as available
        pt_set_page_permissions(e->env_page_directory, currentAddr, PERM_AVAILABLE, 0);
    }
}


//=====================================
// 2) FREE USER MEMORY:
//=====================================
void handle_lru_replacement(struct Env* e, uint32 virtual_address)
{
    env_page_ws_invalidate(e, virtual_address);
}

void handle_fifo_replacement(struct Env* e, uint32 virtual_address)
{
    struct WorkingSetElement* wsElem = NULL;
    struct WorkingSetElement* foundElem = NULL;

    // Search for the element in the working set list
    LIST_FOREACH(wsElem, &(e->page_WS_list))
    {
        if (ROUNDDOWN(wsElem->virtual_address, PAGE_SIZE) == ROUNDDOWN(virtual_address, PAGE_SIZE))
        {
            foundElem = wsElem;
            break;
        }
    }

    if (foundElem)
    {
        if (e->page_last_WS_element == foundElem)
        {
            e->page_last_WS_element = (foundElem->prev_next_info.le_next != NULL)
                                      ? foundElem->prev_next_info.le_next
                                      : LIST_FIRST(&(e->page_WS_list));
        }
        LIST_REMOVE(&(e->page_WS_list), foundElem);
        kfree(foundElem);
    }
}

void handle_page_replacement(struct Env* e, struct FrameInfo* frameDetails, uint32 virtual_address)
{
    if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
    {
        handle_lru_replacement(e, virtual_address);
    }
    else
    {
        struct WorkingSetElement* wsElem = NULL;

        // Search for the element in the working set list
        LIST_FOREACH(wsElem, &(e->page_WS_list))
        {
            if (ROUNDDOWN(wsElem->virtual_address, PAGE_SIZE) == ROUNDDOWN(virtual_address, PAGE_SIZE))
            {
                handle_fifo_replacement(e, virtual_address);
                return;
            }
        }

        handle_lru_replacement(e, virtual_address);

        if (isPageReplacmentAlgorithmFIFO() && e->page_last_WS_element == NULL)
        {
            e->page_last_WS_element = LIST_FIRST(&(e->page_WS_list));
        }
    }
}

void unmap_and_reset_permissions(struct Env* e, uint32 virtual_address)
{
    int pagePermissions = pt_get_page_permissions(e->env_page_directory, virtual_address);

    // Check if the page is present and has both user and writable permissions
    bool isPageEligibleForUnmapping = (pagePermissions & PERM_PRESENT) &&
                                      (pagePermissions & PERM_USER) &&
                                      (pagePermissions & PERM_WRITEABLE);

    if (isPageEligibleForUnmapping)
    {
        // Unmap the frame associated with the virtual address
        unmap_frame(e->env_page_directory, virtual_address);
    }

    // Reset permissions and remove from frame table
    uint32* pageTablePtr = NULL;
    get_page_table(e->env_page_directory, virtual_address, &pageTablePtr);
    if (pageTablePtr)
    {
        pageTablePtr[PTX(virtual_address)] &= ~PERM_AVAILABLE;
    }
    pf_remove_env_page(e, virtual_address);
}

void free_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{
    uint32 currAddr = virtual_address;

    while (currAddr < virtual_address + size)
    {
        // Retrieve the page's frame info and the corresponding page table
        uint32* pageTable = NULL;
        struct FrameInfo* frameDetails = get_frame_info(e->env_page_directory, currAddr, &pageTable);

        // Handle page replacement logic
        handle_page_replacement(e, frameDetails, currAddr);

        // Unmap the page and reset permissions
        unmap_and_reset_permissions(e, currAddr);

        // Move to the next page in memory
        currAddr += PAGE_SIZE;
    }
}

//=====================================
// 2) FREE USER MEMORY (BUFFERING):
//=====================================
void __free_user_mem_with_buffering(struct Env* e, uint32 virtual_address, uint32 size)
{
	// your code is here, remove the panic and write your code
	panic("__free_user_mem_with_buffering() is not implemented yet...!!");
}

//=====================================
// 3) MOVE USER MEMORY:
//=====================================
void move_user_mem(struct Env* e, uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
{
	//[PROJECT] [USER HEAP - KERNEL SIDE] move_user_mem
	//your code is here, remove the panic and write your code
	panic("move_user_mem() is not implemented yet...!!");
}

//=================================================================================//
//========================== END USER CHUNKS MANIPULATION =========================//
//=================================================================================//
