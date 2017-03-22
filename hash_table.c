#include "hash_table.h"

#include "equality.h"
#include "slab.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//----------------------------------------------------------------
// This scheme is based on the paper Ideal Hash Trees by Phil Bagwell.
//----------------------------------------------------------------

#define GOLDEN_RATIO_32 0x9e3779b9

// FIXME: check this is decent
static uint32_t hash_u32(uint32_t n)
{
	return GOLDEN_RATIO_32 * n;
}

static uint32_t hash_ptr(void *ptr)
{
	return hash_u32((uint32_t) ptr);
}

static uint32_t hash(Value v, unsigned level)
{
	assert(get_type(v) == FIXNUM);
	return (hash_u32(as_fixnum(v)) >> (level * 4)) & 0xf;
}

//----------------------------------------------------------------

HBlock hb_alloc_(unsigned nr_entries)
{
	return mm_alloc(HBLOCK, sizeof(HashEntry) * nr_entries);
}

// Returns a clone with one extra entry
static HBlock hb_extend(HBlock hb)
{
	size_t s = get_obj_size(hb);
	HBlock new = mm_alloc(HBLOCK, s + sizeof(HashEntry));
	assert((s / sizeof(HashEntry)) < ENTRIES_PER_HBLOCK);
	memcpy(new, hb, s);
	return new;
}

HashTable *ht_empty()
{
	HashTable *ht = mm_alloc(HTABLE, sizeof(*ht));
	ht->nr_entries = 0;
	return ht;
}

unsigned ht_size(HashTable *ht)
{
	return ht->nr_entries;
}

static unsigned pop_count(uint32_t bits, unsigned bits_below)
{
	bits &= (1 << bits_below) - 1;
	return __builtin_popcount(bits);
}

static inline void set_bit(uint32_t *word, unsigned bit)
{
	(*word) |= (1 << bit);
}

static inline uint32_t test_bit(uint32_t *word, unsigned bit)
{
	return (*word) & (1 << bit);
}

static bool lookup_(HashEntry *he, Value k, Value *v, unsigned level)
{
	if (get_type(he->val) == HBLOCK) {
		uint32_t h = hash(k, level);
		uint32_t bits = he->map;

		if (test_bit(&bits, h)) {
			HBlock hb = he->val.ptr;
			unsigned index = pop_count(bits, h);
			return lookup_(hb + index, k, v, level + 1);
		}

		return false;

	} else if (equalp(he->key, k)) {
		*v = he->val;
		return true;

	} else
		return false;
}

bool ht_lookup(HashTable *ht, Value k, Value *v)
{
	if (!ht->nr_entries)
		return false;

	return lookup_(&ht->root, k, v, 0);
}

static int cmp_he(const void *l, const void *r, void *level_)
{
	const HashEntry *lhs = l, *rhs = r;
	unsigned *level = level_;
	unsigned hl = hash(lhs->key, *level);
	unsigned hr = hash(rhs->key, *level);

	if (hl < hr)
		return -1;

	else if (hl > hr)
		return 1;

	else {
		// hashes must always be unique within an hblock
		assert(0);
		return 0;
	}
}

static void hb_sort(HBlock hb, unsigned nr_entries, unsigned level)
{
	qsort_r(hb, nr_entries, sizeof(HashEntry), cmp_he, &level);
}

static void insert_(HashEntry *root, Value k, Value v, unsigned level)
{
	HBlock hb;

	if (get_type(root->val) == HBLOCK) {
		unsigned h;
		unsigned nr_entries = get_obj_size(root->val.ptr) / sizeof(HashEntry);

		h = hash(k, level);
		if (test_bit(&root->map, h)) {
			root->val.ptr = hb = mm_clone(root->val.ptr);
			insert_(hb + pop_count(root->map, h), k, v, level + 1);

		} else {
			set_bit(&root->map, h);
			hb = hb_extend(root->val.ptr);
			hb[nr_entries].key = k;
			hb[nr_entries].val = v;
			hb_sort(hb, nr_entries + 1, level);
			root->val.ptr = hb;
		}
	} else {
		HashEntry he = *root;

		// If there's a hash clash we create an interim
		// hblock with a single entry.
		if (hash(k, level) == hash(he.key, level)) {
			HBlock hb = hb_alloc_(1);
			hb[0].key = he.key;
			hb[0].val = he.val;
			root->map = 0;
			set_bit(&root->map, hash(he.key, level));
			root->val.ptr = hb;

			return insert_(hb, k, v, level + 1);

		} else {
			assert(hash(k, level) != hash(he.key, level));

			HBlock hb = hb_alloc_(2);
			hb[0].key = k;
			hb[0].val = v;
			hb[1].key = he.key;
			hb[1].val = he.val;
			hb_sort(hb, 2, level);

			root->map = 0;
			set_bit(&root->map, hash(k, level));
			set_bit(&root->map, hash(he.key, level));
			root->val.ptr = hb;
		}
	}
}

HashTable *ht_insert(HashTable *ht, Value k, Value v)
{
	ht = mm_clone(ht);

	if (!ht->nr_entries) {
		ht->root.key = k;
		ht->root.val = v;
	} else
		insert_(&ht->root, k, v, 0);

	ht->nr_entries++;
	return ht;
}

HashTable *ht_erase(HashTable *ht, Value k)
{
	return false;
}

//----------------------------------------------------------------

