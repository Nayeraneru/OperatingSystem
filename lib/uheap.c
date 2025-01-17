#include <inc/lib.h>

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment)
{
	return (void*) sys_sbrk(increment);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================
// Define a structure to manage allocation metadata
#define HEAP_TRACKER_SIZE ((USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE)

struct AllocationEntry {
    uint32 address;   // Start address of the allocation
    uint32 length;    // Size of the allocation in bytes
    bool isActive;    // Indicates if the entry is active (allocated)
    int32 id;
};

struct AllocationEntry heapAllocations[HEAP_TRACKER_SIZE]; // Array to track allocations






// Helper function to add allocation metadata
void addAllocation(uint32 address, uint32 length) {
    for (int i = 0; i < HEAP_TRACKER_SIZE; i++) {
        if (!heapAllocations[i].isActive) { // Find an inactive (free) slot
            heapAllocations[i].address = address;
            heapAllocations[i].length = length;
            heapAllocations[i].isActive = 1;
            return;
        }
    }
}

// Helper function to find allocation metadata
int findAllocation(uint32 address) {
    for (int i = 0; i < HEAP_TRACKER_SIZE; i++) {
        if (heapAllocations[i].isActive && heapAllocations[i].address == address) {
            return i; // Return index of the matching entry
        }
    }
    return -1; // Not found
}

// Function to allocate memory in the user heap
void* malloc(uint32 size)
{
    if (size == 0)
        return NULL;

    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
        return alloc_block_FF(size);

    uint32 numPages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    uint32 startSearch = myEnv->u_hard_limit + PAGE_SIZE;
    uint32 currentAddress = startSearch;

    uint32 potentialStart = 0;
    uint32 foundPages = 0;
    bool isFreeBlock = 0; // Flag to indicate if we've started a free block

    while (currentAddress < USER_HEAP_MAX)
    {

        uint32 pageStatus = sys_get_page_permissions(currentAddress);

        if ((pageStatus & PERM_AVAILABLE) == 0) { // Page is free
            if (!isFreeBlock) {
                potentialStart = currentAddress; // Mark the start of the potential block
                isFreeBlock = 1;  // We are now in a free block
            }

            foundPages++;  // Increase the number of contiguous free pages

            if (foundPages == numPages) {
                // We have found enough contiguous pages
                break;
            }
        } else {
            // If we encounter a used page, reset the search
            isFreeBlock = 0;
            foundPages = 0;  // Reset the number of found pages
        }

        currentAddress += PAGE_SIZE; // Move to the next page
    }

    if (foundPages < numPages) {
        return NULL;  // Could not find enough contiguous pages
    }

    // Allocate memory using the system call
    sys_allocate_user_mem(potentialStart, size);

    // Track the allocation
    addAllocation(potentialStart, size);

    return (void*)potentialStart;
}

// Function to free allocated memory
void free(void* virtual_address)
{
    uint32 address = (uint32)virtual_address;
    uint32 breakPoint = myEnv->u_break;
    uint32 heapStart = myEnv->u_hard_limit + PAGE_SIZE;

    // If within the block allocator range
    if (address >= USER_HEAP_START && address < breakPoint) {
        free_block(virtual_address);
    }
    // If within the page allocator range
    else if (address >= heapStart && address < USER_HEAP_MAX) {
        int index = findAllocation(address);
        if (index == -1) {
            panic("Invalid address: not allocated or already freed!");
        }

        // Free the memory
        sys_free_user_mem(heapAllocations[index].address, heapAllocations[index].length);

        // Mark the entry as inactive
        heapAllocations[index].isActive = 0;
    } else {
        panic("Invalid virtual address!!!");
    }
}


//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable) {


	//cprintf("smalloc\n");
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0)
		return NULL;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code
	//panic("smalloc() is not implemented yet...!!");

	uint32 numPages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint32 startSearch = (uint32)(((char*)myEnv->u_hard_limit) + PAGE_SIZE);
	uint32 currentAddress = startSearch;

	uint32 potentialStart = 0;
	uint32 foundPages = 0;
	bool isFreeBlock = 0; // Flag to indicate if we've started a free block

	while (currentAddress < USER_HEAP_MAX)
	{
		uint32 pageStatus = sys_get_page_permissions(currentAddress);

		if ((pageStatus & PERM_AVAILABLE) == 0) { // Page is free
			if (!isFreeBlock) {
				potentialStart = currentAddress; // Mark the start of the potential block
				isFreeBlock = 1;  // We are now in a free block
			}

			foundPages++;  // Increase the number of contiguous free pages

			if (foundPages == numPages) {
				// We have found enough contiguous pages
				break;
			}
		}
		else {
			// If we encounter a used page, reset the search
			isFreeBlock = 0;
			foundPages = 0;  // Reset the number of found pages
		}
		currentAddress += PAGE_SIZE; // Move to the next page
	}

	if (foundPages < numPages) {
		return NULL;  // Could not find enough contiguous pages
	}

	int32 sysret = sys_createSharedObject(sharedVarName, size, isWritable, (void*) potentialStart);
	if (sysret < 0) {
	    return NULL;
	}


	for (int i = 0; i < HEAP_TRACKER_SIZE; i++) {
		if (heapAllocations[i].address == 0) {
			heapAllocations[i].address = (uint32) potentialStart;
			heapAllocations[i].id = sysret;
			break;
		}
	}


	//cprintf("end of smalloc\n");
	return (void*) potentialStart;

}


//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//cprintf("sget\n");

    // Get size of the shared object
    int size = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);
    if (size == E_SHARED_MEM_NOT_EXISTS) {
        //cprintf("Shared object does not exist.\n");
        return NULL;
    }

    uint32 va = 0;
    bool isFreeBlock = 0;
    int foundPages = 0;
    uint32 numPages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

    // Dynamically adjust startSearch based on existing allocations
    static uint32 nextSearchAddress = 0;
    if (nextSearchAddress == 0) {
        nextSearchAddress = ROUNDUP(myEnv->u_hard_limit, PAGE_SIZE) + PAGE_SIZE;  // Initialize search
    }
    uint32 startSearch = nextSearchAddress;

    //cprintf("Searching from: %x\n", startSearch);
    for (uint32 element = startSearch; element < USER_HEAP_MAX; element += PAGE_SIZE) {
        uint32 pageStatus = sys_get_page_permissions(element);

        if ((pageStatus & PERM_AVAILABLE) == 0) { // Page is free
            if (!isFreeBlock) {
                va = element;  // Potential start of a free block
                isFreeBlock = 1;
            }
            foundPages++;

            if (foundPages == numPages) {
                break; // Found enough contiguous pages
            }
        } else {
            // Reset the search on encountering a used page
            isFreeBlock = 0;
            foundPages = 0;
        }
    }

    if (foundPages < numPages) {
       // cprintf("Not enough pages found.\n");
        return NULL;
    }

    // Finalize VA selection
   // va = ROUNDUP(va, PAGE_SIZE);
    //cprintf("Selected VA: %x, Aligned VA: %x\n", va, ROUNDUP(va, PAGE_SIZE));

    // Update next search address for subsequent requests
    nextSearchAddress = va + (numPages * PAGE_SIZE);

    // Map the shared object to the selected VA
    int ret_obj = sys_getSharedObject(ownerEnvID, sharedVarName, (void*)va);
   // cprintf("sys_getSharedObject returned: %d\n", ret_obj);

    for (int i = 0; i < HEAP_TRACKER_SIZE; i++) {
           if (heapAllocations[i].address == 0) {
               heapAllocations[i].address = (uint32)va;
               heapAllocations[i].id = ret_obj;
               break; // Insert once and exit the loop
           }
       }

    if (ret_obj == E_SHARED_MEM_NOT_EXISTS) {
        return NULL;
    }
	//cprintf("end of sget\n");

    return (void*)va;
}



//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//cprintf("sfree\n");
    uint32 va = (uint32)virtual_address;
    int32 sharedObjectID = 0;

    for (int i = 0; i < HEAP_TRACKER_SIZE; i++)
    {
        if (va == heapAllocations[i].address)
        {
            sharedObjectID = heapAllocations[i].id;
            heapAllocations[i].address = 0; // Mark as free
            heapAllocations[i].id = 0;
            break;
        }
    }

    int result = sys_freeSharedObject(sharedObjectID,virtual_address);

    if (result != 0)
    {
       // cprintf("Failed to free shared object at address: %d\n", result);
    }

	//cprintf("end of sfree\n");

    return;
}






//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.
//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().
//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}

//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
