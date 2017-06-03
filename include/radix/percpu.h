/*
 * include/radix/percpu.h
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

#ifndef RADIX_PERCPU_H
#define RADIX_PERCPU_H

/*
 * Heavily inspired by Linux
 * include/linux/percpu-defs.h
 */

#include <radix/asm/percpu.h>
#include <radix/compiler.h>

#define PER_CPU_SECTION __ARCH_PER_CPU_SECTION

#define DEFINE_PER_CPU(type, name) \
	__section(PER_CPU_SECTION) typeof(type) name

#endif /* RADIX_PERCPU_H */