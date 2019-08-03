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

#define CTOC_LBA(n) (n)
#define CTOC_ADR(n) (n<<24)
#define CTOC_CTRL(n) (n<<28)
#define CTOC_TRACK(n) (n<<16)

void gdrom_hle_op();
