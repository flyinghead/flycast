#include <errno.h>
#include <reent.h>
#include <vitasdk.h>
#include <kubridge.h>

int _newlib_heap_memblock;
unsigned _newlib_heap_size;
char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
static char _newlib_sbrk_mutex[32] __attribute__ ((aligned (8)));

SceUID vm_memblock;

extern int _newlib_heap_size_user __attribute__((weak));

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

void * _sbrk_r(struct _reent *reent, ptrdiff_t incr) {
	if (sceKernelLockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1, 0) < 0)
		goto fail;
	if (!_newlib_heap_base || _newlib_heap_cur + incr >= _newlib_heap_end) {
		sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
fail:
		reent->_errno = ENOMEM;
		return (void*)-1;
	}

	char *prev_heap_end = _newlib_heap_cur;
	_newlib_heap_cur += incr;

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
	return (void*) prev_heap_end;
}

void _init_vita_heap(void) {

	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)0x8290B000;
	vm_memblock = kuKernelAllocMemBlock("code", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 16 * 1024 * 1024, &opt);

	// Create a mutex to use inside _sbrk_r
	if (sceKernelCreateLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, "sbrk mutex", 0, 0, 0) < 0) {
		goto failure;
	}
	
	// Always allocating the max avaliable USER_RW mem on the system
	SceKernelFreeMemorySizeInfo info;
	info.size = sizeof(SceKernelFreeMemorySizeInfo);
	sceKernelGetFreeMemorySize(&info);

	if (&_newlib_heap_size_user != NULL) {
		_newlib_heap_size = _newlib_heap_size_user;
	}

	_newlib_heap_memblock = sceKernelAllocMemBlock("Newlib heap", 0x0c20d060, _newlib_heap_size, 0);
	if (_newlib_heap_memblock < 0) {
		goto failure;
	}
	if (sceKernelGetMemBlockBase(_newlib_heap_memblock, (void**)&_newlib_heap_base) < 0) {
		goto failure;
	}
	_newlib_heap_end = _newlib_heap_base + _newlib_heap_size;
	_newlib_heap_cur = _newlib_heap_base;

	return;
failure:
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}

void _free_vita_heap(void) {

	// Destroy the sbrk mutex
	sceKernelDeleteLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex);

	// Free the heap memblock to avoid memory leakage.
	sceKernelFreeMemBlock(_newlib_heap_memblock);

	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}
