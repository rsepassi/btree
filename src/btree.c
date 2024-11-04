// TODO:
// * Interior split 
// * Value list
// * Deinit free nodes

#include "btree.h"

#include <stdlib.h>
#include <stdio.h>

#include "log.h"

typedef struct Node_s Node;

#define KEYMAX 2
typedef struct {
  u32 key_len;
  Bytes keys[KEYMAX];
  Node* ptrs[KEYMAX + 1];
} InteriorNode;

#define DATAMAX 4
typedef struct {
  u32 len;
  BTreeRecord vals[DATAMAX];
  Node* next;
} DataNode;

typedef enum {
  Interior,
  Data,
} NodeType;

struct Node_s {
  NodeType type;
  union {
    InteriorNode interior;
    DataNode data;
  };
  Node* next;
};

#define MIN(a, b) (a < b ? a : b)

static int keycmp(Bytes a, Bytes b) {
  int rc = memcmp(a.buf, b.buf, MIN(a.len, b.len));
  if (rc != 0) return rc;
  if (a.len == b.len) return 0;
  if (a.len > b.len) return 1;
  return -1;
}

static void interior_insert(InteriorNode* n, Bytes key, Node* right) {
  CHECK(n->key_len < KEYMAX);

  u32 i;
  for (i = 0; i < n->key_len; ++i) {
    if (keycmp(key, n->keys[i]) < 0) break;
  }

  for (u32 j = n->key_len; j > i; --j) {
    n->keys[j] = n->keys[j - 1];
    n->ptrs[j + 1] = n->ptrs[j];
  }

  n->keys[i] = key;
  n->ptrs[i + 1] = right;
  n->key_len++;
}

static void data_insert(DataNode* n, BTreeRecord rec) {
  CHECK(n->len < DATAMAX);

  u32 i;
  for (i = 0; i < n->len; ++i) {
    if (keycmp(rec.key, n->vals[i].key) < 0) break;
  }

  for (u32 j = n->len; j > i; --j) {
    n->vals[j] = n->vals[j - 1];
  }

  n->vals[i] = rec;
  n->len++;
}

static Node* new_interior_node() {
  Node* node = calloc(1, sizeof(Node));
  node->type = Interior;
  return node;
}

static Node* new_data_node() {
  Node* node = calloc(1, sizeof(Node));
  node->type = Data;
  return node;
}

typedef struct {
  Bytes key;
  Node* left;
  Node* right;
} NodeSplit;

static void split_insert(BTree* tree, NodeSplit split, Node* parents);

static NodeSplit interior_split(BTree* tree, Node* interior, Node* parents) {
  // Create the new interior node
  Node* left = interior;
  Node* right = new_interior_node();

  // Split it in the middle
  u32 split_idx = KEYMAX / 2;

  // Copy over values into right
  left->interior.key_len = split_idx;
  right->interior.key_len = KEYMAX - split_idx;
  memcpy(right->interior.keys, &left->interior.keys[split_idx], sizeof(right->interior.keys[0]) * right->interior.key_len);
  memcpy(right->interior.ptrs, &left->interior.ptrs[split_idx + 1], sizeof(right->interior.ptrs[0]) * (right->interior.key_len - 1));

  NodeSplit split = {
    .key = right->interior.keys[0],
    .left = left,
    .right = right,
  };
  LOG("isplit k=%.*s", (int)right->interior.keys[0].len, right->interior.keys[0].buf);

  // Insert the pointers into parents
  split_insert(tree, split, parents);

  return split;
}

static void split_insert(BTree* tree, NodeSplit split, Node* parents) {
  Node* parent = parents;
  if (parent == NULL) {
    LOG("new root s=%.*s", (int)split.key.len, split.key.buf);
    // New root!
    tree->height++;
    tree->root = new_interior_node();
    parent = tree->root;

    parent->interior.keys[0] = split.key;
    parent->interior.ptrs[0] = split.left;
    parent->interior.ptrs[1] = split.right;
    parent->interior.key_len = 1;
    return;
  } else {
    Node* inode = parent;
    if (inode->interior.key_len == KEYMAX) {
      NodeSplit isplit = interior_split(tree, parent, parent->next);
      inode = (keycmp(split.key, isplit.key) < 0) ? isplit.left : isplit.right;
    }
    interior_insert(&parent->interior, split.key, split.right);
  }
}

static NodeSplit data_split(BTree* tree, Node* data, Node* parents) {
  // Create the new data node
  Node* left = data;
  Node* right = new_data_node();

  // Split it in the middle
  u32 split_idx = DATAMAX / 2;

  // Copy over values into right
  left->data.len = split_idx;
  right->data.len = DATAMAX - split_idx;
  memcpy(right->data.vals, &left->data.vals[split_idx], sizeof(right->data.vals[0]) * right->data.len);

  // Fix up next pointers
  right->data.next = left->data.next;
  left->data.next = right;

  NodeSplit split = {
    .key = right->data.vals[0].key,
    .left = left,
    .right = right,
  };

  // Insert the pointers into parents
  split_insert(tree, split, parents);

  return split;
}

BTree_Status btree_init(BTree* tree) {
  *tree = (BTree){0};
  tree->root = new_data_node();
  return 0;
}

void btree_deinit(BTree* tree) {
  if (tree->root) free(tree->root);
}

BTree_Status btree_find(const BTree* tree, BTreeKey key, BTreeIter* it) {
  return 1;
}

BTree_Status btree_next(const BTree* tree, BTreeIter* it, BTreeRecord* rec) {
  return 1;
}

typedef struct {
  Node* data;
  Node* parents;
} FindResult;

static FindResult find_data_page(BTree* tree, Bytes key) {
  Node* cur = tree->root;

  Node* parents = cur->type == Interior ? cur : NULL;
  if (parents) parents->next = NULL;

  for (u32 h = 0; h < tree->height; ++h) {  // while (cur->type == Interior)
    u32 i;
    for (i = 0; i < cur->interior.key_len; ++i) {
      if (keycmp(key, cur->interior.keys[i]) < 0) break;
    }

    cur = cur->interior.ptrs[i];

    if (cur->type == Interior) {
      cur->next = parents;
      parents = cur;
    }
  }
  Node* data = cur;
  CHECK(data);
  CHECK(data->type == Data);
  return (FindResult){data, parents};
}

BTree_Status btree_insert(BTree* tree, BTreeRecord rec) {
  // Find the data node we need to insert into
  FindResult res = find_data_page(tree, rec.key);
  Node* data = res.data;
  Node* parents = res.parents;

  // Split if needed
  if (data->data.len == DATAMAX) {
    NodeSplit split = data_split(tree, data, parents);
    data = (keycmp(rec.key, split.key) < 0) ? split.left : split.right;
  }

  // Insert
  data_insert(&data->data, rec);

  return 0;
}

typedef struct {
  Node* head;
  Node* tail;
} Q;

static void qenq(Q* q, Node* n) {
  n->next = NULL;
  if (!q->head || !q->tail) {
    q->head = n;
    q->tail = n;
  } else {
    q->tail->next = n;
    q->tail = n;
  }
}

static Node* qdeq(Q* q) {
  if (q->head == NULL) return NULL;
  Node* n = q->head;
  q->head = n->next;
  return n;
}

static void print_data_node(DataNode* n) {
  printf("[");
  for (u32 i = 0; i < n->len; ++i) {
    BTreeRecord rec = n->vals[i];
    printf("<%.*s>",
        (int)rec.key.len, 
        rec.key.buf);
  }
  printf("]");
}

static void print_interior_node(InteriorNode* n) {
  printf("[");
  for (u32 i = 0; i < n->key_len; ++i) {
    if (i > 0) printf(", ");
    printf("%.*s", (int)n->keys[i].len, n->keys[i].buf);
  }
  printf("]");
}

static void print_node(Node* n) {
  if (n->type == Data) {
    print_data_node(&n->data);
  } else {
    print_interior_node(&n->interior);
  }
}

void btree_debug_print(BTree* tree) {
  if (!tree->root) {
    printf("<nil>\n");
    return;
  }

  printf("<BTree>\n");

  Q q = {0};
  qenq(&q, tree->root);

  while (q.head) {
    // Dequeue
    Node* cur = qdeq(&q);
    print_node(cur);

    if (cur->type == Interior) {
      // Enqueue children
      for (u32 i = 0; i < (cur->interior.key_len + 1); ++i) {
        Node* n = cur->interior.ptrs[i];
        if (n == NULL) continue;
        qenq(&q, n);
      }
    }
  }
  printf("\n</BTree>\n");
}
