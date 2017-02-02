/*
 * arch/i386/cpu/cpu.c
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

#include <stdio.h>
#include <string.h>
#include <untitled/cpu.h>
#include <untitled/kernel.h>

static long cpuid_max;

/* CPU vendor string */
static char vendor_id[16];

/* cpuid 0x1 information */
static long cpu_info[4];

static struct cache_info {
	unsigned long l1i_size;
	unsigned long l1i_line_size;
	unsigned long l1i_assoc;
	unsigned long l1d_size;
	unsigned long l1d_line_size;
	unsigned long l1d_assoc;
	unsigned long l2_size;
	unsigned long l2_line_size;
	unsigned long l2_assoc;
	unsigned long l3_size;
	unsigned long l3_line_size;
	unsigned long l3_assoc;

	unsigned long tlbi_page_size;
	unsigned long tlbi_entries;
	unsigned long tlbi_assoc;
	unsigned long tlbd_page_size;
	unsigned long tlbd_entries;
	unsigned long tlbd_assoc;

	unsigned long prefetching;
} cache_info;

enum {
	CACHE_ASSOC_2WAY,
	CACHE_ASSOC_4WAY,
	CACHE_ASSOC_6WAY,
	CACHE_ASSOC_8WAY,
	CACHE_ASSOC_12WAY,
	CACHE_ASSOC_16WAY,
	CACHE_ASSOC_24WAY,
	CACHE_ASSOC_FULL
};

#define PAGE_SIZE_4K     (1 << 0)
#define PAGE_SIZE_2M     (1 << 1)
#define PAGE_SIZE_4M     (1 << 2)
#define PAGE_SIZE_256M   (1 << 3)
#define PAGE_SIZE_1G     (1 << 4)

static void read_cache_info(void);

void read_cpu_info(void)
{
	long vendor[4];

	if (!cpuid_supported()) {
		vendor_id[0] = '\0';
		return;
	}

	cpuid(0, cpuid_max, vendor[0], vendor[2], vendor[1]);
	vendor[3] = 0;
	memcpy(vendor_id, vendor, sizeof vendor_id);

	cpuid(1, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);

	memset(&cache_info, 0, sizeof cache_info);
	if (cpuid_max >= 2) {
		read_cache_info();
	} else {
		/*
		 * If cache information cannot be read, assume
		 * default values from Intel Pentium P5.
		 */
		cache_info.l1i_size = _K(8);
		cache_info.l1i_line_size = 32;
		cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
		cache_info.l1d_size = _K(8);
		cache_info.l1d_line_size = 32;
		cache_info.l1d_assoc = CACHE_ASSOC_2WAY;
		cache_info.l2_size = _K(256);
		cache_info.l2_line_size = 32;
		cache_info.l2_assoc = CACHE_ASSOC_4WAY;

		cache_info.tlbi_page_size = PAGE_SIZE_4K;
		cache_info.tlbi_entries = 32;
		cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
		cache_info.tlbd_page_size = PAGE_SIZE_4K;
		cache_info.tlbd_entries = 64;
		cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
	}

	cache_str();
}

int cpu_has_apic(void)
{
	return cpu_info[3] & CPUID_APIC;
}

/*
 * read_cache_info:
 * Parse CPU cache and TLB information from cpuid 0x2.
 */
static void read_cache_info(void)
{
	long buf[4];
	int nreads, i;
	unsigned char *descriptor;

	nreads = 0;
	descriptor = (unsigned char *)buf;

	do {
		cpuid(2, buf[0], buf[1], buf[2], buf[3]);
		/* low byte of eax indicate number of times to call cpuid */
		if (!nreads)
			nreads = descriptor[0];

		for (i = 1; i < 16; ++i) {
			switch (descriptor[i]) {
			case 0x00:
				/* null descriptor */
				break;
			case 0x01:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 32;
				cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x02:
				cache_info.tlbi_page_size = PAGE_SIZE_4M;
				cache_info.tlbi_entries = 2;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x03:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 64;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x04:
				cache_info.tlbd_page_size = PAGE_SIZE_4M;
				cache_info.tlbd_entries = 8;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x05:
				cache_info.tlbd_page_size = PAGE_SIZE_4M;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x06:
				cache_info.l1i_size = _K(8);
				cache_info.l1i_line_size = 32;
				cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x08:
				cache_info.l1i_size = _K(16);
				cache_info.l1i_line_size = 32;
				cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x09:
				cache_info.l1i_size = _K(32);
				cache_info.l1i_line_size = 64;
				cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x0A:
				cache_info.l1d_size = _K(8);
				cache_info.l1d_line_size = 32;
				cache_info.l1d_assoc = CACHE_ASSOC_2WAY;
				break;
			case 0x0B:
				cache_info.tlbi_page_size = PAGE_SIZE_4M;
				cache_info.tlbi_entries = 4;
				cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x0C:
				cache_info.l1d_size = _K(16);
				cache_info.l1d_line_size = 32;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x0D:
				cache_info.l1d_size = _K(16);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x0E:
				cache_info.l1d_size = _K(24);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_6WAY;
				break;
			case 0x10:
				cache_info.l1d_size = _K(16);
				cache_info.l1d_line_size = 32;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x15:
				cache_info.l1i_size = _K(16);
				cache_info.l1i_line_size = 32;
				cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x1A:
				cache_info.l2_size = _K(96);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_6WAY;
				break;
			case 0x1D:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_2WAY;
				break;
			case 0x21:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x22:
				cache_info.l3_size = _K(512);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x23:
				cache_info.l3_size = _M(1);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x24:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0x25:
				cache_info.l3_size = _M(2);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x29:
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x2C:
				cache_info.l1d_size = _K(32);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x30:
				cache_info.l1i_size = _K(32);
				cache_info.l1i_line_size = 64;
				cache_info.l1i_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x39:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x3A:
				cache_info.l2_size = _K(192);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_6WAY;
				break;
			case 0x3B:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_2WAY;
				break;
			case 0x3C:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x3D:
				cache_info.l2_size = _K(384);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_6WAY;
				break;
			case 0x3E:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x41:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x42:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x43:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x44:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x45:
				cache_info.l2_size = _M(2);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x46:
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x47:
				cache_info.l3_size = _M(8);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x48:
				cache_info.l2_size = _M(3);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0x49:
				/* TODO: L3 on P4, L2 on Core 2 */
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0x4A:
				cache_info.l3_size = _M(6);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0x4B:
				cache_info.l3_size = _M(8);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0x4C:
				cache_info.l3_size = _M(12);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0x4D:
				cache_info.l3_size = _M(16);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0x4E:
				cache_info.l2_size = _M(6);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_24WAY;
				break;
			case 0x4F:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 32;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x50:
				cache_info.tlbi_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbi_entries = 64;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x51:
				cache_info.tlbi_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbi_entries = 128;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x52:
				cache_info.tlbi_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbi_entries = 256;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x55:
				cache_info.tlbi_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbi_entries = 7;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x56:
				cache_info.tlbd_page_size = PAGE_SIZE_4M;
				cache_info.tlbd_entries = 16;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x57:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 16;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x59:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 16;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x5A:
				cache_info.tlbd_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x5B:
				cache_info.tlbd_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 64;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x5C:
				cache_info.tlbd_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 128;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x5D:
				cache_info.tlbd_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 256;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x60:
				cache_info.l1d_size = _K(16);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x61:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 48;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x63:
				cache_info.tlbd_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x64:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 512;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x66:
				cache_info.l1d_size = _K(8);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x67:
				cache_info.l1d_size = _K(16);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x68:
				cache_info.l1d_size = _K(32);
				cache_info.l1d_line_size = 64;
				cache_info.l1d_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x6A:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 64;
				cache_info.tlbd_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x6B:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 256;
				cache_info.tlbd_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x6C:
				cache_info.tlbd_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 126;
				cache_info.tlbd_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x6D:
				cache_info.tlbd_page_size = PAGE_SIZE_1G;
				cache_info.tlbd_entries = 16;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x76:
				cache_info.tlbi_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbi_entries = 8;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x77:
				cache_info.l1i_size = _K(16);
				cache_info.l1i_line_size = 64;
				cache_info.l1i_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x78:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x79:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7A:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7B:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7C:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7D:
				cache_info.l2_size = _M(2);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7E:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 128;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x7F:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_2WAY;
				break;
			case 0x80:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x81:
				cache_info.l2_size = _K(128);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x82:
				cache_info.l2_size = _K(256);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x83:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x84:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x85:
				cache_info.l2_size = _M(2);
				cache_info.l2_line_size = 32;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x86:
				cache_info.l2_size = _K(512);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x87:
				cache_info.l2_size = _M(1);
				cache_info.l2_line_size = 64;
				cache_info.l2_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0x88:
				cache_info.l3_size = _M(2);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x89:
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x8A:
				cache_info.l3_size = _M(8);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0x8D:
				cache_info.l3_size = _M(3);
				cache_info.l3_line_size = 128;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0x90:
				cache_info.tlbi_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_256M;
				cache_info.tlbi_entries = 64;
				cache_info.tlbi_assoc = CACHE_ASSOC_FULL;
				break;
			case 0x96:
				cache_info.tlbd_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_256M;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0xA0:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_FULL;
				break;
			case 0xB0:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 128;
				cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xB1:
				cache_info.tlbi_page_size = PAGE_SIZE_4M;
				cache_info.tlbi_entries = 4;
				cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xB2:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 64;
				cache_info.tlbi_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xB3:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 128;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xB4:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 256;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xB5:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 64;
				cache_info.tlbi_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0xB6:
				cache_info.tlbi_page_size = PAGE_SIZE_4K;
				cache_info.tlbi_entries = 128;
				cache_info.tlbi_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0xBA:
				cache_info.tlbd_page_size = PAGE_SIZE_4K;
				cache_info.tlbd_entries = 64;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xC0:
				cache_info.tlbd_page_size = PAGE_SIZE_4K |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 8;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xC2:
				cache_info.tlbd_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 16;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xC4:
				cache_info.tlbd_page_size = PAGE_SIZE_2M |
				                            PAGE_SIZE_4M;
				cache_info.tlbd_entries = 32;
				cache_info.tlbd_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xD0:
				cache_info.l3_size = _K(512);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xD1:
				cache_info.l3_size = _M(1);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xD2:
				cache_info.l3_size = _M(2);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_4WAY;
				break;
			case 0xD6:
				cache_info.l3_size = _M(1);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0xD7:
				cache_info.l3_size = _M(2);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0xD8:
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_8WAY;
				break;
			case 0xDC:
				cache_info.l3_size = _K(1536);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0xDD:
				cache_info.l3_size = _M(3);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0xDE:
				cache_info.l3_size = _M(6);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_12WAY;
				break;
			case 0xE2:
				cache_info.l3_size = _M(2);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0xE3:
				cache_info.l3_size = _M(4);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0xE4:
				cache_info.l3_size = _M(8);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_16WAY;
				break;
			case 0xEA:
				cache_info.l3_size = _M(12);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_24WAY;
				break;
			case 0xEB:
				cache_info.l3_size = _M(18);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_24WAY;
				break;
			case 0xEC:
				cache_info.l3_size = _M(24);
				cache_info.l3_line_size = 64;
				cache_info.l3_assoc = CACHE_ASSOC_24WAY;
				break;
			case 0xF0:
				cache_info.prefetching = 64;
				break;
			case 0xF1:
				cache_info.prefetching = 128;
				break;
			default:
				break;
			}
		}
	} while (--nreads);
}

/*
 * Nothing but silly printing functions below.
 * Turn around now.
 */

static char cache_info_buf[512];

static char *cache_assoc_str(int assoc)
{
	switch(assoc) {
	case CACHE_ASSOC_2WAY:
		return "2-way";
	case CACHE_ASSOC_4WAY:
		return "4-way";
	case CACHE_ASSOC_6WAY:
		return "6-way";
	case CACHE_ASSOC_8WAY:
		return "8-way";
	case CACHE_ASSOC_12WAY:
		return "12-way";
	case CACHE_ASSOC_16WAY:
		return "16-way";
	case CACHE_ASSOC_24WAY:
		return "24-way";
	case CACHE_ASSOC_FULL:
		return "full";
	default:
		return "";
	}
}

static char *print_tlb(char *pos)
{
	int printed = 0;

	if (cache_info.tlbi_page_size) {
		pos += sprintf(pos, "TLBi:\t");
		if (cache_info.tlbi_page_size & PAGE_SIZE_4K) {
			pos += sprintf(pos, "4K");
			printed = 1;
		}
		if (cache_info.tlbi_page_size & PAGE_SIZE_2M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "2M");
			printed = 1;
		}
		if (cache_info.tlbi_page_size & PAGE_SIZE_4M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "4M");
			printed = 1;
		}
		if (cache_info.tlbi_page_size & PAGE_SIZE_256M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "256M");
			printed = 1;
		}
		if (cache_info.tlbi_page_size & PAGE_SIZE_1G) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "1G");
			printed = 1;
		}
		pos += sprintf(pos, " pages, %lu entries, %s associativity\n",
			       cache_info.tlbi_entries,
			       cache_assoc_str(cache_info.tlbi_assoc));
	}

	printed = 0;
	if (cache_info.tlbd_page_size) {
		pos += sprintf(pos, "TLBd:\t");
		if (cache_info.tlbd_page_size & PAGE_SIZE_4K) {
			pos += sprintf(pos, "4K");
			printed = 1;
		}
		if (cache_info.tlbd_page_size & PAGE_SIZE_2M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "2M");
			printed = 1;
		}
		if (cache_info.tlbd_page_size & PAGE_SIZE_4M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "4M");
			printed = 1;
		}
		if (cache_info.tlbd_page_size & PAGE_SIZE_256M) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "256M");
			printed = 1;
		}
		if (cache_info.tlbd_page_size & PAGE_SIZE_1G) {
			if (printed)
				*pos++ = '/';
			pos += sprintf(pos, "1G");
			printed = 1;
		}
		pos += sprintf(pos, " pages, %lu entries, %s associativity\n",
			       cache_info.tlbd_entries,
			       cache_assoc_str(cache_info.tlbd_assoc));
	}

	return pos;
}

char *print_caches(char *pos)
{
	if (cache_info.l1i_size) {
		pos += sprintf(pos, "L1i:\t%lu KiB, %lu byte lines, "
			       "%s associativity\n",
			       cache_info.l1i_size / _K(1),
			       cache_info.l1i_line_size,
			       cache_assoc_str(cache_info.l1i_assoc));
	}
	if (cache_info.l1d_size) {
		pos += sprintf(pos, "L1d:\t%lu KiB, %lu byte lines, "
			       "%s associativity\n",
			       cache_info.l1d_size / _K(1),
			       cache_info.l1d_line_size,
			       cache_assoc_str(cache_info.l1d_assoc));
	}
	if (cache_info.l2_size) {
		pos += sprintf(pos, "L2:\t%lu KiB, %lu byte lines, "
			       "%s associativity\n",
			       cache_info.l2_size / _K(1),
			       cache_info.l2_line_size,
			       cache_assoc_str(cache_info.l2_assoc));
	}
	if (cache_info.l3_size) {
		pos += sprintf(pos, "L3:\t%lu KiB, %lu byte lines, "
			       "%s associativity\n",
			       cache_info.l3_size / _K(1),
			       cache_info.l3_line_size,
			       cache_assoc_str(cache_info.l3_assoc));
	}
	return pos;
}

/*
 * cache_str:
 * Return a beautifully formatted string detailing CPU cache information.
 */
char *cache_str(void)
{
	char *pos;

	if (!cache_info_buf[0]) {
		pos = cache_info_buf;
		pos += sprintf(pos, "CPU cache information:\n");
		pos = print_tlb(pos);
		pos = print_caches(pos);
		if (cache_info.prefetching)
			pos += sprintf(pos, "%lu byte prefetching",
				       cache_info.prefetching);
		if (*--pos == '\n')
			*pos = '\0';
	}

	return cache_info_buf;
}