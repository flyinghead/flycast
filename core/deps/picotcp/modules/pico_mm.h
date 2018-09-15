/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Gustav Janssens, Jonas Van Nieuwenberg, Sam Van Den Berge
 *********************************************************************/


#ifndef _INCLUDE_PICO_MM
#define _INCLUDE_PICO_MM

#include "pico_config.h"

/*
 * Memory init function, this will create a memory manager instance
 * A memory_manager page will be created, along with one page of memory
 * Memory can be asked for via the pico_mem_zalloc function
 * More memory will be allocated to the memory manager according to its needs
 * A maximum amount of memory of uint32_t memsize can be allocated
 */
void pico_mem_init(uint32_t memsize);
/*
 * Memory deinit function, this will free all memory occupied by the current
 * memory manager instance.
 */
void pico_mem_deinit(void);
/*
 * Zero-initialized malloc function, will reserve a memory segment of length uint32_t len
 * This memory will be quickly allocated in a slab of fixed size if possible
 * or less optimally in the heap for a small variable size
 * The fixed size of the slabs can be changed dynamically via a statistics engine
 */
void*pico_mem_zalloc(size_t len);
/*
 * Free function, free a block of memory pointed to by ptr.
 * Unused memory is only returned to the system's control by pico_mem_cleanup
 */
void pico_mem_free(void*ptr);
/*
 * This cleanup function will be provided by the memory manager
 * It can be called during processor downtime
 * This function will return unused pages to the system's control
 * Pages are unused if they no longer contain slabs or heap, and they have been idle for a longer time
 */
void pico_mem_cleanup(uint32_t timestamp);



#ifdef PICO_SUPPORT_MM_PROFILING
/***********************************************************************************************************************
 ***********************************************************************************************************************
   MEMORY PROFILING FUNCTIONS
 ***********************************************************************************************************************
 ***********************************************************************************************************************/
/* General info struct */
struct profiling_data
{
    uint32_t free_heap_space;
    uint32_t free_slab_space;
    uint32_t used_heap_space;
    uint32_t used_slab_space;
};

/*
 * This function fills up a struct with used and free slab and heap space in the memory manager
 * The user is responsible for resource managment
 */
void pico_mem_profile_collect_data(struct profiling_data*profiling_page_struct);

/*
 * This function prints the general structure of the memory manager
 * Printf in this function can be rerouted to send this data over a serial port, or to write it away to memory
 */
void pico_mem_profile_scan_data(void);

/*
 * This function returns the total size that the manager has received from the system
 * This can give an indication of the total system resource commitment, but keep in mind that
 * there can be many free blocks in this "used" size
 * Together with pico_mem_profile_collect_data, this can give a good estimation of the total
 * resource commitment
 */
uint32_t pico_mem_profile_used_size(void);

/*
 * This function returns a pointer to page 0, the main memory manager housekeeping (struct pico_mem_manager).
 * This can be used to collect data about the memory in user defined functions.
 * Use with care!
 */
void*pico_mem_profile_manager(void);

/*
 * paramter manager is a pointer to a struct pico_mem_manager
 */
void pico_mem_init_profiling(void*manager, uint32_t memsize);
#endif /* PICO_SUPPORT_MM_PROFILING */

#endif /* _INCLUDE_PICO_MM */
