#include <stdlib.h>

#include "log.h"
#include "stdtypes.h"

#include "btree.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

int main(int argc, const char** argv) {
  LOG("run");

  // Insert a = hi
  // Get a

  BTree tree;
  CHECK0(btree_init(&tree));
  char* keys[] = {
    // "the00000", "quick000", "brown000", "fox00000", "jumped00", "over0000", "the00000", "lazy0000", "dog00000",
    // "the10000", "quick100", "brown100", "fox10000", "jumped10", "over1000", "the10000", "lazy1000", "dog10000",
    "a",
    "g",
    "c",
    "e",
    "f",
    "d",
    "h",
    "b",
    "z",
    "i",
    "j",
  };

  for (int i = 0; i < ARRAY_SIZE(keys); ++i) {
    LOG("insert %s", keys[i]);
    Bytes key = str_from_c(keys[i]);
    Bytes val = str_from_c(keys[i]);
    BTreeRecord record = {key, val};
    CHECK0(btree_insert(&tree, record));
    LOG("x");
    btree_debug_print(&tree);
    LOG("x1");
  }

  BTreeIter it;
  Bytes key = str_from_c("a0000000");
  CHECK0(btree_find(&tree, key, &it));

  BTreeRecord found;
  CHECK0(btree_next(&tree, &it, &found));

  LOG("found key=%.*s", (int)found.key.len, found.key.buf);
  LOG("found val=%.*s", (int)found.val.len, found.val.buf);

  btree_deinit(&tree);

  return 0;
}
