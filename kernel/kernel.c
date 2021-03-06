/*
 * kernel/kernel.c
 * Copyright (C) 2016-2017 Alexei Frolov
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <acpi/acpi.h>

#include <radix/bootmsg.h>
#include <radix/irq.h>
#include <radix/kernel.h>
#include <radix/mm.h>
#include <radix/multiboot.h>
#include <radix/percpu.h>
#include <radix/tasking.h>
#include <radix/vmm.h>

#include "mm/slab.h"

/* kernel entry point */
int kmain(struct multiboot_info *mbt)
{
	BOOT_OK_MSG("Kernel loaded\n");

	buddy_init(mbt);
	slab_init();
	vmm_init();

	acpi_init();
	irq_init();
	percpu_area_setup();

	tasking_init();
	irq_enable();

	extern void kbd_install(void);
	kbd_install();
	printf("\nWelcome to radix\n");

	return 0;
}
