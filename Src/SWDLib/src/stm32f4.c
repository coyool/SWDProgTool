/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements STM32F4 target specific functions for detecting
 * the device, providing the XML memory map and Flash memory programming.
 *
 * Refereces:
 * ST doc - RM0090
 *   Reference manual - STM32F405xx, STM32F407xx, STM32F415xx and STM32F417xx
 *   advanced ARM-based 32-bit MCUs
 * ST doc - PM0081
 *   Programming manual - STM32F40xxx and STM32F41xxx Flash programming
 *    manual
 */

#include "general.h"
#include "target.h"
#include "target_internal.h"
#include "cortexm.h"
#include "target_stm32f4.h"

void stm32f4_add_flash(target *t,
                              uint32_t addr, size_t length, size_t blocksize,
                              uint8_t base_sector)
{
	//struct stm32f4_flash *sf = calloc(1, sizeof(*sf));
	static uint8_t stm32f4_flash_memory_space_count = 0;
	static uint8_t stm32f4_flash_memory_space[10][64];
	struct stm32f4_flash *sf = (struct stm32f4_flash *)stm32f4_flash_memory_space[stm32f4_flash_memory_space_count++];
	struct target_flash *f = &sf->f;
	f->start = addr;
	f->length = length;
	f->blocksize = blocksize;
	f->erase = stm32f4_flash_erase;
	f->write = stm32f4_flash_write;
	f->align = 4;
	f->erased = 0xff;
	sf->base_sector = base_sector;
	sf->psize = 32;
	target_add_flash(t, f);
}

bool stm32f4_probe(target *t)
{
	bool f2 = false;
	uint32_t idcode;

	idcode = target_mem_read32(t, DBGMCU_IDCODE);
	idcode &= 0xFFF;

	if (idcode == ID_STM32F20X)
	{
		/* F405 revision A have a wrong IDCODE, use ARM_CPUID to make the
		 * distinction with F205. Revision is also wrong (0x2000 instead
		 * of 0x1000). See F40x/F41x errata. */
		uint32_t cpuid = target_mem_read32(t, ARM_CPUID);
		if ((cpuid & 0xFFF0) == 0xC240)
			idcode = ID_STM32F40X;
		else
			f2 = true;
	}

	switch(idcode) {
	case ID_STM32F42X: /* 427/437 */
	case ID_STM32F46X: /* 469/479 */
		/* Second bank for 2M parts. */
		stm32f4_add_flash(t, 0x8100000, 0x10000, 0x4000, 12);
		stm32f4_add_flash(t, 0x8110000, 0x10000, 0x10000, 16);
		stm32f4_add_flash(t, 0x8120000, 0xE0000, 0x20000, 17);
		/* Fall through for stuff common to F40x/F41x */
	case ID_STM32F20X: /* F205 */
	case ID_STM32F40X: /* F405 */
		if (!f2)
			target_add_ram(t, 0x10000000, 0x10000);
		/* Fall through for devices w/o CCMRAM */
	case ID_STM32F446: /* F446 */
	case ID_STM32F401C: /* F401 B/C RM0368 Rev.3 */
	case ID_STM32F411: /* F411     RM0383 Rev.4 */
	case ID_STM32F412: /* F412     RM0402 Rev.4, 256 kB Ram */
	case ID_STM32F401E: /* F401 D/E RM0368 Rev.3 */
		t->driver = f2 ? stm32f2_driver_str : stm32f4_driver_str;
		target_add_ram(t, 0x20000000, 0x40000);
		stm32f4_add_flash(t, 0x8000000, 0x10000, 0x4000, 0);
		stm32f4_add_flash(t, 0x8010000, 0x10000, 0x10000, 4);
		stm32f4_add_flash(t, 0x8020000, 0xE0000, 0x20000, 5);
		target_add_commands(t, stm32f4_cmd_list, f2 ? "STM32F2" :
		                    "STM32F4");
		break;
	case ID_STM32F413: /* F413     RM0430 Rev.2, 320 kB Ram, 1.5 MB flash. */
		t->driver = stm32f4_driver_str;
		target_add_ram(t, 0x20000000, 0x50000);
		stm32f4_add_flash(t, 0x8000000, 0x10000, 0x4000, 0);
		stm32f4_add_flash(t, 0x8010000, 0x10000, 0x10000, 4);
		stm32f4_add_flash(t, 0x8020000, 0x160000, 0x20000, 5);
		target_add_commands(t, stm32f4_cmd_list, "STM32F413");
		break;
	case ID_STM32F74X: /* F74x RM0385 Rev.4 */
		t->driver = stm32f7_driver_str;
		target_add_ram(t, 0x00000000, 0x4000);
		target_add_ram(t, 0x20000000, 0x50000);
		/* AXIM Flash access */
		stm32f4_add_flash(t, 0x8000000, 0x20000, 0x8000, 0);
		stm32f4_add_flash(t, 0x8020000, 0x20000, 0x20000, 4);
		stm32f4_add_flash(t, 0x8040000, 0xC0000, 0x40000, 5);
		/* Flash aliased as ITCM */
		stm32f4_add_flash(t, 0x0200000, 0x20000, 0x8000, 0);
		stm32f4_add_flash(t, 0x0220000, 0x20000, 0x20000, 4);
		stm32f4_add_flash(t, 0x0240000, 0xC0000, 0x40000, 5);
		target_add_commands(t, stm32f4_cmd_list, "STM32F74x");
		break;
	case ID_STM32F76X: /* F76x F77x RM0410 */
		t->driver = stm32f7_driver_str;
		target_add_ram(t, 0x00000000, 0x4000);
		target_add_ram(t, 0x20000000, 0x80000);
		/* AXIM Flash access */
		stm32f4_add_flash(t, 0x8000000, 0x020000, 0x8000, 0);
		stm32f4_add_flash(t, 0x8020000, 0x020000, 0x20000, 4);
		stm32f4_add_flash(t, 0x8040000, 0x1C0000, 0x40000, 5);
		/* Flash aliased as ITCM */
		stm32f4_add_flash(t, 0x200000, 0x020000, 0x8000, 0);
		stm32f4_add_flash(t, 0x220000, 0x020000, 0x20000, 4);
		stm32f4_add_flash(t, 0x240000, 0x1C0000, 0x40000, 5);
		target_add_commands(t, stm32f4_cmd_list, "STM32F76x");
		break;
	case ID_STM32F72X: /* F72x F73x RM0431 */
		t->driver = stm32f7_driver_str;
		target_add_ram(t, 0x00000000, 0x2000);
		target_add_ram(t, 0x20000000, 0x40000);
		stm32f4_add_flash(t, 0x8000000, 0x010000, 0x4000,  0);
		stm32f4_add_flash(t, 0x8010000, 0x010000, 0x10000, 4);
		stm32f4_add_flash(t, 0x8020000, 0x060000, 0x20000, 3);
		target_add_commands(t, stm32f4_cmd_list, "STM32F72x");
		break;
	default:
		return false;
	}
	t->idcode = idcode;
	return true;
}

void stm32f4_flash_unlock(target *t)
{
	if (target_mem_read32(t, FLASH_CR) & FLASH_CR_LOCK) {
		/* Enable FPEC controller access */
		target_mem_write32(t, FLASH_KEYR, KEY1);
		target_mem_write32(t, FLASH_KEYR, KEY2);
	}
}

int stm32f4_flash_erase(struct target_flash *f, target_addr addr, size_t len)
{
	target *t = f->t;
	uint16_t sr;
	/* No address translation is needed here, as we erase by sector number */
	uint8_t sector = ((struct stm32f4_flash *)f)->base_sector +
	                  (addr - f->start)/f->blocksize;

	stm32f4_flash_unlock(t);

	while(len) {
		uint32_t cr = FLASH_CR_EOPIE | FLASH_CR_ERRIE | FLASH_CR_SER |
		              (sector << 3);
		/* Flash page erase instruction */
		target_mem_write32(t, FLASH_CR, cr);
		/* write address to FMA */
		target_mem_write32(t, FLASH_CR, cr | FLASH_CR_STRT);

		/* Read FLASH_SR to poll for BSY bit */
		while(target_mem_read32(t, FLASH_SR) & FLASH_SR_BSY)
			if(target_check_error(t))
				return -1;
		if(len < f->blocksize)   //prevent sub underflow
			break;
		else
			len -= f->blocksize;
		sector++;
	}

	/* Check for error */
	sr = target_mem_read32(t, FLASH_SR);
	if(sr & SR_ERROR_MASK)
		return -1;

	return 0;
}

int stm32f4_flash_write(struct target_flash *f,
                               target_addr dest, const void *src, size_t len)
{
	/* Translate ITCM addresses to AXIM */
	if ((dest >= ITCM_BASE) && (dest < AXIM_BASE)) {
		dest = AXIM_BASE + (dest - ITCM_BASE);
	}

	/* Write buffer to target ram call stub */
	if (((struct stm32f4_flash *)f)->psize == 32)
		target_mem_write(f->t, SRAM_BASE, stm32f4_flash_write_x32_stub,
		                 sizeof(stm32f4_flash_write_x32_stub));
	else
		target_mem_write(f->t, SRAM_BASE, stm32f4_flash_write_x8_stub,
		                 sizeof(stm32f4_flash_write_x8_stub));
	target_mem_write(f->t, STUB_BUFFER_BASE, src, len);

	return cortexm_run_stub(f->t, SRAM_BASE, dest,
	                        STUB_BUFFER_BASE, len, 0);
}

bool stm32f4_cmd_erase_mass(target *t)
{
	const char spinner[] = "|/-\\";
	int spinindex = 0;

	tc_printf(t, "Erasing flash... This may take a few seconds.  ");
	stm32f4_flash_unlock(t);

	/* Flash mass erase start instruction */
	target_mem_write32(t, FLASH_CR, FLASH_CR_MER);
	target_mem_write32(t, FLASH_CR, FLASH_CR_STRT | FLASH_CR_MER);

	/* Read FLASH_SR to poll for BSY bit */
	while (target_mem_read32(t, FLASH_SR) & FLASH_SR_BSY) {
		tc_printf(t, "\b%c", spinner[spinindex++ % 4]);
		if(target_check_error(t)) {
			tc_printf(t, "\n");
			return false;
		}
	}
	tc_printf(t, "\n");

	/* Check for error */
	uint16_t sr = target_mem_read32(t, FLASH_SR);
	if ((sr & SR_ERROR_MASK) || !(sr & SR_EOP))
		return false;

	return true;
}

/* Dev   | DOC  |Rev|ID |OPTCR    |OPTCR   |OPTCR1   |OPTCR1 | OPTCR2
                    |hex|default  |reserved|default  |resvd  | default|resvd
 * F20x  |pm0059|5.1|411|0FFFAAED |F0000010|
 * F40x  |rm0090|11 |413|0FFFAAED |F0000010|
 * F42x  |rm0090|11 |419|0FFFAAED |30000000|0FFF0000 |F000FFFF
 * F446  |rm0390| 2 |421|0FFFAAED |7F000010|
 * F401BC|rm0368| 3 |423|0FFFAAED |7FC00010|
 * F411  |rm0383| 2 |431|0FFFAAED |7F000010|
 * F401DE|rm0368| 3 |433|0FFFAAED |7F000010|
 * F46x  |rm0386| 2 |434|0FFFAAED |30000000|0FFF0000 |F000FFFF
 * F412  |rm0402| 4 |441|0FFFAAED*|70000010|
 * F74x  |rm0385| 4 |449|C0FFAAFD |3F000000|00400080*|00000000
 * F76x  |rm0410| 2 |451|FFFFAAFD*|00000000|00400080*|00000000
 * F72x  |rm0431| 1 |452|C0FFAAFD |3F000000|00400080*|00000000|00000000|800000FF
 * F410  |rm0401| 2 |458|0FFFAAED*|7FE00010|
 * F413  |rm0430| 2 |463|7FFFAAED*|00000010|
 *
 * * Documentation for F7 with OPTCR1 default = 0fff7f0080 seems wrong!
 * * Documentation for F412 with OPTCR default = 0ffffffed seems wrong!
 * * Documentation for F413 with OPTCR default = 0ffffffed seems wrong!
 */

bool optcr_mask(target *t, uint32_t *val)
{
	switch (t->idcode) {
	case ID_STM32F20X:
	case ID_STM32F40X:
		val[0] &= ~0xF0000010;
		break;
	case ID_STM32F46X:
	case ID_STM32F42X:
		val[0] &= ~0x30000000;
		val[1] &=  0x0fff0000;
		break;
	case ID_STM32F401C:
		val[0] &= ~0x7FC00010;
		break;
	case ID_STM32F446:
	case ID_STM32F411:
	case ID_STM32F401E:
		val[0] &= ~0x7F000010;
		break;
	case ID_STM32F410:
		val[0] &= ~0x7FE00010;
		break;
	case ID_STM32F412:
		val[0] &= ~0x70000010;
		break;
	case ID_STM32F413:
		val[0] &= ~0x00000010;
		break;
	case ID_STM32F72X:
		val[2] &=  ~0x800000ff;
		/* Fall through*/
	case ID_STM32F74X:
		val[0] &= ~0x3F000000;
		break;
	case ID_STM32F76X:
		break;
	default:
		return false;
	}
	return true;
}

bool stm32f4_option_write(target *t, uint32_t *val, int count)
{
	target_mem_write32(t, FLASH_OPTKEYR, OPTKEY1);
	target_mem_write32(t, FLASH_OPTKEYR, OPTKEY2);
	while (target_mem_read32(t, FLASH_SR) & FLASH_SR_BSY)
		if(target_check_error(t))
			return -1;

	/* WRITE option bytes instruction */
	if (((t->idcode == ID_STM32F42X) || (t->idcode == ID_STM32F46X) ||
		 (t->idcode == ID_STM32F72X) || (t->idcode == ID_STM32F74X) ||
		 (t->idcode == ID_STM32F76X)) && (count > 1))
	    /* Checkme: Do we need to read old value and then set it? */
		target_mem_write32(t, FLASH_OPTCR + 4, val[1]);
	if ((t->idcode == ID_STM32F72X) && (count > 2))
			target_mem_write32(t, FLASH_OPTCR + 8, val[2]);

	target_mem_write32(t, FLASH_OPTCR, val[0]);
	target_mem_write32(t, FLASH_OPTCR, val[0] | FLASH_OPTCR_OPTSTRT);
	/* Read FLASH_SR to poll for BSY bit */
	while(target_mem_read32(t, FLASH_SR) & FLASH_SR_BSY)
		if(target_check_error(t))
			return false;
	target_mem_write32(t, FLASH_OPTCR, FLASH_OPTCR_OPTLOCK);
	return true;
}

bool stm32f4_option_write_default(target *t)
{
	uint32_t val[3];
	switch (t->idcode) {
	case ID_STM32F42X:
	case ID_STM32F46X:
		val[0] = 0x0FFFAAED;
		val[1] = 0x0FFF0000;
		return stm32f4_option_write(t, val, 2);
	case ID_STM32F72X:
		val[0] = 0xC0FFAAFD;
		val[1] = 0x00400080;
		val[2] = 0;
		return stm32f4_option_write(t, val, 3);
	case ID_STM32F74X:
		val[0] = 0xC0FFAAFD;
		val[1] = 0x00400080;
		return stm32f4_option_write(t, val, 2);
	case ID_STM32F76X:
		val[0] = 0xFFFFAAFD;
		val[1] = 0x00400080;
		return stm32f4_option_write(t, val, 2);
	case ID_STM32F413:
		val[0] = 0x7FFFAAFD;
		return stm32f4_option_write(t, val, 1);
	default:
		val[0] = 0x0FFFAAED;
		return stm32f4_option_write(t, val, 1);
	}
}

bool stm32f4_cmd_option(target *t, int argc, char *argv[])
{
	uint32_t start = 0x1FFFC000, val[3];
	int count = 0, readcount = 1;

	switch (t->idcode) {
	case ID_STM32F72X: /* STM32F72|3 */
		readcount++;
		/* fall through.*/
	case ID_STM32F74X:
	case ID_STM32F76X:
		/* F7 Devices have option bytes at 0x1FFF0000. */
		start = 0x1FFF0000;
		readcount++;
		break;
	case ID_STM32F42X:
	case ID_STM32F46X:
		readcount++;
	}

	if ((argc == 2) && !strcmp(argv[1], "erase")) {
		stm32f4_option_write_default(t);
	}
	else if ((argc > 1) && !strcmp(argv[1], "write")) {
		val[0] = strtoul(argv[2], NULL, 0);
		count++;
		if (argc > 2) {
			val[1] = strtoul(argv[3], NULL, 0);
			count ++;
		}
		if (argc > 3) {
			val[2] = strtoul(argv[4], NULL, 0);
			count ++;
		}
		if (optcr_mask(t, val))
			stm32f4_option_write(t, val, count);
		else
			tc_printf(t, "error\n");
	} else {
		tc_printf(t, "usage: monitor option erase\n");
		tc_printf(t, "usage: monitor option write <OPTCR>");
		if (readcount > 1)
			tc_printf(t, " <OPTCR1>");
		if (readcount > 2)
			tc_printf(t, " <OPTCR2>");
		tc_printf(t, "\n");
	}

	val[0]  = (target_mem_read32(t, start + 8) & 0xffff) << 16;
	val[0] |= (target_mem_read32(t, start    ) & 0xffff);
	if (readcount > 1) {
		if (start == 0x1FFFC000) /* F4 */ {
			val[1] = target_mem_read32(t, start +  8 - 0x10000);
			val[1] &= 0xffff;
		} else {
			val[1] =  (target_mem_read32(t, start + 0x18) & 0xffff) << 16;
			val[1] |= (target_mem_read32(t, start + 0x10) & 0xffff);
		}
	}
	if (readcount > 2) {
			val[2] =  (target_mem_read32(t, start + 0x28) & 0xffff) << 16;
			val[2] |= (target_mem_read32(t, start + 0x20) & 0xffff);
	}
	optcr_mask(t, val);
	tc_printf(t, "OPTCR: 0x%08X ", val[0]);
	if (readcount > 1)
		tc_printf(t, "OPTCR1: 0x%08X ", val[1]);
	if (readcount > 2)
		tc_printf(t, "OPTCR2: 0x%08X" , val[2]);
	tc_printf(t, "\n");
	return true;
}

bool stm32f4_cmd_psize(target *t, int argc, char *argv[])
{
	if (argc == 1) {
		uint8_t psize = 8;
		for (struct target_flash *f = t->flash; f; f = f->next) {
			if (f->write == stm32f4_flash_write) {
				psize = ((struct stm32f4_flash *)f)->psize;
			}
		}
		tc_printf(t, "Flash write parallelism: %s\n",
		          psize == 32 ? "x32" : "x8");
	} else {
		uint8_t psize;
		if (!strcmp(argv[1], "x8")) {
			psize = 8;
		} else if (!strcmp(argv[1], "x32")) {
			psize = 32;
		} else {
			tc_printf(t, "usage: monitor psize (x8|x32)\n");
			return false;
		}
		for (struct target_flash *f = t->flash; f; f = f->next) {
			if (f->write == stm32f4_flash_write) {
				((struct stm32f4_flash *)f)->psize = psize;
			}
		}
	}
	return true;
}
