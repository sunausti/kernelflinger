/*
 * Copyright (c) 2024, Intel Corporation
 * All rights reserved.
 *
 * Authors: Austin Sun <austin.sun@intel.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "fatfs.h"
#include "gpt.h"

static FAT_FS g_fatfs;

EFI_STATUS fat_readdisk(INT64 offset, UINT64 len, void *data)
{
    struct fat_fs *fs= &g_fatfs;
    EFI_STATUS ret = EFI_SUCCESS;
	debug(L"fat_readdisk offset: 0x%x, len: 0x%x", offset, len);
	if (fs == NULL || fs->parti.dio == NULL || fs->parti.bio == NULL)
	{
	    debug(L"fat_readdisk init fail");
	    return EFI_INVALID_PARAMETER;
	}
	//ret = fs->dio->ReadDisk(fs->dio, fs->bio->Media->MediaId, offset, len, data);
	ret = uefi_call_wrapper(fs->parti.dio->ReadDisk, 5, fs->parti.dio,
				fs->parti.bio->Media->MediaId,
				offset, len, (VOID *)data);

	if (EFI_ERROR(ret))
        efi_perror(ret, L"fat read failed");
    return ret;
}

EFI_STATUS fat_writedisk( INT64 offset, UINT64 len, void *data)
{
    struct fat_fs *fs= &g_fatfs;
    EFI_STATUS ret = EFI_SUCCESS;
	if (fs == NULL || fs->parti.bio == NULL)
	{
	    debug(L"fat_writedisk init fail");
	    return EFI_INVALID_PARAMETER;
	}
	//ret = fs->dio->WriteDisk(fs->dio, fs->bio->Media->MediaId, offset, len, data);
	ret = uefi_call_wrapper(fs->parti.dio->WriteDisk, 5, fs->parti.dio, fs->parti.bio->Media->MediaId,offset,len,data);
	if (EFI_ERROR(ret))
        efi_perror(ret, L"fat write failed");
    return ret;
}
EFI_STATUS fat_init() 
{
    struct fat_fs *fs= &g_fatfs;
    UINT8 bpb_sec[512];
    EFI_STATUS ret = EFI_SUCCESS;
    EFI_GUID guid;
    ret = gpt_get_partition_by_label(PRIMARY_LABEL, &fs->parti, LOGICAL_UNIT_USER);
	if (EFI_ERROR(ret)) {
		efi_perror(ret, L"Failed to get disk information");
		return ret;
	}
	guid = fs->parti.part.type;
	debug(L"the guid(%x-%x-%x) is matched",guid.Data1,guid.Data2,guid.Data3 );
	if (0 == CompareGuid(&fs->parti.part.type, &EfiPartTypeSystemPartitionGuid)) {
	    debug(L"the guid is matched");
	}else {
	    error(L"can not find Efi system partition");
	}
    fs->bpb_offset = fs->parti.part.starting_lba*512;
    ret = fat_readdisk(fs->bpb_offset,512,bpb_sec);
	if (EFI_ERROR(ret)) {
		efi_perror(ret, L"Failed to get FAT BPB");
		return ret;
	}
    
    fs->BytsPerSec=UINT8to16(bpb_sec[12],bpb_sec[11]);
    fs->SecPerClus=bpb_sec[13];
	fs->RsvdSecCnt=UINT8to16(bpb_sec[15],bpb_sec[14]);
	fs->NumFATs = bpb_sec[16];
	fs->RootEntCnt = UINT8to16(bpb_sec[18],bpb_sec[17]);
    debug(L"first 4 %x,%x,%x,%x",fs->BytsPerSec,fs->RsvdSecCnt,fs->NumFATs,fs->RootEntCnt);
    fs->TotSec16 = UINT8to16(bpb_sec[20],bpb_sec[19]);
    fs->FATSz16 = UINT8to16(bpb_sec[23],bpb_sec[22]);
	fs->TotSec32 = UINT8to32(bpb_sec[35],bpb_sec[34],bpb_sec[33],bpb_sec[32]);
    fs->FATSz32 = UINT8to32(bpb_sec[39],bpb_sec[38],bpb_sec[37],bpb_sec[36]);
    debug(L" Second 4 %x,%x,%x,%x",fs->TotSec16 ,fs->FATSz16,fs->TotSec32,fs->FATSz32);

    fs->RootClus = UINT8to32(bpb_sec[47],bpb_sec[46],bpb_sec[45],bpb_sec[44]);
    fs->FSInfo = UINT8to16(bpb_sec[49],bpb_sec[48]);
    fs->BkBootSec = UINT8to16(bpb_sec[51],bpb_sec[50]);
    fs->RootDirSectors = ((fs->RootEntCnt * 32) + (fs->BytsPerSec -1))/ fs->BytsPerSec;
    debug(L" Third 4 %x,%x,%x,%x",fs->RootClus,fs->FSInfo,fs->BkBootSec,fs->RootDirSectors );
    if( fs->FATSz16 != 0 ) {
        fs->FATSz = fs->FATSz16;
    } else {
        fs->FATSz = fs->FATSz32;
    }
    if( fs->TotSec16 != 0 ) {
        fs->TotSec = fs->TotSec16;
    } else {
        fs->TotSec = fs->TotSec32;
    }
    fs->DataSec = fs->TotSec - ((UINT32)fs->RsvdSecCnt + (fs->NumFATs*fs->FATSz) + (UINT32)fs->RootDirSectors);
    debug(L" DataSec: %x",fs->DataSec);
    fs->CountofClusters = fs->DataSec / fs->SecPerClus;
    debug(L"CountofClusters : %x",fs->CountofClusters );
    if(fs->CountofClusters < 4085) {
        efi_perror(ret, L"fat12 is not supported");
        fs->FilSysType = FAT12;
    } else if(fs->CountofClusters < 65525) {
        fs->FilSysType = FAT16;
        debug(L"FAT16 System");
    } else {
        efi_perror(ret, L"fat32 is not supported");
        fs->FilSysType = FAT32;
    }
    fs->fat_offset = fs->bpb_offset + fs->RsvdSecCnt*fs->BytsPerSec;
    fs->root_offset = fs->bpb_offset + ((UINT32)fs->RsvdSecCnt + fs->NumFATs*fs->FATSz)*fs->BytsPerSec;
    fs->data_offset = fs->root_offset + fs->RootDirSectors*fs->BytsPerSec;
    debug(L"fat offset: %x, root_offset: %x, data_offset: %x",fs->fat_offset, fs->root_offset, fs->data_offset);
    return ret;
}

VOID debug_hex(UINT32 offset, CHAR8 *data, UINT16 size){
    UINT16 i;
	for(i = 0; i < size/8;i++) {
	    debug(L"%x   %x,%x,%x,%x,%x,%x,%x,%x",offset+i*8,data[i*8],
            data[i*8+1],data[i*8+2],data[i*8+3],data[i*8+4],
            data[i*8+5],data[i*8+6],data[i*8+7]);
	}
}
