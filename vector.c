#include "vector.h"
#include "vm.h"

#include <stdbool.h>
#include <string.h>

//----------------------------------------------------------------

#define RADIX_SHIFT 5u
#define RADIX_MASK ((1u << RADIX_SHIFT) - 1u)
#define ENTRIES_PER_BLOCK (1u << RADIX_SHIFT)

typedef Value *VBlock;

struct __vector {
	unsigned size;
	unsigned cursor_index;

	VBlock root;
	VBlock cursor;
	bool cursor_dirty;
};

Vector *v_alloc()
{
	return zalloc(VECTOR, sizeof(Vector));
}

unsigned v_size(Vector *v)
{
	return v->size;
}

#define VBLOCK_SIZE (sizeof(Value) * ENTRIES_PER_BLOCK)

static VBlock vb_alloc()
{
	return untyped_alloc(VBLOCK_SIZE);
}

static VBlock vb_clone(VBlock vb)
{
	return untyped_clone(vb, VBLOCK_SIZE);
}

//----------------------------------------------------------------
// Manipulating a constructed tree

// FIXME: use gcc builtins to speed up.
static inline unsigned size_to_levels_(unsigned size)
{
	unsigned r = 0;
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
static void commit_cursor__(Vector *v)
{
	VBlock *vb = &v->root;
	unsigned level = size_to_levels_(v->size);

	while (--level) {
		*vb = vb_clone(*vb);
		vb = vb + level_index_(v->cursor_index, level);
	}

	*vb = v->cursor;  // already cloned when mutated
	v->cursor_dirty = false;
}

static void commit_cursor_(Vector *v)
{
	if (v->cursor && v->cursor_dirty)
		commit_cursor__(v);
}

static void prep_cursor__(Vector *v, unsigned i, unsigned bi)
{
	VBlock vb = v->root;
	unsigned level = size_to_levels_(v->size);

	while (--level)
		vb = as_ref(vb[level_index_(i, level)]);

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

Vector *v_set(Vector *v, unsigned i, Value val)
{
	v = clone(v);
	prep_cursor_(v, i);
	v->cursor = vb_clone(v->cursor);
	v->cursor[level_index_(i, 0)] = val;
	v->cursor_dirty = true;
	return v;
}

//----------------------------------------------------------------
// Extending a tree
#if 0
static VBlock insert(VBlock vb, unsigned level, unsigned i, Value val)
{
	VBlock new = vb_clone(vb);

	if (level == 0)
		new[level_index_(i, 0)] = val;
	else {

		new[level_index(i, level)] = mk_ref(insert(vb, level - 1, i, val));
	}

	return new;
}
#endif

static Vector *shrink_(Vector *v, unsigned new_size)
{
#if 0
	Vector *new = clone(v);
	commit_cursor_(new);
#endif
	return v;
}

// Assumes the vec has already been cloned
static void add_leaf_(Vector *v)
{
	// FIXME: finish
	
	// For a given size, each layer needs a given number of vblocks.
	v->root = vb_alloc();
}

static Vector *grow_(Vector *v, unsigned new_size, Value init)
{
	// FIXME: only grows by one entry
	v = clone(v);
	if (!(v->size & RADIX_MASK))
		add_leaf_(v);
	v->size++;
	prep_cursor_(v, v->size - 1);
	v->cursor = vb_clone(v->cursor);
	v->cursor[level_index_(v->size - 1, 0)] = init;
	v->cursor_dirty = true;
	return v;
#if 0
	// FIXME: finish
	Vector *new = clone(v);
	unsigned new_size = v->size + 1;

	// do we need to add a new level?
	if (size_to_levels(new_size) > size_to_levels_(v->size))
		add_level(new);

	// do we need to add a new vblock?
	if ((new_size & RADIX_MASK) == 1) {
		vb = vb_alloc();
		insert_vb(new, vb, new_size >> RADIX_SHIFT);
	}

	v->size = new_size;
	v = prep_cursor(v, new_size - 1);
	v->cursor[level_index(new_size - 1, 0)] = val;
	return v;
#endif
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
