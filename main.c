/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "utils.h"
#include "start.h"
#include "hollywood.h"
#include "sdhc.h"
#include "string.h"
#include "memory.h"
#include "gecko.h"
#include "ff.h"
#include "panic.h"
#include "powerpc_elf.h"
#include "irq.h"
#include "ipc.h"
#include "exception.h"
#include "crypto.h"
#include "nand.h"
#include "boot2.h"

#define NAND_DUMP_FILE "/bootmii/nand.bin"

FATFS fatfs;

u32 _main(void *base)
{	sensorPrep();
	FRESULT fres;
	int res;
	u32 vector=0;
	(void)base;

	gecko_init();
	gecko_printf("Triinux team's NAND dumper loading\n");

	gecko_printf("Initializing exceptions...\n");
	exception_initialize();
	gecko_printf("Configuring caches and MMU...\n");
	mem_initialize();

/*	gecko_printf("IOSflags: %08x %08x %08x\n",
		read32(0xffffff00), read32(0xffffff04), read32(0xffffff08));
	gecko_printf("          %08x %08x %08x\n",
		read32(0xffffff0c), read32(0xffffff10), read32(0xffffff14));
*/
	irq_initialize();
//	irq_enable(IRQ_GPIO1B);
	irq_enable(IRQ_GPIO1);
	irq_enable(IRQ_RESET);
	irq_enable(IRQ_TIMER);
	irq_set_alarm(20, 1);
	gecko_printf("Interrupts initialized\n");

	crypto_initialize();
	gecko_printf("crypto support initialized\n");

	nand_initialize();
	gecko_printf("NAND initialized.\n");

	boot2_init();

	gecko_printf("Initializing IPC...\n");
	ipc_initialize();

	gecko_printf("Initializing SDHC...\n");
	sdhc_init();

	gecko_printf("Mounting SD...\n");
	fres = f_mount(0, &fatfs);

	if (read32(0x0d800190) & 2) {
		gecko_printf("GameCube compatibility mode detected...\n");
		vector = boot2_run(1, 0x101);
		goto shutdown;
	}
	
	if(fres != FR_OK) {
		gecko_printf("Error %d while trying to mount SD\n", fres);
		panic2(0, PANIC_MOUNT);
	}

	

	if(read32(0x01200004) == 0x016AE570)
	{	gecko_printf("Dumping to : %s\n", (char*)0x01200008);
		res = dump_NAND_to((char*)0x01200008);
	}
	else
	{	gecko_printf("Dumping to : %s\n", PPC_BOOT_FILE);
		res = dump_NAND_to(NAND_DUMP_FILE);
	}
	gecko_printf("Booting System Menu\n");
	vector = boot2_run(1, 2);
	goto shutdown;

	gecko_printf("Going into IPC mainloop...\n");
	vector = ipc_process_slow();
	gecko_printf("IPC mainloop done!\n");
	gecko_printf("Shutting down IPC...\n");
	ipc_shutdown();

shutdown:
	gecko_printf("Shutting down interrupts...\n");
	irq_shutdown();
	gecko_printf("Shutting down caches and MMU...\n");
	mem_shutdown();

	gecko_printf("Vectoring to 0x%08x...\n", vector);
	return vector;
}

