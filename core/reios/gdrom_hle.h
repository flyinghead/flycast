#pragma once

#define SYSCALL_GDROM			0x00

#define GDROM_SEND_COMMAND		0x00
#define GDROM_CHECK_COMMAND		0x01
#define GDROM_MAIN				0x02
#define GDROM_INIT				0x03
#define GDROM_CHECK_DRIVE		0x04
#define GDROM_G1_DMA_END		0x05	// r4: callback, r5: callback arg
#define GDROM_REQ_DMA_TRANS		0x06	// r4: request id
#define GDROM_CHECK_DMA_TRANS	0x07	// r4: request id, r5: u32 *size
#define GDROM_ABORT_COMMAND		0x08
#define GDROM_RESET				0x09
#define GDROM_SECTOR_MODE		0x0A
#define GDROM_SET_PIO_CALLBACK	0x0B	// r4: callback, r5: callback arg
#define GDROM_REQ_PIO_TRANS		0x0C	// r4: request id
#define GDROM_CHECK_PIO_TRANS	0x0D	// r4: request id, r5: u32 *size

#define GDCC_PIOREAD			0x10
#define GDCC_DMAREAD			0x11
#define GDCC_GETTOC				0x12
#define GDCC_GETTOC2			0x13
#define GDCC_PLAY				0x14
#define GDCC_PLAY_SECTOR		0x15
#define GDCC_PAUSE				0x16
#define GDCC_RELEASE			0x17
#define GDCC_INIT				0x18
#define GDCC_SEEK				0x1b
#define GDCC_READ				0x1c
#define GDCC_REQ_MODE			0x1e
#define GDCC_SET_MODE			0x1f
#define GDCC_STOP				0x21
#define GDCC_GETSCD				0x22
#define GDCC_GETSES				0x23
#define GDCC_REQ_STAT			0x24
#define GDCC_MULTI_DMAREAD		0x26
#define GDCC_MULTI_PIOREAD		0x27
#define GDCC_GET_VER			0x28
// pseudo-commands for multi dma/pio xfers
#define GDCC_REQ_DMA_TRANS	   0x106
#define GDCC_REQ_PIO_TRANS	   0x10C

#define MISC_INIT				0x00
#define MISC_SETVECTOR			0x01

void gdrom_hle_op();

typedef enum { BIOS_ERROR = -1, BIOS_INACTIVE, BIOS_ACTIVE, BIOS_COMPLETED, BIOS_DATA_AVAIL } gd_bios_status;
struct gdrom_hle_state_t
{
	u32 last_request_id;
	u32 next_request_id;
	gd_bios_status status;
	u32 command;
	u32 params[4];
	u32 result[4];
	u32 cur_sector;
	u32 multi_read_sector;
	u32 multi_read_offset;
	u32 multi_read_count;
	u32 multi_read_total;
	u32 multi_callback;
	u32 multi_callback_arg;
	bool dma_trans_ended;
	u64 xfer_end_time;
	
	bool Serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(last_request_id);
		REICAST_S(next_request_id);
		REICAST_S(status);
		REICAST_S(command);
		REICAST_S(params);
		REICAST_S(result);
		REICAST_S(cur_sector);
		REICAST_S(multi_read_sector);
		REICAST_S(multi_read_offset);
		REICAST_S(multi_read_count);
		REICAST_S(multi_read_total);
		REICAST_S(multi_callback);
		REICAST_S(multi_callback_arg);
		REICAST_S(dma_trans_ended);
		REICAST_S(xfer_end_time);
		
		return true;
	}
	bool Unserialize(void **data, unsigned int *total_size)
	{
		REICAST_US(last_request_id);
		REICAST_US(next_request_id);
		REICAST_US(status);
		REICAST_US(command);
		REICAST_US(params);
		REICAST_US(result);
		REICAST_US(cur_sector);
		REICAST_US(multi_read_sector);
		REICAST_US(multi_read_offset);
		REICAST_US(multi_read_count);
		REICAST_US(multi_read_total);
		REICAST_US(multi_callback);
		REICAST_US(multi_callback_arg);
		REICAST_US(dma_trans_ended);
		REICAST_US(xfer_end_time);
		
		return true;
	}
};
extern gdrom_hle_state_t gd_hle_state;

