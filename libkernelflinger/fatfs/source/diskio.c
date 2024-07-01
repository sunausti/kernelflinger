/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "log.h"
#include "gpt.h"
#include "fatfs.h"
/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */
#define DEV_NVME    4	/* Example: Map USB MSD to physical drive 2 */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT ;

     debug(L"disk_status %d, pdrv %d", __LINE__, pdrv);
	switch (pdrv) {
	case DEV_NVME:
	    return 0x0;
	default:
		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    debug(L"disk_init %d, pdrv %d", __LINE__, pdrv);
	switch (pdrv) {	
	case DEV_NVME:
	    return 0x0;
	default:
		return STA_NODISK;

	}
	return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
static void debugdumphex(UINT32 offset, CHAR8 *data, UINT16 size){
    UINT16 i;
	for(i = 0; i < size/8;i++) {
	    debug(L"%x   %x,%x,%x,%x,%x,%x,%x,%x",offset+i*8,data[i*8],
            data[i*8+1],data[i*8+2],data[i*8+3],data[i*8+4],
            data[i*8+5],data[i*8+6],data[i*8+7]);
	}
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT ret = RES_OK;
    UINT32 offset = 0;
	offset += sector;
	offset = offset * 512;
    offset += fat_getbpb_offset();
    debug(L"disk_read %d, pdrv %d sector %d, count %d, offset %d, bpb: %d", __LINE__, pdrv, sector, count, offset, fat_getbpb_offset());
	switch (pdrv) {
	case DEV_NVME:
	    fat_readdisk(offset,count*512,buff);
        debugdumphex(0, buff, 512);
	    return ret;
	default:
		return ret;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
    UINT32 offset = 0;
	offset += sector;
	offset = offset * 512;
    offset += fat_getbpb_offset();
    debug(L"disk_write %d, pdrv %d sector %d, count %d, offset %d, bpb: %d", __LINE__, pdrv, sector, count, offset, fat_getbpb_offset());

	switch (pdrv) {
	case DEV_NVME:
        fat_writedisk(offset,count*512,(void *)buff);
	    return 0x0;
	default:
		return RES_PARERR;
	}


	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT ret = RES_OK;
    if (cmd != 0) {
	   debug(L"%x", (CHAR8 *)buff);
	   return ret;
	}
	switch (pdrv) {
    case DEV_NVME:
		return ret;
	}

	return RES_PARERR;
}

