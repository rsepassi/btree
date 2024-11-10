#include <stdlib.h>

#include "btree.h"
#include "log.h"
#include "stdtypes.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

int main(int argc, const char **argv) {
  BTree tree;
  CHECK0(btree_init(&tree));

  char *keys[] = {
      "a", "g", "c", "e", "f", "d", "h", "b", "z", "i", "j",
  };

  for (int i = 0; i < ARRAY_SIZE(keys); ++i) {
    LOG("insert %s", keys[i]);
    Bytes key = str_from_c(keys[i]);
    Bytes val = str_from_c(keys[i]);
    BTreeRecord record = {key, val};
    CHECK0(btree_insert(&tree, record));
    btree_debug_print(&tree);
  }

  BTreeVal val;
  Bytes key = str_from_c("c");
  CHECK0(btree_find(&tree, key, &val));
  CHECK(str_eq(key, val));

  BTreeIter it;
  CHECK0(btree_iter(&tree, key, &it));

  BTreeRecord found;
  for (int i = 0; i < 9; ++i) {
    CHECK0(btree_next(&it, &found));
    LOG("iter key=%.*s val=%.*s", (int)found.key.len, found.key.buf,
        (int)found.val.len, found.val.buf);
  }
  CHECK(btree_next(&it, &found) == BTree_END);

  btree_deinit(&tree);

  return 0;
}
