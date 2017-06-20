/*
 * arch/i386/include/radix/asm/cpu.h
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

#ifndef ARCH_I386_RADIX_CPU_H
#define ARCH_I386_RADIX_CPU_H

#ifndef RADIX_CPU_H
#error only <radix/cpu.h> can be included directly
#endif

#include <radix/asm/cpu_defs.h>
#include <radix/compiler.h>

static __always_inline unsigned long cpuid_supported(void)
{
	unsigned long res;

	asm volatile("pushf\n\t"
	             "pop %%eax\n\t"
	             "movl %%eax, %%ecx\n\t"
	             "xorl $0x200000, %%eax\n\t"
	             "push %%eax\n\t"
	             "popf\n\t"
	             "pushf\n\t"
	             "pop %%eax\n\t"
	             "xorl %%ecx, %%eax"
	             : "=a"(res)
	             :
	             : "%ecx");
	return res;
}

static __always_inline unsigned long cpu_read_cr2(void)
{
	unsigned long ret;

	asm volatile("mov %%cr2, %0" : "=r"(ret));
	return ret;
}

#define cpuid(eax, a, b, c, d)                                  \
	asm volatile("xchg %%ebx, %1\n\t"                       \
	             "cpuid\n\t"                                \
	             "xchg %%ebx, %1"                           \
	             : "=a"(a), "=r"(b), "=c"(c), "=d"(d)       \
	             : "0"(eax))

#define __modify_control_register(cr, clear, set)               \
	asm volatile("movl %%" cr ", %%eax\n\t"                 \
	             "andl %0, %%eax\n\t"                       \
	             "orl %1, %%eax\n\t"                        \
	             "movl %%eax, %%" cr                        \
	             :                                          \
	             : "r"(~clear), "r"(set)                    \
	             : "%eax", "%edx")

#define cpu_modify_cr0(clear, set) \
	__modify_control_register("cr0", clear, set)

#define cpu_modify_cr4(clear, set) \
	__modify_control_register("cr4", clear, set)

#include <radix/types.h>

int cpu_supports(uint64_t features);

#define __arch_cache_line_size i386_cache_line_size
#define __arch_cache_str       i386_cache_str

unsigned long i386_cache_line_size(void);
char *i386_cache_str(void);

#endif /* ARCH_I386_RADIX_CPU_H */
