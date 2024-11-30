#pragma once
#include "serialize.h"
#include "cfg/option.h"

#define SYSCALL_GDROM			0x00

enum gd_entry_points {
	GDROM_REQ_CMD,
	GDROM_GET_CMD_STAT,
	GDROM_EXEC_SERVER,
	GDROM_INIT_SYSTEM,
	GDROM_GET_DRV_STAT,
	GDROM_G1_DMA_END,		// r4: callback, r5: callback arg
	GDROM_REQ_DMA_TRANS,	// r4: request id
	GDROM_CHECK_DMA_TRANS,	// r4: request id, r5: u32 *size
	GDROM_READ_ABORT,
	GDROM_RESET,
	GDROM_CHANGE_DATA_TYPE,
	GDROM_SET_PIO_CALLBACK,	// r4: callback, r5: callback arg
	GDROM_REQ_PIO_TRANS,	// r4: request id
	GDROM_CHECK_PIO_TRANS	// r4: request id, r5: u32 *size
};

typedef enum : u32 {
	GDCC_NONE = 0,
	GDCC_PIOREAD = 0x10,
	GDCC_DMAREAD,
	GDCC_GETTOC,
	GDCC_GETTOC2,
	GDCC_PLAY,
	GDCC_PLAY2,
	GDCC_PAUSE,
	GDCC_RELEASE,
	GDCC_INIT,
	GDCC_READABORT,
	GDCC_OPEN,
	GDCC_SEEK,
	GDCC_DMA_READ_REQ,
	GDCC_GETQINFO,
	GDCC_REQ_MODE,
	GDCC_SET_MODE,
	GDCC_SCAN,
	GDCC_STOP,
	GDCC_GETSCD,
	GDCC_REQ_SES,
	GDCC_REQ_STAT,
	GDCC_PIOREADREQ,
	GDCC_MULTI_DMAREAD,
	GDCC_MULTI_PIOREAD,
	GDCC_GET_VERSION,
	GDCC_CMDA,
	GDCC_CMDB,
	GDCC_CMDC,
	GDCC_CMDD,
	GDCC_CMDE,
	GDCC_CMDF,
	GDCC_CMDG,
	// pseudo-commands for multi dma/pio xfers
	GDCC_REQ_DMA_TRANS = 0x106,
	GDCC_REQ_PIO_TRANS = 0x10C
} gd_command;

enum gdc_wait {
	GDC_WAIT_INTERNAL,
	GDC_WAIT_IRQ
};

enum misc_command {
	MISC_INIT,
	MISC_SETVECTOR
};

void gdrom_hle_op();
void gdrom_hle_reset();

typedef enum : int32_t {
    GDC_ERR = -1,
    GDC_OK,			// Idle
    GDC_BUSY,		// Command being processed
    GDC_COMPLETE,	// Command has finished
    GDC_CONTINUE,	// Data has been transferred, more data available
    GDC_SMPHR_BUSY,
    GDC_RET1,
    GDC_RET2
} gd_return_value;

// status for GDROM_GET_DRV_STAT
enum gd_drv_stat {
	GD_STAT_BUSY,
	GD_STAT_PAUSE,
	GD_STAT_STANDBY,
	GD_STAT_PLAY,
	GD_STAT_SEEK,
	GD_STAT_SCAN,
	GD_STAT_OPEN,
	GD_STAT_NODISC,
	GD_STAT_RETRY,
	GD_STAT_ERROR
};
