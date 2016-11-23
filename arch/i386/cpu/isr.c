/*
 * arch/i386/cpu/isr.c
 * Copyright (C) 2016 Alexei Frolov
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

#include <untitled/irq.h>
#include <untitled/kernel.h>
#include <untitled/sys.h>
#include "idt.h"
#include "isr.h"
#include "pic.h"

#define NUM_EXCEPTIONS	32 /* CPU protected mode exceptions */
#define NUM_IRQS	16 /* Industry Standard Architecture IRQs */

/* +1 for syscall interrupt */
#define NUM_INTERRUPTS (NUM_EXCEPTIONS + NUM_IRQS + 1)

#define IRQ_BASE	0x20

/*
 * Routines to be called on interrupt.
 * They fill up a struct regs and then call the interrupt handler proper.
 */
extern void (*intr[NUM_INTERRUPTS])(void);

extern void isr_table_setup(void);

/* interrupt handler functions */
static void (*isr_handlers[256])(struct regs *);

/* hardware interrupt handler functions */
static void (*irq_handlers[NUM_IRQS])(struct regs *);

static void irq_generic(struct regs *r);

void load_interrupt_routines(void)
{
	size_t i;

	isr_table_setup();

	/* add exception routines */
	for (i = 0; i < NUM_EXCEPTIONS; ++i)
		idt_set(i, (uintptr_t)intr[i], 0x08, 0x8E);

	/* add irq routines */
	for (; i < NUM_INTERRUPTS - 1; ++i) {
		idt_set(i, (uintptr_t)intr[i], 0x08, 0x8E);
		isr_handlers[i] = irq_generic;
	}

	/* syscall interrupt */
	idt_set(SYSCALL_INTERRUPT, (uintptr_t)intr[SYSCALL_VECTOR], 0x08, 0x8E);

	/* remap IRQs to vectors 0x20 through 0x2F */
	pic_remap(IRQ_BASE, IRQ_BASE + 8);
}

#define CLI() asm volatile("cli")
#define STI() asm volatile("sti")

void install_interrupt_handler(uint32_t irqno, void (*hnd)(struct regs *))
{
	if (irqno >= NUM_IRQS)
		return;
	CLI();
	irq_handlers[irqno] = hnd;
	STI();
}

void uninstall_interrupt_handler(uint32_t irqno)
{
	if (irqno >= NUM_IRQS)
		return;
	CLI();
	irq_handlers[irqno] = NULL;
	STI();
}

static volatile int depth;

void interrupt_disable(void)
{
	/* interrupts were already enabled; this is first disable */
	if (irq_active())
		depth = 1;
	else
		++depth;

	CLI();
}

void interrupt_enable(void)
{
	if (depth == 0 || depth == 1)
		STI();
	else
		--depth;
}

static const char *exceptions[] = {
	"Division by zero",			/* 0x00 */
	"Debug",				/* 0x01 */
	"Non-maskable interrupt",		/* 0x02 */
	"Breakpoint",				/* 0x03 */
	"Overflow",				/* 0x04 */
	"Out of bounds",			/* 0x05 */
	"Invalid opcode",			/* 0x06 */
	"Device not available",			/* 0x07 */
	"Double fault",				/* 0x08 */
	"Coprocessor segment overrun",		/* 0x09 */
	"Invalid TSS",				/* 0x0A */
	"Segment not present",			/* 0x0B */
	"Stack fault",				/* 0x0C */
	"General protection fault",		/* 0x0D */
	"Page fault",				/* 0x0E */
	"Unknown exception",			/* 0x0F */
	"x87 floating-point exception",		/* 0x10 */
	"Alignment check",			/* 0x11 */
	"Machine check",			/* 0x12 */
	"SIMD floating-point exception",	/* 0x13 */
	"Virtualization exception",		/* 0x14 */
	"Unknown exception",			/* 0x15 */
	"Unknown exception",			/* 0x16 */
	"Unknown exception",			/* 0x17 */
	"Unknown exception",			/* 0x18 */
	"Unknown exception",			/* 0x19 */
	"Unknown exception",			/* 0x1A */
	"Unknown exception",			/* 0x1B */
	"Unknown exception",			/* 0x1C */
	"Unknown exception",			/* 0x1D */
	"Security exception",			/* 0x1E */
	"Unknown exception"			/* 0x1F */
};

void interrupt_handler(struct regs r)
{
	if (isr_handlers[r.intno]) {
		isr_handlers[r.intno](&r);
	} else if (r.intno < 0x20) {
		panic("unhandled CPU exception 0x%02X `%s'\n",
				r.intno, exceptions[r.intno]);
	}
}

#include <stdio.h>
#include <untitled/mm_types.h>

static void irq_generic(struct regs *r)
{
	irq_disable();

	if (irq_handlers[r->intno - IRQ_BASE])
		irq_handlers[r->intno - IRQ_BASE](r);

	/* temp debugging stuff */
	if (r->errno >= 0x23 && r->errno <= 0x26) {
		extern struct page *page_map;
		static int pfn = -1;

		if (pfn != -1) {
			switch (r->errno) {
			case 0x23:
				pfn -= 128;
				break;
			case 0x24:
				++pfn;
				break;
			case 0x25:
				--pfn;
				break;
			case 0x26:
				pfn += 128;
				break;
			}
		}
		if (pfn < 0)
			pfn = 0;

		printf("pfn: %u\n", pfn);
		printf("\tslab_cache:\t0x%08X\n", page_map[pfn].slab_cache);
		printf("\tslab_desc:\t0x%08X\n", page_map[pfn].slab_desc);
		printf("\tmem:\t\t0x%08X\n", page_map[pfn].mem);
		printf("\tstatus:\t\t0x%08X\n", page_map[pfn].status);
	}

	pic_eoi(r->intno - 0x20);
	irq_enable();
}
