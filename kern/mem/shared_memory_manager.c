#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

extern uint32 sys_calculate_free_frames();

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
struct Share* get_share(int32 ownerID, char* name);

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init() {
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list);
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName) {
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//===========================
// [1] Create frames_storage:
//===========================
inline struct FrameInfo** create_frames_storage(int numOfFrames) {
	//cprintf("create_frames_storage\n");

	struct FrameInfo** framesStorage = (struct FrameInfo**) kmalloc(numOfFrames * sizeof(struct FrameInfo*));
	if (framesStorage == NULL) {
		return NULL;
	}
	memset(framesStorage, 0, numOfFrames * sizeof(struct FrameInfo*));
	//cprintf("end of create_frames_storage\n");

	return framesStorage;
}

//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable) {
	//cprintf("create_share\n");


	struct Share* shared_object = (struct Share*) kmalloc(sizeof(struct Share));
	if (shared_object == NULL) {
		return NULL;
	}
	int no_frame = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	struct FrameInfo** reserved_frames = create_frames_storage(no_frame);
	if (reserved_frames == NULL) {
		kfree((void*) shared_object);
		return NULL;
	}
	shared_object->ownerID = ownerID;
	shared_object->isWritable = isWritable;
	strncpy(shared_object->name, shareName, sizeof(shared_object->name) - 1);
	shared_object->name[sizeof(shared_object->name) - 1] = '\0';
	shared_object->framesStorage = reserved_frames;
	shared_object->references = 1;
	shared_object->size = size;
	shared_object->ID = (uint32) shared_object & 0x7FFFFFFF;
	//cprintf("end of create_share\n");

	return shared_object;
}

//=============================
// [3] Search for Share Object:
//=============================
struct Share* get_share(int32 ownerID, char* name) {
	//cprintf("get_share\n");

	acquire_spinlock(&(AllShares.shareslock));

	// Assuming lock should already be held before calling this function
	struct Share* shared_element;
	LIST_FOREACH(shared_element, &AllShares.shares_list) {
		if (shared_element->ownerID == ownerID && strcmp(shared_element->name, name) == 0) {
			release_spinlock(&(AllShares.shareslock));
			//cprintf("end of get_share\n");

			return shared_element;
		}
	}
	release_spinlock(&(AllShares.shareslock));
	return NULL;
}

//=========================
// [4] Create Share Object:
//=========================
int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address) {
    struct Env* myenv = get_cpu_proc();


    if (get_share(ownerID, shareName) != NULL) {
        //release_spinlock(&(AllShares.shareslock));
    	//cprintf("RETURN 1\n");
        return E_SHARED_MEM_EXISTS;
    }
   // acquire_spinlock(&(AllShares.shareslock));

    struct Share* shared_obj = create_share(ownerID, shareName, size, isWritable);
    if (shared_obj == NULL) {
        //release_spinlock(&(AllShares.shareslock));
        return E_NO_SHARE;
    }

    acquire_spinlock(&(AllShares.shareslock));
    LIST_INSERT_TAIL(&(AllShares.shares_list), shared_obj);
    release_spinlock(&(AllShares.shareslock));



    uint32 va = (uint32) virtual_address;
    int num_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

    for (int i = 0; i < num_pages; i++) {
        struct FrameInfo* frame = NULL;
        int ret = allocate_frame(&frame);

        if (ret != 0) {
           /* for (int j = 0; j < i; j++) {
                unmap_frame(myenv->env_page_directory, va - (j * PAGE_SIZE));
            }
            kfree(shared_obj->framesStorage);
            kfree(shared_obj);
            release_spinlock(&(AllShares.shareslock));*/
        	acquire_spinlock(&(AllShares.shareslock));
        	LIST_REMOVE(&(AllShares.shares_list), shared_obj);
        	    release_spinlock(&(AllShares.shareslock));


            return E_NO_SHARE;
        }
        int map_ret = map_frame(myenv->env_page_directory, frame, va, PERM_WRITEABLE | PERM_PRESENT | PERM_USER);

        if (ret != 0) {

        	acquire_spinlock(&(AllShares.shareslock));
        	LIST_REMOVE(&(AllShares.shares_list), shared_obj);
        	release_spinlock(&(AllShares.shareslock));
        	free_frame(frame);

            return E_NO_SHARE;
        }
        shared_obj->framesStorage[i] = frame;
        pt_set_page_permissions(myenv->env_page_directory, va, PERM_AVAILABLE, 0);

        va += PAGE_SIZE;
    }

        return shared_obj->ID;
}


//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address) {

    struct Env* myenv = get_cpu_proc();
    struct Share* share_obj = get_share(ownerID, shareName);
    acquire_spinlock(&(AllShares.shareslock));  // Lock before accessing the list

    if (share_obj == NULL) {
        release_spinlock(&(AllShares.shareslock));  // Unlock if not found
        return E_SHARED_MEM_NOT_EXISTS;
    }

    uint32 numPages = ROUNDUP(share_obj->size, PAGE_SIZE) / PAGE_SIZE;

    for (uint32 i = 0; i < numPages; i++) {
        struct FrameInfo* frame = share_obj->framesStorage[i];
        if (frame == NULL) {
            for (uint32 j = 0; j < i; j++) {
                unmap_frame(myenv->env_page_directory, (uint32) virtual_address + (j * PAGE_SIZE));
            }
            release_spinlock(&(AllShares.shareslock));
            return E_NO_MEM;
        }

        uint32 permissions = PERM_PRESENT | PERM_USER;
        if (share_obj->isWritable) {
            permissions |= PERM_WRITEABLE;
        }

        map_frame(myenv->env_page_directory, frame, (uint32) virtual_address + (i * PAGE_SIZE), permissions);
    }

    share_obj->references++;
    release_spinlock(&(AllShares.shareslock));  // Unlock after finishing

    return share_obj->ID;
}




//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself


void free_share(struct Share* ptrShare)
{
    LIST_REMOVE(&AllShares.shares_list, ptrShare);
    kfree(ptrShare->framesStorage);
    kfree(ptrShare);
}


//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
    struct Env* myenv = get_cpu_proc();
    struct Share* shared_element ;
    uint32 va = (uint32) startVA;

    LIST_FOREACH(shared_element, &AllShares.shares_list)
    {
        if (shared_element->ID == sharedObjectID)
            break;
    }

	if (shared_element == NULL) {
		//cprintf("Shared object not found!\n");
		return E_NO_SHARE; // Shared object not found
	}

    uint32 numPages = ROUNDUP(shared_element->size, PAGE_SIZE) / PAGE_SIZE;
    va = ROUNDDOWN((uint32)startVA, PAGE_SIZE);


    for (uint32 i = 0; i < numPages; i++) {
        uint32 pageVA = va + i * PAGE_SIZE;

        uint32 *page_table = NULL;
        struct FrameInfo *frame = get_frame_info(myenv->env_page_directory, pageVA, &page_table);
        unmap_frame(myenv->env_page_directory, pageVA);

        if (page_table != NULL) {
            bool el_table_fady = 1;

            for (uint32 j = 0; j < 1024; j++) {
                if (page_table[j] & PERM_PRESENT){
                    el_table_fady = 0;
                    break;
                }
            }

            if (el_table_fady) {
                kfree(page_table);
                pd_clear_page_dir_entry(myenv->env_page_directory, pageVA);
            }
        }
    }

    shared_element->references--;
    if (shared_element->references == 0) {
        free_share(shared_element);
    }

    tlbflush();

    return 0;
}

