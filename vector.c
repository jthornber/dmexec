#include "vector.h"
#include "vm.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

//----------------------------------------------------------------

// d must be a power of 2
static unsigned div_up_pow(unsigned n, unsigned d)
{
	unsigned mask = d - 1;
	return (n + mask) / d;
}

//----------------------------------------------------------------

Vector *v_empty()
{
	static Vector *empty = NULL;

	if (!empty)
		empty = mm_zalloc(VECTOR, sizeof(Vector));

	return empty;
}

static Vector *v_shadow(Vector *v)
{
	return v->transient ? v : mm_clone(v);
}

unsigned v_size(Vector *v)
{
	return v->size;
}

#define VBLOCK_SIZE (sizeof(Value) * ENTRIES_PER_VBLOCK)

static VBlock vb_alloc()
{
	return mm_alloc(VBLOCK, VBLOCK_SIZE);
}

// FIXME: remove
static VBlock vb_clone(VBlock vb)
{
	return mm_clone(vb);
}

//----------------------------------------------------------------
// Manipulating a constructed tree

// FIXME: use gcc builtins to speed up.
static inline unsigned size_to_levels_(unsigned size)
{
	unsigned r = 0;

	assert(size);
	size--; // convert to a zero index
	do {
		size = size >> RADIX_SHIFT;
		r++;
	} while (size);

	return r;
}

// leaves are level 0
static inline unsigned level_index_(unsigned i, unsigned level)
{
	return (i >> (RADIX_SHIFT * level)) & RADIX_MASK;
}

// Committing the cursor doesn't change the logical state of the vector, so we
// don't create a new Vector object.  However a new vblock spine is created
// otherwise we'd break sharing.
static VBlock insert_cursor_(VBlock cursor, unsigned bi, VBlock vb, unsigned level)
{
	if (level) {
		vb = vb_clone(vb);
		unsigned index = level_index_(bi, level - 1);
		vb[index].ptr = insert_cursor_(cursor, bi, vb[index].ptr, level - 1);
		return vb;

	} else
		return cursor;
}

static void commit_cursor_(Vector *v)
{
	if (v->cursor && v->cursor_dirty) {
		unsigned levels = size_to_levels_(v->size);
		v->root = insert_cursor_(v->cursor, v->cursor_index, v->root, levels - 1);
		v->cursor_dirty = false;
	}
}

static void prep_cursor__(Vector *v, unsigned i, unsigned bi)
{
	VBlock vb = v->root;
	unsigned level = size_to_levels_(v->size);

	while (--level)
		vb = vb[level_index_(i, level)].ptr;

	v->cursor = vb;
	v->cursor_index = bi;
	v->cursor_dirty = false;
}

static void prep_cursor_(Vector *v, unsigned i)
{
	unsigned bi;

	if (i > v->size)
		error("vector index out of bounds");

	bi = i >> RADIX_SHIFT;
	if (!v->cursor || bi != v->cursor_index) {
		commit_cursor_(v);
		prep_cursor__(v, i, bi);
	}
}

Value v_ref(Vector *v, unsigned i)
{
	prep_cursor_(v, i);
	return v->cursor[level_index_(i, 0)];
}

static void shadow_cursor_(Vector *v)
{
	if (!v->cursor_dirty || !v->transient) {
		v->cursor = vb_clone(v->cursor);
		v->cursor_dirty = true;
	}
}

Vector *v_set(Vector *v, unsigned i, Value val)
{
	v = v_shadow(v);
	prep_cursor_(v, i);
	shadow_cursor_(v);
	v->cursor[level_index_(i, 0)] = val;
	return v;
}

//----------------------------------------------------------------

static unsigned full_tree(unsigned levels)
{
	return 1u << (RADIX_SHIFT * levels);
}

static unsigned tail_entries(unsigned size, unsigned levels)
{
	return div_up_pow(size, full_tree(levels - 1));
}

static VBlock trim_(VBlock vb, unsigned size, unsigned level)
{
	vb = vb_clone(vb);
	unsigned i, te = tail_entries(size, level);
	for (i = te; i < ENTRIES_PER_VBLOCK; i++)
		vb[i] = mk_nil();

	if (level && (size = size % full_tree(level - 1)))
		vb[te - 1].ptr = trim_(vb[te - 1].ptr, size, level - 1);

	return vb;
}

static Vector *shrink_(Vector *v, unsigned new_size)
{
	commit_cursor_(v);
	v = v_shadow(v);

	v->size = new_size;

	// Drop entries beyond new_size so they can be GCd.
	v->root = trim_(v->root, v->size, size_to_levels_(v->size));
	v->cursor = NULL;
	v->cursor_dirty = false;

	return v;
}

// We know the tree is going to be treated in an immutable way, so we can share
// sub trees.
static VBlock alloc_tree_(unsigned level, Value init)
{
	Value v;
	unsigned i;
	VBlock vb = vb_alloc();

	if (level)
		v.ptr = alloc_tree_(level - 1, init);
	else
		v = init;

	for (i = 0; i < ENTRIES_PER_VBLOCK; i++)
		vb[i] = v;

	return vb;
}

// Copies entries from rhs to the tail block of each lhs level.
static VBlock merge_bottom_levels_(VBlock lhs, unsigned lhs_size,
		      	           VBlock rhs, unsigned levels)
{
	VBlock vb = vb_clone(lhs);
	unsigned te = tail_entries(lhs_size, levels);

	if (te != ENTRIES_PER_VBLOCK)
		memcpy(vb + te, rhs + te, sizeof(Value) * (ENTRIES_PER_VBLOCK - te));

	// deliberate assignment in conditional
	if (levels && (lhs_size = lhs_size % full_tree(levels - 1))) {
		vb[te - 1].ptr = merge_bottom_levels_(lhs[te - 1].ptr, lhs_size,
						      rhs[te - 1].ptr, levels - 1);
	}

	return vb;
}

// Takes the left most path down rhs.
static VBlock merge_top_levels_(VBlock lhs, unsigned lhs_size,
				VBlock rhs, unsigned rhs_levels)
{
	VBlock vb = vb_clone(rhs);
	unsigned lhs_levels = size_to_levels_(lhs_size);

	if (--rhs_levels > lhs_levels)
		vb[0].ptr = merge_top_levels_(lhs, lhs_size, rhs[0].ptr, rhs_levels);
	else
		vb[0].ptr = merge_bottom_levels_(lhs, lhs_size, rhs[0].ptr, rhs_levels);

	return vb;
}

static VBlock merge_trees_(VBlock lhs, unsigned lhs_size,
		 	   VBlock rhs, unsigned rhs_size)
{
	if (!lhs_size)
		return rhs;

	else {
		unsigned llevels = size_to_levels_(lhs_size);
		unsigned rlevels = size_to_levels_(rhs_size);

		assert(rlevels >= llevels);

		if (rlevels > llevels)
			return merge_top_levels_(lhs, lhs_size, rhs, rlevels);
		else
			return merge_bottom_levels_(lhs, lhs_size, rhs, rlevels);
	}
}

static Vector *merge_(Vector *lhs, VBlock rhs, unsigned rhs_size)
{
	Vector *v;

	commit_cursor_(lhs);

	v = v_shadow(lhs);
	v->root = merge_trees_(lhs->root, lhs->size, rhs, rhs_size);
	v->size = rhs_size;
	v->cursor = NULL;
	return v;
}

static Vector *grow_(Vector *v, unsigned new_size, Value init)
{
	return merge_(v, alloc_tree_(size_to_levels_(new_size) - 1, init),
		      new_size);
}

Vector *v_resize(Vector *v, unsigned new_size, Value init)
{
	if (new_size > v->size)
		return grow_(v, new_size, init);

	else if (new_size < v->size)
		return shrink_(v, new_size);

	else
		return v;
}

Vector *v_append(Vector *v, Value val)
{
	return grow_(v, v->size + 1, val);
}

//----------------------------------------------------------------

Vector *v_transient_begin(Vector *v)
{
	commit_cursor_(v);
	v = mm_clone(v);
	v->transient = true;
	return v;
}

void v_transient_end(Vector *v)
{
	v->transient = false;
}

//----------------------------------------------------------------

