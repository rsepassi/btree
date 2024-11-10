// An in-memory B-Tree

#include "stdtypes.h"

typedef enum {
  BTree_OK,
  BTree_ERROR,
  BTree_NOT_FOUND,
  BTree_END,
} BTreeStatus;

typedef struct {
  void *root;
  u32 height;
} BTree;

typedef Bytes BTreeKey;
typedef Bytes BTreeVal;

typedef struct {
  BTreeKey key;
  BTreeVal val;
} BTreeRecord;

typedef struct {
  const BTree *tree;
  void *node;
  u32 offset;
} BTreeIter;

BTreeStatus btree_init(BTree *);
void btree_deinit(BTree *);
BTreeStatus btree_find(const BTree *, BTreeKey key, BTreeVal *val);
BTreeStatus btree_insert(BTree *, BTreeRecord);
BTreeStatus btree_delete(BTree *, BTreeKey key);
BTreeStatus btree_iter(const BTree *, BTreeKey key, BTreeIter *iter);
BTreeStatus btree_next(BTreeIter *, BTreeRecord *);
void btree_debug_print(BTree *);
