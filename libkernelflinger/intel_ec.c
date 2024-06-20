#include <efi.h>
#include <efilib.h>
#include <ui.h>
#include "lib.h"
#include "timer.h"

#define SPICmd_WRSR            0x01
#define SPICmd_PageProgram     0x02
#define SPICmd_READ            0x03
#define SPICmd_WRDI            0x04
#define SPICmd_RDSR            0x05
#define SPICmd_WREN            0x06
#define SPICmd_FastRead        0x0B
#define SPICmd_ChipErase       0xC7
#define SPICmd_JEDEC_ID        0x9F 
#define SPICmd_EWSR            0x50
#define SPICmd_1KSectorErase   0xD7
#define SPICmd_AAIBytePro      0xAF
#define SPICmd_AAIWordPro      0xAD
#define SPICmd_VERSION         0x80

#define PM_OBF                 0x01
#define PM_IBF                 0x02
#define RETRY_INTERVAL_US      100000
#define WAIT_INTERVAL_US       3000000
#define _Enterfollow_mode      0x01
#define _Exitfollow_mode       0x05
#define _ENTER_flash_mode      0xDC
#define _EXIT_flash_mode       0xFC
#define _SendCmd               0x02
#define _SendByte              0x03
#define _ReadByte              0x04

static UINT8 PM_STATUS_PORT66    = 0x66;
static UINT8 PM_CMD_PORT66       = 0x66;
static UINT8 PM_DATA_PORT62      = 0x62;
static UINT8 SPIFlashID[5];
static UINT8 *str1, *str2, *str3, *str4;
static UINT8  FW_Size = 128;

static inline UINT8 inb(int port)
{
	UINT8 val;
	__asm__ __volatile__("inb %w1, %b0" : "=a"(val) : "Nd"(port));
	return val;
}

static inline void outb(UINT8 val, int port)
{
	__asm__ __volatile__("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static void Wait_PM_OBF(void)
{
	int retries = 100;

	register unsigned char status = inb(PM_STATUS_PORT66);

	while (!(status & PM_OBF) && retries > 0) {
		pause_us(5000);
		retries--;
		status = inb(PM_STATUS_PORT66);
	}
}

static void Wait_PM_IBE (void)
{
	int retries = 100;

	register unsigned char status = inb(PM_STATUS_PORT66);

	while ((status & PM_IBF) && retries > 0) {
		pause_us(5000);
		retries--;
		status = inb(PM_STATUS_PORT66);
	}
}

static void Send_cmd_by_PM(UINT8 Cmd)
{
	Wait_PM_IBE();
	outb(Cmd, PM_CMD_PORT66);
	Wait_PM_IBE();
}

static UINT8 Read_data_from_PM(void)
{
	register unsigned char data;
	Wait_PM_OBF();
	data = inb(PM_DATA_PORT62);
	return(data);
}

static void follow_mode(UINT8 mode)
{
	Send_cmd_by_PM(mode);
}

static void send_cmd_to_flash(UINT8 cmd)
{
	Send_cmd_by_PM(_SendCmd);
	Send_cmd_by_PM(cmd);
}

static void send_byte_to_flash(UINT8 data)
{
	Send_cmd_by_PM(_SendByte);
	Send_cmd_by_PM(data);
}

static UINT8 read_byte_from_flash(void)
{
	Send_cmd_by_PM(_ReadByte);
	return(Read_data_from_PM());
}

static void wait_flash_free(void)
{
	int retries = 10;
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_RDSR);
	while ((read_byte_from_flash() & 0x01) && retries > 0) {
		retries--;
		pause_us(RETRY_INTERVAL_US);
	}

	follow_mode(_Exitfollow_mode);
}

static void FlashWriteEnable(void)
{
	int retries = 10;
	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_WRSR);
	send_byte_to_flash(0x00);

	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_WREN);

	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_RDSR);
	while (!(read_byte_from_flash() & 0x02) && retries > 0) {
		retries--;
		pause_us(RETRY_INTERVAL_US);
	}
	follow_mode(_Exitfollow_mode);
}

static void flash_write_disable(void)
{
	int retries = 10;
	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_WRDI);

	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_RDSR);
	while ((read_byte_from_flash() & 0x02) && retries > 0) {
		retries--;
		pause_us(RETRY_INTERVAL_US);
	}

	follow_mode(_Exitfollow_mode);
}

static void FlashStatusRegWriteEnable(void)
{
	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_WREN);

	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_EWSR);

}

static void Read_SPIFlash_JEDEC_ID(void)
{
	UINT8 index;

	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_JEDEC_ID);
	for(index=0x00;index<3;index++)
	{
		SPIFlashID[index]=read_byte_from_flash();
	}
	follow_mode(_Exitfollow_mode);
}

static void Block_1K_Erase(UINT8 addr2,UINT8 addr1,UINT8 addr0)
{
	FlashStatusRegWriteEnable();
	FlashWriteEnable();

	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_1KSectorErase);
	send_byte_to_flash(addr2);
	send_byte_to_flash(addr1);
	send_byte_to_flash(addr0);
	wait_flash_free();
}

static void ITE_eFlash_Erase(void)
{
	UINT16 i,j;
	debug(L"   Eraseing...      : ");

	for(i=0; i<0x02; i++) {
		for(j=0; j<0x100; j+=0x04) {
			Block_1K_Erase((UINT8)i, (UINT8)j, 0x00);
			if(0 == j%0x8)
				debug(L"#");
		}
	}

	debug(L"   -- Erase OK. \n\n");
}

static UINT8 ITE_eFlash_Erase_Verify(void)
{
	UINT16 counter;
	UINT8 Dat;
	UINT8 i;
	debug(L"   Erase Verify...  : ");

	flash_write_disable();
	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_FastRead);
	send_byte_to_flash(0x00);   // addr[24:15]
	send_byte_to_flash(0x00);   // addr[8:14]
	send_byte_to_flash(0x00);   // addr[0:7]
	send_byte_to_flash(0x00);   // fast read dummy byte

	for(i=0; i<4; i++) {
		for(counter=0x0000;counter<0x8000;counter++) {
			Dat=read_byte_from_flash();
			if(Dat!=0xFF) {
				wait_flash_free();
				error(L" Dat is: %d \n",Dat);
				error(L" Counter is: %d \n",counter);
				error(L" Block is: %d \n",i);
				error(L" -- Verify Fail. \n");
				return(FALSE);
			}
			if(0 == counter%0x800)
				debug(L"#");
		}
	}

	wait_flash_free();
	debug(L"   -- Verify OK. \n\n");
	return(TRUE);
}

static int ec_erase_and_verify(void)
{
	debug(L"EC: erase ec\n");
	int retry_count = 3;

	while (retry_count > 0) {
		ITE_eFlash_Erase();
		if (ITE_eFlash_Erase_Verify() == TRUE)
			break;

		retry_count--;
	}

	if (retry_count == 0) {
		error(L"EC: ec erase failed!\n");
		return -1;
	}
	return 0;
}


static void eFlash_Program(void)
{
	UINT16 i;
	debug(L"   Programing...    : ");

	FlashStatusRegWriteEnable();
	FlashWriteEnable();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_AAIWordPro);
	send_byte_to_flash(0x00);
	send_byte_to_flash(0x00);
	send_byte_to_flash(0x00);
	send_byte_to_flash(str1[0]);
	send_byte_to_flash(str1[1]);
	wait_flash_free();

	debug(L"#");
	for(i=2; i<0x8000; i+=2) // First tow byte already to to flash
	{
		follow_mode(_Enterfollow_mode);
		send_cmd_to_flash(SPICmd_AAIWordPro);
		send_byte_to_flash(str1[i]);
		send_byte_to_flash(str1[i+1]);
		if(0 == i%0x800)
			debug(L"#");
	}

	for(i=0; i<0x8000; i+=2)
	{
		follow_mode(_Enterfollow_mode);
		send_cmd_to_flash(SPICmd_AAIWordPro);
		send_byte_to_flash(str2[i]);
		send_byte_to_flash(str2[i+1]);
		wait_flash_free();
		if(0 == i%0x800)
			debug(L"#");
	}

	for(i=0; i<0x8000; i+=2)
	{
		follow_mode(_Enterfollow_mode);
		send_cmd_to_flash(SPICmd_AAIWordPro);
		send_byte_to_flash(str3[i]);
		send_byte_to_flash(str3[i+1]);
		wait_flash_free();
		if(0 == i%0x800)
			debug(L"#");
	}

	for(i=0; i<0x8000; i+=2)
	{
		follow_mode(_Enterfollow_mode);
		send_cmd_to_flash(SPICmd_AAIWordPro);
		send_byte_to_flash(str4[i]);
		send_byte_to_flash(str4[i+1]);
		wait_flash_free();
		if(0 == i%0x800)
			debug(L"#");
	}

	flash_write_disable();
	debug(L"   -- Programing OK. \n\n");
}

UINT8 Program_Flash_Verify(void)
{
	UINT16 counter;

	debug(L"   Program Verify...: ");

	flash_write_disable();
	wait_flash_free();
	follow_mode(_Enterfollow_mode);
	send_cmd_to_flash(SPICmd_FastRead);
	send_byte_to_flash(0x00);   // addr[24:15]
	send_byte_to_flash(0x00);   // addr[8:14]
	send_byte_to_flash(0x00);   // addr[0:7]
	send_byte_to_flash(0x00);   // fast read dummy byte

	for(counter=0x0000;counter<0x8000;counter++)
	{
		if(read_byte_from_flash()!=str1[counter])
		{
			wait_flash_free();
			error(L" -- Verify Fail. \n");
			return(FALSE);
		}
		if(0 == counter%0x800)
			debug(L"#");
	}

	for(counter=0x0000;counter<0x8000;counter++)
	{
		if(read_byte_from_flash()!=str2[counter])
		{
			wait_flash_free();
			error(L" -- Verify Fail. \n");
			return(FALSE);
		}
		if(0 == counter%0x800)
			debug(L"#");
	}

	if(128==FW_Size)
	{
		for(counter=0x0000;counter<0x8000;counter++)
		{
			if(read_byte_from_flash()!=str3[counter])
			{
				wait_flash_free();
				error(L" -- Verify Fail. \n");
				return(FALSE);
			}
			if(0 == counter%0x800)
				debug(L"#");
		}

		for(counter=0x0000;counter<0x8000;counter++)
		{
			if(read_byte_from_flash()!=str4[counter])
			{
				wait_flash_free();
				error(L" -- Verify Fail. \n");
				return(FALSE);
			}
			if(0 == counter%0x800)
				debug(L"#");
		}
	}

	wait_flash_free();
	debug(L"   -- Verify OK. \n");
	return(TRUE);
}

static void Send_cmd_by_PM_port(UINT8 Cmd, UINT8 Port)
{
	Wait_PM_IBE();
	outb(Cmd, Port);
	Wait_PM_IBE();
}

UINT8 get_ec_sub_ver(UINT8 Port)
{
	Send_cmd_by_PM_port(SPICmd_VERSION, PM_CMD_PORT66);
	Send_cmd_by_PM_port(Port, PM_DATA_PORT62);

	return Read_data_from_PM();
}

void output_ec_version(void)
{
	info(L"Main version: %x", get_ec_sub_ver(0x0));
	info(L"Sub  version: %x", get_ec_sub_ver(0x1));
	info(L"Test version: %x", get_ec_sub_ver(0x2));
}

int update_ec(void *data, uint32_t len)
{
	int retries = 5;

	debug(L"Update EC start\n");

	debug(L"Get current EC version");
	output_ec_version();

	if (!data) {
		error(L"EC data is NULL");
		return -1;
	}

	if (len != 128*1024) {
		error(L"We only support EC data length with 128K bytes");
		return -1;
	}

	str1 = (UINT8 *)data;
	str2 = (UINT8 *)data + 0x8000;
	str3 = (UINT8 *)data + 0x8000*2;
	str4 = (UINT8 *)data + 0x8000*3;

	Send_cmd_by_PM(_ENTER_flash_mode); 

	while ((Read_data_from_PM() != 0x33) && retries > 0) {
		retries--;
		pause_us(RETRY_INTERVAL_US);
	}
	if (retries == 0) {
		error(L"EC: enter flash mode failed\n");
		pause_us(1000000);
		goto end;
	}

	Read_SPIFlash_JEDEC_ID();

	debug(L"Update EC Erase verify\n");
	if(0 != ec_erase_and_verify()) {
		error(L"Verify Erase fail\n");
		goto end;
	}

	eFlash_Program();

	debug(L"Update EC flash Verify\n");
	if(FALSE == Program_Flash_Verify()) {
		error(L"Verify program fail\n");
		goto end;
	}

end:
	pause_us(30000);
	Send_cmd_by_PM(_EXIT_flash_mode);

	debug(L"EC version after flashing\n");
	output_ec_version();
	debug(L"Update EC Done\n");

	return 0;
}
