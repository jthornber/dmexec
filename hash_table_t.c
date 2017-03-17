#include "equality.h"
#include "hash_table.h"
#include "mm.h"
#include "symbol.h"
#include "vm.h"

#include <assert.h>
#include <stdio.h>

//----------------------------------------------------------------

static void t_empty()
{
	ht_empty();
}

static void t_single_entry()
{
	Value v;
	HashTable *ht = ht_empty();
	HashTable *ht2 = ht_insert(ht, mk_fixnum(1), mk_fixnum(123));
	assert(ht2 != ht);

	assert(ht_lookup(ht2, mk_fixnum(1), &v));
	assert(equalp(mk_fixnum(123), v));;

	assert(!ht_lookup(ht, mk_fixnum(1), &v));
}

static void t_two_entries()
{
	Value v;
	HashTable *ht = ht_empty();
	HashTable *ht2 = ht_insert(ht, mk_fixnum(1), mk_fixnum(123));
	HashTable *ht3 = ht_insert(ht2, mk_fixnum(2), mk_fixnum(234));
	assert(ht3 != ht2);

	assert(ht_lookup(ht3, mk_fixnum(2), &v));
	assert(equalp(mk_fixnum(234), v));

	assert(ht_lookup(ht3, mk_fixnum(1), &v));
	assert(equalp(mk_fixnum(123), v));

	assert(ht_lookup(ht2, mk_fixnum(1), &v));
	assert(equalp(mk_fixnum(123), v));

	assert(!ht_lookup(ht2, mk_fixnum(2), &v));
	assert(!ht_lookup(ht, mk_fixnum(2), &v));
	assert(!ht_lookup(ht, mk_fixnum(1), &v));
}

static void insert_many(unsigned count)
{
	Value v;
	unsigned i;
	HashTable *ht = ht_empty();

	for (i = 0; i < count; i++) {
		ht = ht_insert(ht, mk_fixnum(i), mk_fixnum(i * i));
		assert(ht_lookup(ht, mk_fixnum(i), &v));
		assert(equalp(mk_fixnum(i * i), v));

		if (!(i % (16 * 1024))) {
			Value val = mk_ref(ht);
			mm_garbage_collect(&val, 1);
		}
	}

	for (i = 0; i < count; i++) {
		assert(ht_lookup(ht, mk_fixnum(i), &v));
		assert(equalp(mk_fixnum(i * i), v));
	}
}
static void t_single_level()
{
	insert_many(16);
}

static void t_two_levels()
{
	insert_many(256);
}

static void t_100k()
{
	insert_many(1024 * 1024);
}

//----------------------------------------------------------------

static size_t total_allocated_()
{
	return memory_stats_.total_allocated;
}

static void indent(unsigned n)
{
	while (n--)
		fputc(' ', stderr);
}

static void run(const char *name, void (*fn)())
{
	size_t before, after;

	fprintf(stderr, "%s", name);
	indent(24 - strlen(name));
	fprintf(stderr, "... ");
	before = total_allocated_();

	fn();

	after = total_allocated_();
	fprintf(stderr, "%llu\n", (unsigned long long) (after - before));
}

int main(int argc, const char *argv[])
{
	mm_init(96 * 1024 * 1024);
	run("empty", t_empty);
	run("single entry", t_single_entry);
	run("two entries", t_two_entries);
	run("single level", t_single_level);
	run("two levels", t_two_levels);
	run("100k", t_100k);
	mm_exit();

	return 0;
}

//----------------------------------------------------------------

