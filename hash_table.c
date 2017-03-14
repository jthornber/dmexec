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
	return hash_u32(as_fixnum(v)) & (0xf << (level * 4));
}

//----------------------------------------------------------------

HBlock hb_alloc_(unsigned nr_entries)
{
	return mm_alloc(HBLOCK, sizeof(HashEntry) * nr_entries);
}

HashTable *ht_empty()
{
	HashTable *ht = mm_alloc(HTABLE, sizeof(*ht));
	ht->nr_entries = 0;
	return ht;
}

static unsigned pop_count(uint32_t bits, unsigned bits_below)
{
	bits &= (1 << bits_below) - 1;
	return __builtin_popcount(bits);
}

static void set_bit(uint32_t *word, unsigned bit)
{
	(*word) |= (1 << bit);
}

static uint32_t test_bit(uint32_t *word, unsigned bit)
{
	return (*word) & (1 << bit);
}

bool ht_lookup(HashTable *ht, Value k, Value *v)
{
	if (!ht->nr_entries)
		return false;

	if (get_type(ht->root.val) == HBLOCK) {
		uint32_t h = hash(k, 0);
		uint32_t bits = ht->root.map;
		if (test_bit(&bits, h)) {
			HBlock hb = ht->root.val.ptr;
			HashEntry *he = hb + pop_count(bits, h);
			*v = he->val;
			return equalp(k, he->key);
		}

	} else if (equalp(ht->root.key, k)) {
		*v = ht->root.val;
		return true;
	}

	return false;
}

static int cmp_he(const void *l, const void *r)
{
	const HashEntry *lhs = l, *rhs = r;
	unsigned hl = hash(lhs->key, 0);
	unsigned hr = hash(rhs->key, 0);

	if (hl < hr)
		return -1;

	else if (hl > hr)
		return 1;

	else
		return 0;
}

static void hb_sort(HBlock hb, unsigned nr_entries)
{
	qsort(hb, nr_entries, sizeof(*hb), cmp_he);
}

HashTable *ht_insert(HashTable *ht, Value k, Value v)
{
	ht = mm_clone(ht);

	if (!ht->nr_entries) {
		ht->root.key = k;
		ht->root.val = v;
		assert(get_type(ht->root.key) == FIXNUM);

	} else if (ht->nr_entries == 1) {
		HashEntry new_root;
		memset(&new_root, 0, sizeof(new_root));

		HBlock hb = hb_alloc_(2);
		hb[0].key = k;
		hb[0].val = v;
		hb[1].key = ht->root.key;
		hb[1].val = ht->root.val;
		hb_sort(hb, 2);
		set_bit(&new_root.map, hash(hb[0].key, 0));
		set_bit(&new_root.map, hash(hb[1].key, 0));
		new_root.val = mk_ref(hb);
		ht->root = new_root;

	} else {
		unsigned h;
		HashEntry new_root;
		memcpy(&new_root, &ht->root, sizeof(new_root));
		HBlock hb = hb_alloc_(ht->nr_entries + 1);
		memcpy(hb, ht->root.val.ptr, sizeof(HashEntry) * ht->nr_entries);
		hb[ht->nr_entries].key = k;
		hb[ht->nr_entries].val = v;
		hb_sort(hb, ht->nr_entries + 1);
		h = hash(k, 0);
		if (test_bit(&new_root.map, h)) {
			fprintf(stderr, "collision\n");
			exit(1);
		}
		set_bit(&new_root.map, h);
		new_root.val = mk_ref(hb);
		ht->root = new_root;
	}

	ht->nr_entries++;
	return ht;
}

HashTable *ht_erase(HashTable *ht, Value k)
{
	return false;
}

HashTable *ht_transient_begin(HashTable *ht)
{
	return ht;
}

void ht_transient_end(HashTable *ht)
{

}

//----------------------------------------------------------------

