/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/layout.h"

#include "hf/std.h"
#include "hf/dlog.h"
#include "crc.h"

/**
 * Get the address the .text section begins at.
 */
paddr_t layout_text_begin(void)
{
	extern uint8_t text_begin[];

	return pa_init((uintpaddr_t)text_begin);
}

/**
 * Get the address the .text section ends at.
 */
paddr_t layout_text_end(void)
{
	extern uint8_t text_end[];

	return pa_init((uintpaddr_t)text_end);
}

/**
 * Get the address the .rodata section begins at.
 */
paddr_t layout_rodata_begin(void)
{
	extern uint8_t rodata_begin[];

	return pa_init((uintpaddr_t)rodata_begin);
}

/**
 * Get the address the .rodata section ends at.
 */
paddr_t layout_rodata_end(void)
{
	extern uint8_t rodata_end[];

	return pa_init((uintpaddr_t)rodata_end);
}

/**
 * Get the address the .data section begins at.
 */
paddr_t layout_data_begin(void)
{
	extern uint8_t data_begin[];

	return pa_init((uintpaddr_t)data_begin);
}

/**
 * Get the address the .data section ends at.
 */
paddr_t layout_data_end(void)
{
	extern uint8_t data_end[];

	return pa_init((uintpaddr_t)data_end);
}

/**
 * Get the address the .stacks section begins at.
 */
paddr_t layout_stacks_begin(void)
{
	extern uint8_t stacks_begin[];

	return pa_init((uintpaddr_t)stacks_begin);
}

/**
 * Get the address the .stacks section ends at.
 */
paddr_t layout_stacks_end(void)
{
	extern uint8_t stacks_end[];

	return pa_init((uintpaddr_t)stacks_end);
}

/**
 * Get the address the .initrd section begins at.
 */
paddr_t layout_initrd_begin(void)
{
	extern uint8_t initrd_begin[];

	return pa_init((uintpaddr_t)initrd_begin);
}

/**
 * Get the address the .initrd section ends at.
 */
paddr_t layout_initrd_end(void)
{
	extern uint8_t initrd_end[];

	return pa_init((uintpaddr_t)initrd_end);
}

/**
 * Get the address the .fdt section begins at.
 */
paddr_t layout_fdt_begin(void)
{
	extern uint8_t fdt_begin[];

	return pa_init((uintpaddr_t)fdt_begin);
}

/**
 * Get the address the .fdt section ends at.
 */
paddr_t layout_fdt_end(void)
{
	extern uint8_t fdt_end[];

	return pa_init((uintpaddr_t)fdt_end);
}

/**
 * Get the address the loaded image ends at.
 */
paddr_t layout_image_end(void)
{
	extern uint8_t image_end[];

	return pa_init((uintpaddr_t)image_end);
}

/**
 * Get the address to load the primary VM at.
 *
 * This is placed just after the image.
 */
paddr_t layout_primary_begin(void)
{
	paddr_t image_end = layout_image_end();

	/*
	 * Linux usually expects to be loaded at offset 0x80000 into a 2MB
	 * aligned address.
	 * TODO: This is a hack, and isn't always correct. We should really read
	 * the alignment from the header of the binary, or have a bootloader
	 * within the VM do so.
	 */
	return pa_init(align_up(pa_addr(image_end), LINUX_ALIGNMENT) +
		       LINUX_OFFSET);
}

paddr_t layout_bss_begin(void)
{
	extern uint8_t bss_begin[];

	return pa_init((uintpaddr_t)bss_begin);
}

/**
 * Get the address the .fdt section ends at.
 */
paddr_t layout_bss_end(void)
{
	extern uint8_t bss_end[];

	return pa_init((uintpaddr_t)bss_end);
}

void layout_dump(void)
{
    paddr_t begin, end;
    uint32_t crc;

    dlog_info(">>>>>> layout dump: \n");
    begin = layout_text_begin();
    end   = layout_text_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("Text layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);

    begin = layout_rodata_begin();
    end   = layout_rodata_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("Rodata layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);

    begin = layout_data_begin();
    end   = layout_data_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("data layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);
    
    begin = layout_stacks_begin();
    end   = layout_stacks_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("stack layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);

    begin = layout_bss_begin();
    end   = layout_bss_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("bss layout: 0x%08x : 0x%08x(crc: 0x%08x)\n", begin.pa, end.pa, crc);
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("bss layout: 0x%08x : 0x%08x(crc: 0x%08x)\n", begin.pa, end.pa, crc);

    begin = layout_initrd_begin();
    end   = layout_initrd_end();
    crc = crcSlow((unsigned char *)begin.pa, end.pa - begin.pa);
    dlog_info("initrd layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);

    begin = layout_fdt_begin();
    end   = layout_fdt_end();
    dlog_info("fdt layout: 0x%08x : 0x%08x (crc: 0x%08x)\n", begin.pa, end.pa, crc);
    dlog_info(">>>>>> layout dump end. \n");
}
