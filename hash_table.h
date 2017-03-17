#ifndef DMEXEC_HASH_H
#define DMEXEC_HASH_H

#include "mm.h"

#include <stdbool.h>

//----------------------------------------------------------------

HashTable *ht_empty();

// FIXME: keys can only be fixnum atm.
HashTable *ht_insert(HashTable *ht, Value k, Value v);
bool ht_lookup(HashTable *ht, Value k, Value *v);
HashTable *ht_erase(HashTable *ht, Value k);

//----------------------------------------------------------------

#endif
