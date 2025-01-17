/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	assert(
			LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() {
	_PageRepAlgoType = PG_REP_CLOCK;
}
void setPageReplacmentAlgorithmFIFO() {
	_PageRepAlgoType = PG_REP_FIFO;
}
void setPageReplacmentAlgorithmModifiedCLOCK() {
	_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;
}
/*2018*/void setPageReplacmentAlgorithmDynamicLocal() {
	_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;
}
/*2021*/void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps) {
	_PageRepAlgoType = PG_REP_NchanceCLOCK;
	page_WS_max_sweeps = PageWSMaxSweeps;
}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	return _PageRepAlgoType == LRU_TYPE ? 1 : 0;
}
uint32 isPageReplacmentAlgorithmCLOCK() {
	if (_PageRepAlgoType == PG_REP_CLOCK)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmFIFO() {
	if (_PageRepAlgoType == PG_REP_FIFO)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmModifiedCLOCK() {
	if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK)
		return 1;
	return 0;
}
/*2018*/uint32 isPageReplacmentAlgorithmDynamicLocal() {
	if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmNchanceCLOCK() {
	if (_PageRepAlgoType == PG_REP_NchanceCLOCK)
		return 1;
	return 0;
}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) {
	_EnableModifiedBuffer = enableIt;
}
uint8 isModifiedBufferEnabled() {
	return _EnableModifiedBuffer;
}

void enableBuffering(uint32 enableIt) {
	_EnableBuffering = enableIt;
}
uint8 isBufferingEnabled() {
	return _EnableBuffering;
}

void setModifiedBufferLength(uint32 length) {
	_ModifiedBufferLength = length;
}
uint32 getModifiedBufferLength() {
	return _ModifiedBufferLength;
}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault = 0;

struct Env* last_faulted_env = NULL;

void fault_handler(struct Trapframe *tf) {

	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//	cprintf("\n************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	//cprintf("MAX SIZE IS %d WSSIZE IS %d \n",cur_env->page_WS_max_size,wsSize);
	if (last_fault_va == fault_va && last_faulted_env == cur_env) {
		num_repeated_fault++;
		if (num_repeated_fault == 3) {
			print_trapframe(tf);
			panic(
					"Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n",
					before_last_fault_va, before_last_eip, fault_va);
		}
	} else {
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32) tf->tf_eip;
	last_fault_va = fault_va;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap) {
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env
				&& fault_va
						>= (uint32) cur_env->kstack&& fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va
				>= (uint32) c->stack&& fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!",
					c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else {
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL) {
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ((faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT)
			!= PERM_PRESENT) {
		// we have a table fault =============================================================
		//		cprintf("[%s] user TABLE fault va %08x\n", curenv->prog_name, fault_va);
		//		print_trapframe(tf);

		faulted_env->tableFaultsCounter++;

		table_fault_handler(faulted_env, fault_va);
	} else {
		if (userTrap) {
			/*============================================================================================*/
			//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			int page_permissions = pt_get_page_permissions(
					faulted_env->env_page_directory, fault_va);
			if (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) {
				if ((!(page_permissions & PERM_AVAILABLE))
						&& (!(page_permissions & PERM_PRESENT))) {
					//cprintf("UUUUUUUUUUUUUUnmarked\n");
					env_exit(); // unmarked
				}
			}
			if (fault_va >= USER_LIMIT) {
				//cprintf("KKKKKKKKKKERNEL\n");
				env_exit();  //kernel
			}
			if ((!(page_permissions & PERM_WRITEABLE))
					&& (page_permissions & PERM_PRESENT)) {
				//cprintf("RRRRRRRead\n");
				env_exit(); //  read only permission
			}

			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory,
				fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va);
		/*============================================================================================*/

		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter++;

		//		cprintf("[%08s] user PAGE fault va %08x\n", curenv->prog_name, fault_va);
		//		cprintf("\nPage working set BEFORE fault handler...\n");
		//		env_page_ws_print(curenv);

		if (isBufferingEnabled()) {
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		} else {
			//cprintf("IN ELSEEEEEEEEEEE\n");
			//page_fault_handler(faulted_env, fault_va);
			//cprintf("MAX SIZE IS %d WSSIZE IS %d \n",faulted_env->page_WS_max_size,wsSize);
			page_fault_handler(faulted_env, fault_va);
			//cprintf("MAX SIZE IS %d WSSIZE IS %d \n",faulted_env->page_WS_max_size,wsSize);
		}
		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(curenv);

	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va) {
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory,
				(uint32) fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}




//=========================
// [3] PAGE FAULT HANDLER:
//=========================65uj6/
void page_fault_handler(struct Env * faulted_env, uint32 fault_va) {
	//cprintf("um hereee page fault handler\n");
#if USE_KHEAP
	struct WorkingSetElement *victimWSElement = NULL;
	uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
	//	cprintf("MAX SIZE IS %d WSSIZE IS %d \n",faulted_env->page_WS_max_size,wsSize);
#else
	int iWS =faulted_env->page_last_WS_index;
	uint32 wsSize = env_page_ws_get_size(faulted_env);
#endif
	if (isPageReplacmentAlgorithmNchanceCLOCK()) {
		if (wsSize < (faulted_env->page_WS_max_size)) {
			//cprintf("placement\n");

		    // Ensure proper placement handling

		    struct FrameInfo* ptr_frame_info = NULL;
		    int alloc_ret = allocate_frame(&ptr_frame_info);
		    if (alloc_ret != 0) {
		        panic("Placement: Unable to allocate frame!");
		    }

		    int mapret = map_frame(faulted_env->env_page_directory, ptr_frame_info, fault_va,PERM_PRESENT | PERM_WRITEABLE | PERM_USER);
		    if (mapret != 0) {
		        panic("Placement: Failed to map frame!");
		    }


		    // Read page from the page file
		    int ret = pf_read_env_page(faulted_env, (void*)fault_va);
		    if (ret != 0 && ret != E_PAGE_NOT_EXIST_IN_PF) {
		        panic("Placement: Failed to read page from page file!");
		    }

		    if (ret == E_PAGE_NOT_EXIST_IN_PF) {
		        // Ensure valid fault address is in stack or heap
		        if (!((fault_va >= USTACKBOTTOM && fault_va <= USTACKTOP) || (fault_va >= USER_HEAP_START && fault_va <= USER_HEAP_MAX))) {
		            env_exit(); // Invalid page not in heap or stack
		        }
		    }

		    //acquire_spinlock(&placement_lock);

		    // Insert the page into the working set
		    struct WorkingSetElement* rett = env_page_ws_list_create_element(faulted_env, fault_va);
		    if (rett != NULL) {
		        LIST_INSERT_TAIL(&(faulted_env->page_WS_list), rett);

		        if (faulted_env->page_WS_max_size > faulted_env->page_WS_list.size) {
		            faulted_env->page_last_WS_element = NULL;
		        } else if (faulted_env->page_WS_max_size == faulted_env->page_WS_list.size) {
		            faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
		        }
		    } else {
		        panic("Placement: Failed to create working set element!");
		    }

		    //release_spinlock(&placement_lock);
			//cprintf("end of placement\n");

		}
		else {
			//cprintf("replacement\n");

			//cprintf("REPLACEMENT=========================WS Size = %d\n", wsSize );
			//refer to the project presentation and documentation for details
			//TODO: [PROJECT'24.MS3] [2] FAULT HANDLER II - Replacement
			//Write your code here, remove the panic and write your code
			//panic("page_fault_handler() Replacement is not implemented yet...!!");

			// Start with the next element from the last used one
			struct WorkingSetElement *element =faulted_env->page_last_WS_element;
			struct WorkingSetElement *element_c =faulted_env->page_last_WS_element;
			struct WorkingSetElement *felement = LIST_FIRST(&(faulted_env->page_WS_list));
			if (element == NULL)
				element = felement;
			//env_page_ws_print(faulted_env);
			int abs_val;
			if(page_WS_max_sweeps >= 0)
					abs_val = page_WS_max_sweeps;
			else
				abs_val = page_WS_max_sweeps * -1;
			for(int i = 0; i< faulted_env->page_WS_max_size; i++){
				// Check if the page is recently used
				uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory,(uint32) element->virtual_address);
				uint32 is_used = perm & PERM_USED;
				uint32 is_modified = perm & PERM_MODIFIED;

				//cprintf("1 REEEE after break");
				if (page_WS_max_sweeps == 0 && !is_used) //FIFO
						{
					//cprintf("1 REEE before calling\n");
					replacement_NERO_RORO(faulted_env, fault_va, element, is_modified);
					break;
				}
				if (is_used) {
					// Clear the PERM_USED flag
					pt_set_page_permissions(faulted_env->env_page_directory,(uint32) element->virtual_address, 0, PERM_USED);
					element->sweeps_counter = 0; // Reset the sweeps counter

				} else {
					//cprintf("modified: %d, no of sweeps: %d\n", is_modified,element->sweeps_counter);
					if(page_WS_max_sweeps < 0)
					{
					if(is_modified && element->sweeps_counter == 1){ //modiied 2nd try
						//cprintf("mod 2nd try\n");
						element->sweeps_counter += abs_val ;
					}
					else if(is_modified){//mod first try
						//cprintf("mod 1st try\n");
						element->sweeps_counter = 1;
					}
					else{//normal case
					//cprintf("Normal case\n");
					//cprintf("abs val %d\n",abs_val );
					element->sweeps_counter = abs_val ;
				}
					}
					else
					{
						element->sweeps_counter = abs_val ;
					}
					unsigned int num_of_sweeps = element->sweeps_counter;
					//cprintf("num_of_sweeps %d, page_WS_max_sweeps %d\n",num_of_sweeps,page_WS_max_sweeps);
					// Determine replacement conditions
					if (page_WS_max_sweeps < 0) { // Modified algorithm
						if ((is_modified && num_of_sweeps == abs_val + 1)
								|| (!(is_modified) && (num_of_sweeps == abs_val))) {
								//cprintf("MOD\n");
							replacement_NERO_RORO(faulted_env, fault_va,element, is_modified);
							// cprintf("2 after calling\n");
							break;// Exit once a page is replaced
						}
					} else if (page_WS_max_sweeps > 0) { // Normal algorithm
						if (num_of_sweeps == page_WS_max_sweeps) {
							//cprintf("NORMAL\n");
							replacement_NERO_RORO(faulted_env, fault_va,element, is_modified);
							break; // Exit once a page is replaced
						}
					}
				}

				// Move to the first element if the end of the list is reached
				element = element->prev_next_info.le_next;
				if (element == NULL) {
					element = LIST_FIRST(&(faulted_env->page_WS_list));

				}
			}
			//cprintf("end of replacement\n");

		}

	}

}
void replacement_NERO_RORO(struct Env *faulted_env, uint32 fault_va, struct WorkingSetElement *victim_element, uint32 is_modified) {
	unsigned int victim = victim_element->virtual_address;

	// Get the frame info for the victim page
	uint32 *ptr_page_table = NULL;
	struct FrameInfo *victim_frame_info = get_frame_info(faulted_env->env_page_directory, (uint32) victim, &ptr_page_table);
	if (victim_frame_info == NULL) {
		cprintf("Error: Could not retrieve frame info for victim VA: %x\n",victim);
		return;
	}
	// If the victim page is modified, update it in the page file
	if (is_modified) {
		int update_ret = pf_update_env_page(faulted_env, (uint32) victim,
				victim_frame_info);
		if (update_ret != 0) {
			cprintf("The page file is full, can’t add any more pages to it\n");
			return;
		}
	}

	// Map the new frame to the faulting virtual address
	unmap_frame(faulted_env->env_page_directory, (uint32) victim);
	struct FrameInfo *new_frame = NULL;
	int allF_ret = allocate_frame(&new_frame);
	map_frame(faulted_env->env_page_directory, new_frame, fault_va,PERM_PRESENT | PERM_WRITEABLE | PERM_USER);

	// Read the faulting page into memory
	int ret = pf_read_env_page(faulted_env, (void *) fault_va);
	if (ret == E_PAGE_NOT_EXIST_IN_PF) {
		// Handle invalid page access outside heap/stack
		if (!((fault_va >= USTACKBOTTOM && fault_va <= USTACKTOP) || (fault_va >= USER_HEAP_START && fault_va <= USER_HEAP_MAX))) {
			env_exit(); // Exit for invalid page
		}
	}


	// Update the working set to point to the next element
	struct WorkingSetElement* rett = env_page_ws_list_create_element(faulted_env, fault_va);
	if (rett != NULL) {
		LIST_INSERT_AFTER(&(faulted_env->page_WS_list), victim_element, rett);
		LIST_REMOVE(&(faulted_env->page_WS_list), victim_element);
		if (rett->prev_next_info.le_next == NULL)
			faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
		else
			faulted_env->page_last_WS_element = rett->prev_next_info.le_next;
	}

}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	//[PROJECT] PAGE FAULT HANDLER WITH BUFFERING
	// your code is here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");
}
