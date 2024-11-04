// An in-memory B-Tree

#include "stdtypes.h"

#define BTree_OK 0
typedef int BTree_Status;

typedef struct {
  void* root;
  u32 height;
} BTree;

typedef Bytes BTreeKey;
typedef Bytes BTreeVal;

typedef struct {
  BTreeKey key;
  BTreeVal val;
} BTreeRecord;

typedef struct {
} BTreeIter;

BTree_Status btree_init(BTree*);
void btree_deinit(BTree*);

BTree_Status btree_find(const BTree*, BTreeKey key, BTreeIter*);
BTree_Status btree_next(const BTree*, BTreeIter*, BTreeRecord*);
BTree_Status btree_insert(BTree*, BTreeRecord);
BTree_Status btree_delete(BTree*, BTreeKey key);
void btree_debug_print(BTree*);
