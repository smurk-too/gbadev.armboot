/*
        mini - a Free Software replacement for the Nintendo/BroadOn IOS.
        PowerPC ELF file loading

Copyright (C) 2008, 2009        Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009                      Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "powerpc.h"
#include "hollywood.h"
#include "utils.h"
#include "start.h"
#include "gecko.h"
#include "ff.h"
#include "powerpc_elf.h"
#include "elf.h"
#include "memory.h"
#include "string.h"

//obcd
//missing
typedef signed int bool;
#define false 0
#define true 1

extern u8 __mem2_area_start[];

#define PPC_MEM1_END    (0x017fffff)
#define PPC_MEM2_START  (0x10000000)
#define PPC_MEM2_END    ((u32) __mem2_area_start)

#define PHDR_MAX 10

typedef struct dol_t dol_t;
struct dol_t
{
	u32 offsetText[7];
	u32 offsetData[11];
	u32 addressText[7];
	u32 addressData[11];
	u32 sizeText[7];
	u32 sizeData[11];
	u32 addressBSS;
	u32 sizeBSS;
	u32 entrypt;
	u8 pad[0x1C];
};

static int _check_physaddr(u32 addr) {
	if ((addr >= PPC_MEM2_START) && (addr <= PPC_MEM2_END))
		return 2;

	if (addr < PPC_MEM1_END)
		return 1;

	return -1;
}

static int _check_physrange(u32 addr, u32 len) {
	switch (_check_physaddr(addr)) {
	case 1:
		if ((addr + len) < PPC_MEM1_END)
			return 1;
		break;
	case 2:
		if ((addr + len) < PPC_MEM2_END)
			return 2;
		break;
	}

	return -1;
}

static Elf32_Ehdr elfhdr;
static Elf32_Phdr phdrs[PHDR_MAX];

u32 virtualToPhysical(u32 virtualAddress)
{
	if ((virtualAddress & 0xC0000000) == 0xC0000000) return virtualAddress & ~0xC0000000;
	if ((virtualAddress & 0x80000000) == 0x80000000) return virtualAddress & ~0x80000000;
	return virtualAddress;
}

u32 makeRelativeBranch(u32 currAddr, u32 destAddr, bool linked)
{
	u32 ret = 0x48000000 | (( destAddr - currAddr ) & 0x3FFFFFC );
	if(linked)
		ret |= 1;
	return ret;
}

u32 makeAbsoluteBranch(u32 destAddr, bool linked)
{
	u32 ret = 0x48000002 | ( destAddr & 0x3FFFFFC );
	if(linked)
	ret |= 1;
	return ret;
}

int powerpc_load_dol(const char *path, u32 *endAddress)
{
	u32 read;
	FIL fd;
	FRESULT fres;
	dol_t dol_hdr;
	gecko_printf("Loading DOL file: %s .\n", path);
	fres = f_open(&fd, path, FA_READ);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, &dol_hdr, sizeof(dol_t), &read);
	if (fres != FR_OK)
		return -fres;

	u32 end = 0;
	int ii;

	/* TEXT SECTIONS */
	for (ii = 0; ii < 7; ii++)
	{	//gecko_printf("Loading text section %d .\n", ii);
		if (!dol_hdr.sizeText[ii])
			continue;
		fres = f_lseek(&fd, dol_hdr.offsetText[ii]);
		if (fres != FR_OK)
			return -fres;
		u32 phys = virtualToPhysical(dol_hdr.addressText[ii]);
		fres = f_read(&fd, (void*)phys, dol_hdr.sizeText[ii], &read);
		if (fres != FR_OK)
			return -fres;
		if (phys + dol_hdr.sizeText[ii] > end)
			end = phys + dol_hdr.sizeText[ii];
		gecko_printf("Section of size %08x loaded from offset %08x to memory %08x.\n", dol_hdr.sizeText[ii], dol_hdr.offsetText[ii], virtualToPhysical(dol_hdr.addressText[ii]));
	}

	/* DATA SECTIONS */
	for (ii = 0; ii < 11; ii++)
	{	//gecko_printf("Loading data section %d .\n", ii);
		if (!dol_hdr.sizeData[ii])
			continue;
		fres = f_lseek(&fd, dol_hdr.offsetData[ii]);
		if (fres != FR_OK)
			return -fres;
		u32 phys = virtualToPhysical(dol_hdr.addressData[ii]);
		fres = f_read(&fd, (void*)phys, dol_hdr.sizeData[ii], &read);
		if (fres != FR_OK)
			return -fres;
		if (phys + dol_hdr.sizeData[ii] > end)
			end = phys + dol_hdr.sizeData[ii];
		gecko_printf("Section of size %08x loaded from offset %08x to memory %08x.\n", dol_hdr.sizeData[ii], dol_hdr.offsetData[ii], virtualToPhysical(dol_hdr.addressData[ii]));
	}
	if (endAddress)
	{	*endAddress = end - 1;
		gecko_printf("endAddress = %08x\n", *endAddress);
	}
	return 0;
}

int powerpc_boot_file(const char *path)
{
	u32 read;
	FIL fd;
	FRESULT fres;

	// do race attack here
	u32 decryptionEndAddress;
	//powerpc_hang();
	udelay(300000);
	sensorbarOn();
	udelay(300000);
	fres = powerpc_load_dol("/bootmii/00000003.app", &decryptionEndAddress);
	gecko_printf("powerpc_load_dol returned %d .\n", fres);
	sensorbarOff();
	udelay(300000);

	u32 oldValue = read32(0x1330100), oldValue2 = read32(decryptionEndAddress);
	//sensorbarOn();
	//udelay(300000);
	//oldValue2 = read32(decryptionEndAddress);
	u32 Core0JumpInstruction = makeAbsoluteBranch(0x1800, false);
	// We'll trap PPC here with an infinite loop until we're done loading other stuff
	//sensorbarOff();
	//udelay(300000);
	
/*	// flash stub
	write32(0x1800, 0x38600005); //li   r3,5
	write32(0x1804, 0x7c6903a6); //mtctr  r3
	write32(0x1808, 0x3c600d00); //lis r3,3328
	write32(0x180c, 0x808300c0); //lwz r4,192(r3)
	write32(0x1810, 0x3ca000ff); //lis r5,255
	write32(0x1814, 0x60a5feff); //ori r5,r5,65279
	write32(0x1818, 0x7c842838); //and r4,r4,r5
	write32(0x181c, 0x908300c0); //stw r4,192(r3)
	write32(0x1820, 0x7c6c42e6); //mftbl  r3
	write32(0x1824, 0x3ca001c9); //lis r5,457
	write32(0x1828, 0x60a5c380); //ori r5,r5,50048
	write32(0x182c, 0x7c8c42e6); //mftbl  r4
	write32(0x1830, 0x7c832050); //subf r4,r3,r4
	write32(0x1834, 0x7c042840); //cmplw  r4,r5
	write32(0x1838, 0x4180fff4); //blt+ 0x12c ?
	write32(0x183c, 0x3c600d00); //lis r3,3328
	write32(0x1840, 0x808300c0); //lwz r4,192(r3)
	write32(0x1844, 0x60840100); //ori r4,r4,256
	write32(0x1848, 0x908300c0); //stw r4,192(r3)
	write32(0x184c, 0x7c6c42e6); //mftbl  r3
	write32(0x1850, 0x3ca001c9); //lis r5,457
	write32(0x1854, 0x60a5c380); //ori r5,r5,50048
	write32(0x1858, 0x7c8c42e6); //mftbl  r4
	write32(0x185c, 0x7c832050); //subf r4,r3,r4
	write32(0x1860, 0x7c042840); //cmplw  r4,r5
	write32(0x1864, 0x4200ff98); //blt+ 0x1858 ?
	write32(0x1868, 0x4200ff98); //bdnz+  0x1800 ?
	write32(0x186c, 0x48000000); // makeAbsoluteBranch(0x1870, false));
	//write32(0x1870, makeAbsoluteBranch(0x186c, false));
*/
    powerpc_upload_stub(0x1800, 0);
	write32(0x18b8, 0x7c0004ac); //sync
	write32(0x18bc, makeRelativeBranch(0x18bc, 0x18b8, false));

	sensorbarOn();
	udelay(300000);
	dc_flushall();

	sensorbarOff();
	udelay(300000);
    //set32(HW_GPIO1OWNER, HW_GPIO1_SENSE);
	//powerpc_reset();
	gecko_printf("Resetting PPC. End debug output.\n");
	gecko_enable(0);
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
	set32(HW_RESETS, 0x10);

	do
	{	dc_invalidaterange((void*)0x1330100,32);
		ahb_flush_from(AHB_1);
	}while(oldValue == read32(0x1330100));

	// where core 0 will end up once the ROM is done decrypting 1-200
	write32(0x1330100, Core0JumpInstruction);
	dc_flushrange((void*)0x1330100,32);

	sensorbarOn();

	// wait for decryption / validation to finish and PPC to flag that we have control.
	do
	{	dc_invalidaterange((void*)decryptionEndAddress,32);
		ahb_flush_from(AHB_1);
	}while(oldValue2 == read32(decryptionEndAddress));
	//udelay(2000000);
	
	udelay(300000);
	sensorbarOff();
	//udelay(300000);
	/*      sensorbarOn();
	udelay(300000);
	*/

	fres = f_open(&fd, path, FA_READ);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, &elfhdr, sizeof(elfhdr), &read);

	if (fres != FR_OK)
		return -fres;

	if (read != sizeof(elfhdr))
		return -100;

	if (memcmp("\x7F" "ELF\x01\x02\x01\x00\x00",elfhdr.e_ident,9)) {
		gecko_printf("Invalid ELF header! 0x%02x 0x%02x 0x%02x 0x%02x\n",elfhdr.e_ident[0], elfhdr.e_ident[1], elfhdr.e_ident[2], elfhdr.e_ident[3]);
		return -101;
	}

	if (_check_physaddr(elfhdr.e_entry) < 0) {
		gecko_printf("Invalid entry point! 0x%08x\n", elfhdr.e_entry);
		return -102;
	}

	if (elfhdr.e_phoff == 0 || elfhdr.e_phnum == 0) {
		gecko_printf("ELF has no program headers!\n");
		return -103;
	}

	if (elfhdr.e_phnum > PHDR_MAX) {
		gecko_printf("ELF has too many (%d) program headers!\n", elfhdr.e_phnum);
		return -104;
	}

	fres = f_lseek(&fd, elfhdr.e_phoff);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, phdrs, sizeof(phdrs[0])*elfhdr.e_phnum, &read);
	if (fres != FR_OK)
		return -fres;

	if (read != sizeof(phdrs[0])*elfhdr.e_phnum)
		return -105;

	u16 count = elfhdr.e_phnum;
	Elf32_Phdr *phdr = phdrs;

	while (count--) {
			if (phdr->p_type != PT_LOAD) {
					gecko_printf("Skipping PHDR of type %d\n", phdr->p_type);
			} else {
					if (_check_physrange(phdr->p_paddr, phdr->p_memsz) < 0) {
							gecko_printf("PHDR out of bounds [0x%08x...0x%08x]\n",
															phdr->p_paddr, phdr->p_paddr + phdr->p_memsz);
							return -106;
					}

					void *dst = (void *) phdr->p_paddr;

					gecko_printf("LOAD 0x%x @0x%08x [0x%x]\n", phdr->p_offset, phdr->p_paddr, phdr->p_filesz);
					fres = f_lseek(&fd, phdr->p_offset);
					if (fres != FR_OK)
							return -fres;
					fres = f_read(&fd, dst, phdr->p_filesz, &read);
					if (fres != FR_OK)
							return -fres;
					if (read != phdr->p_filesz)
							return -107;
			}
			phdr++;
	}

	//powerpc_upload_stub(0x1800, elfhdr.e_entry);
    write32(0x1a50, 0x3c600000 | elfhdr.e_entry >> 16 ); //lis     r3,entry@h
    write32(0x1a54, 0x60630000 | (elfhdr.e_entry & 0xffff) ); //ori     r3,r3,entry@l

	dc_flushall();

	gecko_printf("ELF load done, booting PPC...\n");

	udelay(300000);
	sensorbarOn();
	udelay(300000);

	write32(0x18bc, 0x60000000); //nop
	dc_flushrange((void*)0x18a0,32);
	gecko_printf("PPC booted!\n");
	sensorbarOff();

	return 0;
}

int powerpc_boot_mem(const u8 *addr, u32 len)
{
	if (len < sizeof(Elf32_Ehdr))
		return -100;

	Elf32_Ehdr *ehdr = (Elf32_Ehdr *) addr;

	if (memcmp("\x7F" "ELF\x01\x02\x01\x00\x00", ehdr->e_ident, 9)) {
		gecko_printf("Invalid ELF header! 0x%02x 0x%02x 0x%02x 0x%02x\n",
					ehdr->e_ident[0], ehdr->e_ident[1],
					ehdr->e_ident[2], ehdr->e_ident[3]);
		return -101;
	}

	if (_check_physaddr(ehdr->e_entry) < 0) {
		gecko_printf("Invalid entry point! 0x%08x\n", ehdr->e_entry);
		return -102;
	}

	if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		gecko_printf("ELF has no program headers!\n");
		return -103;
	}

	if (ehdr->e_phnum > PHDR_MAX) {
		gecko_printf("ELF has too many (%d) program headers!\n",
					ehdr->e_phnum);
		return -104;
	}

	u16 count = ehdr->e_phnum;
	if (len < ehdr->e_phoff + count * sizeof(Elf32_Phdr))
		return -105;

	Elf32_Phdr *phdr = (Elf32_Phdr *) &addr[ehdr->e_phoff];

	// TODO: add more checks here
	// - loaded ELF overwrites itself?

	powerpc_hang();

	while (count--) {
		if (phdr->p_type != PT_LOAD) {
			gecko_printf("Skipping PHDR of type %d\n", phdr->p_type);
		} else {
			if (_check_physrange(phdr->p_paddr, phdr->p_memsz) < 0) {
				gecko_printf("PHDR out of bounds [0x%08x...0x%08x]\n",
								phdr->p_paddr, phdr->p_paddr + phdr->p_memsz);
				return -106;
			}

			gecko_printf("LOAD 0x%x @0x%08x [0x%x]\n", phdr->p_offset, phdr->p_paddr, phdr->p_filesz);
			memcpy((void *) phdr->p_paddr, &addr[phdr->p_offset],
				phdr->p_filesz);
		}
		phdr++;
	}

	dc_flushall();

	gecko_printf("ELF load done, booting PPC...\n");
	powerpc_upload_stub(0x104, ehdr->e_entry);
	powerpc_reset();
	gecko_printf("PPC booted!\n");

	return 0;
}
