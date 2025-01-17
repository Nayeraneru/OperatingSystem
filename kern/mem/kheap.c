#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"
#include <kern/conc/sleeplock.h>

uint32 START = 0;
uint32 HARD_LIMIT = 0;
uint32 BREAK = 0;

extern uint32 sys_calculate_free_frames() ;

//struct sleeplock k_lock;
struct spinlock f_lock;

#define MAX_PAGES (1 << 20)  // Maximum number of pages

struct PageInfo {
    uint32 startAddress;
    uint32 allocatedSize;
};

struct
{
	struct PageInfo pageAllocations[MAX_PAGES];
	//struct spinlock pllock;
} locky;



//Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//	On success: 0
//	Otherwise (if no memory OR initial size exceed the given limit): PANIC
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
    cprintf("inti %u, limit %u, break %u\n", daStart, HARD_LIMIT, BREAK);

    init_spinlock(&f_lock, "array lock");
    //init_sleeplock(&k_lock, "k_heap lock");

    if(daStart >= daLimit)
        panic("initial size exceeds the given limit\n");

    START = daStart;
    HARD_LIMIT = daLimit;
    struct FrameInfo *ptr_frame_info = NULL;
    int no_of_frames = ROUNDUP((uint32)(initSizeToAllocate / PAGE_SIZE), (uint32)PAGE_SIZE);
    uint32 st;

    for(int i = 0; i < no_of_frames; i++){//protect?
        allocate_frame(&ptr_frame_info);
        if(i == 0)
            st = daStart;
        else
            st += PAGE_SIZE;

        map_frame(ptr_page_directory, ptr_frame_info, st, PERM_WRITEABLE | PERM_PRESENT);
        ptr_frame_info->va = st;
    }

    BREAK = daStart + initSizeToAllocate;

    if(BREAK >= HARD_LIMIT)
        panic("not enough memory\n");

//        uint32 array_list_size = MAX_PAGES_ARRAY * sizeof(struct BlockElement*);
//         ArrayBlocksList = (struct BlockElement**) kmalloc(array_list_size);
//
//        if (ArrayBlocksList == NULL)
//            panic("Memory allocation for ArrayBlocksList failed\n");
//
//        for (int i = 0; i < MAX_PAGES_ARRAY; i++) {
//            ArrayBlocksList[i] = NULL;
//        }
    initialize_dynamic_allocator(START, initSizeToAllocate);

    return 0;
}

void* sbrk(int numOfPages) {
    uint32 old_break = BREAK;
    uint32 increment = numOfPages * PAGE_SIZE;

    // Check if numOfPages is valid
    if (numOfPages > 0) {
        if (old_break + increment > HARD_LIMIT) {
            return (void*)-1;
        }

        uint32 new_break = old_break + increment;

        for (int i = 0; i < numOfPages; i++) {//protect?
            struct FrameInfo *frame = NULL;
            int ret = allocate_frame(&frame);

            if (ret != 0 || frame == NULL) {
                return (void*)-1;
            }

            uint32 virtual_address = old_break + (i * PAGE_SIZE);

            uint32 *page_table = NULL;
            get_page_table(ptr_page_directory, virtual_address, &page_table);

            if (page_table && (page_table[PTX(virtual_address)] & PERM_PRESENT)) {
                continue;  // Skip this page and continue with the next
            }

            // Mapping the frame
            int nrt = map_frame(ptr_page_directory, frame, virtual_address, PERM_WRITEABLE | PERM_PRESENT);
            frame->va = virtual_address;
            if (nrt == 0) {
                //cprintf("Successfully mapped frame to VA %u\n", virtual_address);
            } else {
                //cprintf("Mapping failed with error code %d\n", nrt);
            }

            get_page_table(ptr_page_directory, virtual_address, &page_table);
            if (page_table) {
                //cprintf("PTE after mapping: %x\n", page_table[PTX(virtual_address)]);
            } else {
                //cprintf("No page table found for VA %u after mapping\n", virtual_address);
            }

            // Free frames check

            if (nrt != 0) {
                if (nrt == E_NO_MEM) {
                    //cprintf("sbrk: Mapping failed due to insufficient memory\n");
                }
                return (void*)-1;
            }

          }

        BREAK = new_break;
        return (void*)old_break;

    } else if (numOfPages == 0) {
         return (void*)old_break;
    }

    return (void*)-1;
}



//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

void* kmalloc(uint32 size) {
	acquire_spinlock(&f_lock);

	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		void *va = alloc_block_FF(size);
		release_spinlock(&f_lock);
		return va;
	}

	uint32 pageCount = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint32 startVA = 0;
	uint32 freeCount = 0;
	bool isTracking = 0;

	for (uint32 va = HARD_LIMIT + PAGE_SIZE; va < KERNEL_HEAP_MAX; va +=PAGE_SIZE) {
		uint32* pageTable = NULL;

		if (get_frame_info(ptr_page_directory, va, &pageTable) == 0) {
			if (!isTracking) {
				startVA = va;
				isTracking = 1;
			}
			freeCount++;

			if (freeCount == pageCount) {
				break;
			}
		}
		else {
			isTracking = 0;
			freeCount = 0;
		}
	}

	if (!startVA || freeCount < pageCount) {
		release_spinlock(&f_lock);
		return NULL;
	}

	uint32 pageIndex = (startVA - (HARD_LIMIT + PAGE_SIZE)) >> 12;

	locky.pageAllocations[pageIndex].allocatedSize = size;
	locky.pageAllocations[pageIndex].startAddress = startVA;

	for (uint32 offset = 0; offset < pageCount; ++offset) {
		struct FrameInfo* frame = NULL;
		if (allocate_frame(&frame) == E_NO_MEM) {
			for (uint32 rollbackOffset = 0; rollbackOffset < offset; ++rollbackOffset) {
				unmap_frame(ptr_page_directory,startVA + (rollbackOffset * PAGE_SIZE));
			}
			release_spinlock(&f_lock);
			return NULL;
		}

		uint32 va = startVA + (offset * PAGE_SIZE);
		frame->va = va;

		if (map_frame(ptr_page_directory, frame, va, PERM_WRITEABLE) == E_NO_MEM) {
			for (uint32 rollbackOffset = 0; rollbackOffset <= offset; ++rollbackOffset) {
				unmap_frame(ptr_page_directory,startVA + (rollbackOffset * PAGE_SIZE));
			}
			release_spinlock(&f_lock);
			return NULL;
		}
	}

	release_spinlock(&f_lock);
	return (void*) startVA;
}

void kfree(void* virtualAddress) {
	acquire_spinlock(&MemFrameLists.mfllock);

	uint32 va = (uint32) virtualAddress;
	if (!(va >= HARD_LIMIT && va < KERNEL_HEAP_MAX) && !(va < HARD_LIMIT && va >= START)) {
		panic("Invalid address passed to kfree()");
	}

	if (va >= HARD_LIMIT && va < KERNEL_HEAP_MAX) { // Page allocator case
		uint32 pageIndex = (va - (HARD_LIMIT + PAGE_SIZE)) >> 12;
		uint32 size = locky.pageAllocations[pageIndex].allocatedSize;

		if (size == 0) {
			panic("Invalid or already freed address passed to kfree()");
		}

		uint32 pageCount = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

		locky.pageAllocations[pageIndex].allocatedSize = 0;
		locky.pageAllocations[pageIndex].startAddress = 0;

		uint32 baseVA = (uint32) virtualAddress;
		for (uint32 offset = 0; offset < pageCount; ++offset) {
			unmap_frame(ptr_page_directory, baseVA + (offset * PAGE_SIZE));
		}
	} else { // Block allocator case
		free_block(virtualAddress);
	}

	release_spinlock(&MemFrameLists.mfllock);
}



unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address
		// Write your code here, remove the panic and write your code
	   //	panic("kheap_physical_address() is not implemented yet...!!");
    acquire_spinlock(&f_lock);
	    uint32 *ptr_page_table=NULL;
	    struct FrameInfo *info_framo_ptr = get_frame_info(ptr_page_directory,(uint32)virtual_address,
	    		&ptr_page_table);
	    if (info_framo_ptr == NULL)
	    {
	         release_spinlock(&f_lock);
	    	return 0;
	    }

		uint32 entry=ptr_page_table[PTX(virtual_address)];
		uint32 f_no = entry>>12;
		uint32 offset=virtual_address & 0xfff;
		uint32 phys_address= (f_no*PAGE_SIZE)+offset;
        release_spinlock(&f_lock);
		return phys_address;

}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_virtual_address() is not implemented yet...!!");

	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
	struct FrameInfo *ptr_frame_info = NULL;
	ptr_frame_info = to_frame_info(physical_address);
	uint32 offset = physical_address & 0xfff;
	if (ptr_frame_info->va != 0) {
		uint32 va = ptr_frame_info->va + offset;
		return va;
	}
	return 0;
}





//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

#define BLOCK_ALIGNMENT 4

void* krealloc(void* virtual_address, uint32 new_size) {
    // Case 1: If virtual_address is NULL, act as kmalloc.
    if (virtual_address == NULL) {
        return kmalloc(new_size);
    }

    // Case 2: If new_size is 0, act as kfree and return NULL.
    if (new_size == 0) {
        kfree(virtual_address);
        return NULL;
    }

    uint32 va = (uint32)virtual_address;

    // Case 3: Handle Block Allocator
    if (va < HARD_LIMIT && va >= START) {
        uint32 current_block_size = get_block_size(virtual_address);
        uint32 aligned_new_size = ROUNDUP(new_size, BLOCK_ALIGNMENT);

        if (aligned_new_size == current_block_size) {
            return virtual_address;
        } else if (aligned_new_size < current_block_size) {
            realloc_block_FF(virtual_address, new_size);
            return virtual_address;
        } else {
            void* new_block = realloc_block_FF(virtual_address, new_size);
            if (new_block != NULL) {
                return new_block;
            } else {
                new_block = kmalloc(new_size);
                if (new_block != NULL) {
                    memcpy(new_block, virtual_address, current_block_size - 2 * sizeof(uint32));
                    kfree(virtual_address);
                }
                return new_block;
            }
        }
    }

    // Case 4: Handle Page Allocator
    else if (va >= (HARD_LIMIT + PAGE_SIZE) && va < KERNEL_HEAP_MAX) {
        uint32 pageIndex = (va - (uint32)(void*)(HARD_LIMIT + PAGE_SIZE)) >> 12;
        uint32 current_size = locky.pageAllocations[pageIndex].allocatedSize;

        if (new_size == current_size) {
            return virtual_address;
        }

        uint32 current_pages = ROUNDUP(current_size, PAGE_SIZE) / PAGE_SIZE;
        uint32 new_pages = ROUNDUP(new_size, PAGE_SIZE) / PAGE_SIZE;

        if (new_pages <= current_pages) {
            uint32 shrink_size = current_size - new_size;
            void* shrink_start = (void*)((uint32)virtual_address + new_size);

            if (shrink_size >= PAGE_SIZE) {
                uint32 shrink_pages = shrink_size / PAGE_SIZE;
                uint32 baseVA = (uint32)shrink_start;

                for (uint32 offset = 0; offset < shrink_pages; ++offset) {
                    unmap_frame(ptr_page_directory, baseVA + (offset * PAGE_SIZE));
                }
            }

            locky.pageAllocations[pageIndex].allocatedSize = new_size;
            locky.pageAllocations[pageIndex].startAddress = (uint32)virtual_address;

            return virtual_address;
        } else {
            // Find free pages
            uint32 startVA = 0;
            uint32 freeCount = 0;
            bool isTracking = 0;

            // Scan from HARD_LIMIT + PAGE_SIZE onwards for contiguous free pages
            for (uint32 va = HARD_LIMIT + PAGE_SIZE; va < KERNEL_HEAP_MAX; va += PAGE_SIZE) {
                uint32* pageTable = NULL;

                if (get_frame_info(ptr_page_directory, va, &pageTable) == 0) {
                    if (!isTracking) {
                        startVA = va;
                        isTracking = 1;
                    }
                    freeCount++;

                    if (freeCount == new_pages - current_pages) {
                        break;
                    }
                } else {
                    isTracking = 0;
                    freeCount = 0;
                }
            }

            if (freeCount == new_pages - current_pages) {
                // Allocate contiguous pages
                uint32 baseVA = (uint32)startVA;

                for (uint32 offset = 0; offset < new_pages - current_pages; ++offset) {
                    struct FrameInfo* frame = NULL;

                    if (allocate_frame(&frame) == E_NO_MEM) {
                        for (uint32 dealloc = 0; dealloc < offset; ++dealloc) {
                            uint32 va = baseVA + (dealloc * PAGE_SIZE);
                            unmap_frame(ptr_page_directory, va);
                        }
                        return NULL;
                    }

                    uint32 va = baseVA + (offset * PAGE_SIZE);
                    frame->va = va;

                    if (map_frame(ptr_page_directory, frame, va, PERM_WRITEABLE) == E_NO_MEM) {
                        for (uint32 dealloc = 0; dealloc < offset; ++dealloc) {
                            uint32 va = baseVA + (dealloc * PAGE_SIZE);
                            unmap_frame(ptr_page_directory, va);
                        }
                        return NULL;
                    }
                }

                void* extended_va = (void*)startVA;
                memcpy(extended_va, virtual_address, current_size);
                locky.pageAllocations[pageIndex].allocatedSize = new_size;
                locky.pageAllocations[pageIndex].startAddress = (uint32)extended_va;

                return extended_va;
            } else {
                void* new_va = kmalloc(new_size);
                if (new_va != NULL) {
                    memcpy(new_va, virtual_address, current_size);
                    kfree(virtual_address);
                }
                return new_va;
            }
        }
    }

    panic("Invalid address passed to krealloc()");
    return NULL;
}



