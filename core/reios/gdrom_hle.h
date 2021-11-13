#pragma once
#include "serialize.h"

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

void gdrom_hle_init();
void gdrom_hle_term();
void gdrom_hle_op();

typedef enum { BIOS_ERROR = -1, BIOS_INACTIVE, BIOS_ACTIVE, BIOS_COMPLETED, BIOS_DATA_AVAIL } gd_bios_status;
struct gdrom_hle_state_t
{
	gdrom_hle_state_t() : params{}, result{} {}

	u32 last_request_id = 0xFFFFFFFF;
	u32 next_request_id = 2;
	gd_bios_status status = BIOS_INACTIVE;
	u32 command = 0;
	u32 params[4];
	u32 result[4];
	u32 cur_sector = 0;
	u32 multi_read_sector = 0;
	u32 multi_read_offset = 0;
	u32 multi_read_count = 0;
	u32 multi_read_total = 0;
	u32 multi_callback = 0;
	u32 multi_callback_arg = 0;
	bool dma_trans_ended = false;
	u64 xfer_end_time = 0;
	
	void Serialize(Serializer& ser)
	{
		ser << last_request_id;
		ser << next_request_id;
		ser << status;
		ser << command;
		ser << params;
		ser << result;
		ser << cur_sector;
		ser << multi_read_sector;
		ser << multi_read_offset;
		ser << multi_read_count;
		ser << multi_read_total;
		ser << multi_callback;
		ser << multi_callback_arg;
		ser << dma_trans_ended;
		ser << xfer_end_time;

	}
	void Deserialize(Deserializer& deser)
	{
		deser >> last_request_id;
		deser >> next_request_id;
		deser >> status;
		deser >> command;
		deser >> params;
		deser >> result;
		deser >> cur_sector;
		deser >> multi_read_sector;
		deser >> multi_read_offset;
		deser >> multi_read_count;
		deser >> multi_read_total;
		deser >> multi_callback;
		deser >> multi_callback_arg;
		deser >> dma_trans_ended;
		deser >> xfer_end_time;
	}
};
extern gdrom_hle_state_t gd_hle_state;

