/*
 * kernel/mm/vmm.c
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

#include <radix/kernel.h>
#include <radix/mm.h>
#include <radix/slab.h>
#include <radix/vmm.h>

#include <rlibc/string.h>

struct vmm_block {
	addr_t          base;
	size_t          size;
	unsigned long   flags;
	struct list     global_list;
	struct list     size_list;
	struct rb_node  size_node;
	struct rb_node  addr_node;
};

#define VMM_ALLOCATED (1 << 0)

static struct slab_cache *vmm_block_cache;

static struct vmm_structures vmm_kernel = {
	.block_list = LIST_INIT(vmm_kernel.block_list),
	.addr_tree = RB_ROOT,
	.size_tree = RB_ROOT
};

static void vmm_block_init(void *p)
{
	struct vmm_block *block = p;

	list_init(&block->global_list);
	list_init(&block->size_list);
	rb_init(&block->size_node);
	rb_init(&block->addr_node);
}

/*
 * vmm_size_tree_insert:
 * Insert `block` into given VMM block size tree.
 */
static void vmm_size_tree_insert(struct rb_root *tree, struct vmm_block *block)
{
	struct rb_node **pos, *parent;
	struct vmm_block *curr;

	pos = &tree->root_node;
	parent = NULL;

	while (*pos) {
		curr = rb_entry(*pos, struct vmm_block, size_node);
		parent = *pos;

		if (block->size < curr->size) {
			pos = &(*pos)->left;
		} else if (block->size > curr->size) {
			pos = &(*pos)->right;
		} else {
			list_ins(&curr->size_list, &block->size_list);
			return;
		}
	}

	rb_link(&block->size_node, parent, pos);
	rb_balance(tree, &block->size_node);
}

/*
 * vmm_addr_tree_insert:
 * Insert `block` into given VMM block address tree.
 */
static void vmm_addr_tree_insert(struct rb_root *tree, struct vmm_block *block)
{
	struct rb_node **pos, *parent;
	struct vmm_block *curr;

	pos = &tree->root_node;
	parent = NULL;

	while (*pos) {
		curr = rb_entry(*pos, struct vmm_block, addr_node);
		parent = *pos;

		if (block->base < curr->base)
			pos = &(*pos)->left;
		else if (block->base > curr->base)
			pos = &(*pos)->right;
		else
			return;
	}

	rb_link(&block->addr_node, parent, pos);
	rb_balance(tree, &block->addr_node);
}

/*
 * vmm_tree_insert:
 * Insert a vmm_block into both address and size trees.
 */
static __always_inline void vmm_tree_insert(struct vmm_structures *s,
                                            struct vmm_block *block)
{
	vmm_addr_tree_insert(&s->addr_tree, block);
	vmm_size_tree_insert(&s->size_tree, block);
}

/*
 * vmm_tree_delete:
 * Delete a vmm_block from both address and size trees.
 */
static __always_inline void vmm_tree_delete(struct vmm_structures *s,
                                            struct vmm_block *block)
{
	rb_delete(&s->addr_tree, &block->addr_node);
	rb_delete(&s->size_tree, &block->size_node);
}

void vmm_init(void)
{
	struct vmm_block *first;

	vmm_block_cache = create_cache("vmm_block", sizeof (struct vmm_block),
	                               SLAB_MIN_ALIGN, SLAB_PANIC,
	                               vmm_block_init, vmm_block_init);

	first = alloc_cache(vmm_block_cache);
	if (IS_ERR(first))
		panic("failed to allocate intial vmm_block: %s\n",
		      strerror(ERR_VAL(first)));

	first->base = RESERVED_VIRT_BASE;
	first->size = RESERVED_SIZE;
	first->flags = 0;

	list_add(&vmm_kernel.block_list, &first->global_list);
	vmm_tree_insert(&vmm_kernel, first);
}

/*
 * vmm_split:
 * Split a single vmm_block into multiple blocks, one of which
 * starts at address `base` and is of length `size`.
 * Return this block.
 *
 * `block` MUST be large enough to fit `base` + `size`.
 */
static struct vmm_block *vmm_split(struct vmm_block *block,
                                   struct vmm_structures *s,
                                   addr_t base, size_t size)
{
	struct vmm_block *new;
	size_t new_size, end;

	new_size = base - block->base;
	end = block->base + block->size;

	/* create a new block [block->base, base) */
	if (new_size) {
		block->size = new_size;
		rb_delete(&s->size_tree, &block->size_node);
		vmm_size_tree_insert(&s->size_tree, block);

		new = alloc_cache(vmm_block_cache);
		if (IS_ERR(new))
			return new;

		new->base = base;
		new->size = size;
		new->flags = 0;
		list_add(&block->global_list, &new->global_list);
		block = new;
	} else if (size != block->size) {
		/* base == block->base */
		block->size = size;
		vmm_tree_delete(s, block);
	}

	new_size = end - (block->base + block->size);

	/* create a new block [block->base + block->size, end) */
	if (new_size) {
		new = alloc_cache(vmm_block_cache);
		if (IS_ERR(new))
			return new;

		new->base = block->base + block->size;
		new->size = new_size;
		new->flags = 0;
		list_add(&block->global_list, &new->global_list);
		vmm_tree_insert(s, new);
	}

	return block;
}