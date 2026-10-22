// SPDX-License-Identifier: GPL-2.0-or-later
// CLI-only observer. Scope it to CPU planning on the calling thread; GPU,
// driver and worker allocations are deliberately outside this measurement.
#include <cstdlib>
#include <new>
#include <stdexcept>
#include "metal_offscreen_test_hooks.h"
namespace {
thread_local bool tracking=false;
thread_local MetalOffscreenAllocationStats totals;
void record(size_t bytes){if(tracking){++totals.calls;totals.bytes+=bytes;}}
}
void metalOffscreenBeginAllocations(){if(tracking)throw std::logic_error("Nested allocation observation");totals={};tracking=true;}
MetalOffscreenAllocationStats metalOffscreenEndAllocations(){tracking=false;return totals;}
void *operator new(size_t bytes){void *p=std::malloc(bytes?bytes:1);if(!p)throw std::bad_alloc();record(bytes);return p;}
void *operator new[](size_t bytes){return ::operator new(bytes);}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete[](void *p) noexcept{std::free(p);}
void operator delete(void *p,size_t) noexcept{std::free(p);}
void operator delete[](void *p,size_t) noexcept{std::free(p);}
void *operator new(size_t bytes,std::align_val_t alignment){void *p=nullptr;if(posix_memalign(&p,size_t(alignment),bytes?bytes:1))throw std::bad_alloc();record(bytes);return p;}
void *operator new[](size_t bytes,std::align_val_t alignment){return ::operator new(bytes,alignment);}
void operator delete(void *p,std::align_val_t) noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t) noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t) noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t) noexcept{std::free(p);}
