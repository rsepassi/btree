// TODO:
// * Value list
// * Deinit free nodes

#include "btree.h"

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define BTREE_FANOUT 4
#define BTREE_LEAF_N 3

typedef struct Node_s Node;

// n specifies the number of children
// the number of pivots is always n-1
typedef struct {
  u32 n;
  BTreeKey pivots[BTREE_FANOUT - 1];
  Node *children[BTREE_FANOUT];
} InteriorNode;

typedef struct {
  u32 n;
  BTreeRecord vals[BTREE_LEAF_N];
  Node *next;
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
  Node *next;
};

static Node *new_interior_node() {
  Node *node = calloc(1, sizeof(Node));
  node->type = Interior;
  return node;
}

static Node *new_data_node() {
  Node *node = calloc(1, sizeof(Node));
  node->type = Data;
  return node;
}

static inline u32 nkeys(InteriorNode *n) { return n->n - 1; }

// Our key comparator
// Returns:
//  0 = a = b
//  1 = a > b
// -1 = a < b
static int keycmp(Bytes a, Bytes b) {
  int rc = memcmp(a.buf, b.buf, MIN(a.len, b.len));
  if (rc != 0)
    return rc;
  if (a.len == b.len)
    return 0;
  if (a.len > b.len)
    return 1;
  return -1;
}

static void interior_insert(InteriorNode *n, Bytes key, Node *right) {
  // This function is only used when it has been established that there is
  // room for a new element
  CHECK(n->n < BTREE_FANOUT);

  // Find i, the index of the first key > key
  u32 i;
  for (i = 0; i < nkeys(n); ++i) {
    if (keycmp(key, n->pivots[i]) < 0)
      break;
  }

  // Make space for the key by shifting the remainder to the right
  usize nmove = nkeys(n) - i;
  if (nmove) {
    memmove(&n->pivots[i + 1], &n->pivots[i], nmove * sizeof(BTreeKey));
    memmove(&n->children[i + 2], &n->children[i + 1], nmove * sizeof(Node *));
  }

  // Insert at i
  n->pivots[i] = key;
  n->children[i + 1] = right;
  n->n++;
}

static void data_insert(DataNode *n, BTreeRecord rec) {
  // This function is only used when it has been established that there is
  // room for a new element
  CHECK(n->n < BTREE_LEAF_N);

  // Find i, the index of the first key > key
  u32 i;
  for (i = 0; i < n->n; ++i) {
    if (keycmp(rec.key, n->vals[i].key) < 0)
      break;
  }

  // Make space for the key by shifting the remainder to the right
  usize nmove = n->n - i;
  if (nmove)
    memmove(&n->vals[i + 1], &n->vals[i], nmove * sizeof(BTreeRecord));

  // Insert at i
  n->vals[i] = rec;
  n->n++;
}

static void print_data_node(DataNode *n) {
  fprintf(stderr, "[");
  for (u32 i = 0; i < n->n; ++i) {
    BTreeRecord rec = n->vals[i];
    fprintf(stderr, "<%.*s>", (int)rec.key.len, rec.key.buf);
  }
  fprintf(stderr, "]");
}

static void print_interior_node(InteriorNode *n) {
  fprintf(stderr, "[");
  for (u32 i = 0; i < nkeys(n); ++i) {
    if (i > 0)
      fprintf(stderr, ", ");
    fprintf(stderr, "%.*s", (int)n->pivots[i].len, n->pivots[i].buf);
  }
  fprintf(stderr, "]");
}

static void print_node(Node *n) {
  if (n->type == Data)
    print_data_node(&n->data);
  else
    print_interior_node(&n->interior);
}

typedef struct {
  Bytes key;
  Node *left;
  Node *right;
} NodePivot;

static void pivot_insert(BTree *tree, NodePivot pivot, Node *parent);

static NodePivot interior_split(BTree *tree, Node *interior, Node *parent) {
  // This function is only called when the interior node is full
  CHECK(interior->interior.n == BTREE_FANOUT);

  // Create the new interior node
  Node *left = interior;
  Node *right = new_interior_node();

  // Each node will have half the children
  STATIC_CHECK(BTREE_FANOUT % 2 == 0);
  left->interior.n = BTREE_FANOUT / 2;
  right->interior.n = BTREE_FANOUT / 2;

  // Copy over the pivots and children
  memcpy(right->interior.pivots, &left->interior.pivots[(BTREE_FANOUT / 2)],
         (BTREE_FANOUT / 2 - 1) * sizeof(BTreeKey));
  memcpy(right->interior.children, &left->interior.children[BTREE_FANOUT / 2],
         (BTREE_FANOUT / 2) * sizeof(Node *));

  // The new pivot in the parent is the middle pivot, which is now not in
  // left nor right.
  NodePivot pivot = {
      .key = left->interior.pivots[BTREE_FANOUT / 2 - 1],
      .left = left,
      .right = right,
  };

  // Insert the new pivot into the parent
  pivot_insert(tree, pivot, parent);

  return pivot;
}

static void pivot_insert(BTree *tree, NodePivot pivot, Node *parent) {
  if (parent == NULL) {
    // New root!
    tree->height++;
    tree->root = new_interior_node();
    parent = tree->root;

    parent->interior.pivots[0] = pivot.key;
    parent->interior.children[0] = pivot.left;
    parent->interior.children[1] = pivot.right;
    parent->interior.n = 2;
    return;
  }

  // Split the parent if it is full
  if (parent->interior.n == BTREE_FANOUT) {
    NodePivot split_pivot = interior_split(tree, parent, parent->next);
    parent = keycmp(pivot.key, split_pivot.key) < 0 ? split_pivot.left
                                                    : split_pivot.right;
  }
  interior_insert(&parent->interior, pivot.key, pivot.right);
}

static NodePivot data_split(BTree *tree, Node *data, Node *parent) {
  // Create the new data node
  Node *left = data;
  Node *right = new_data_node();

  // Split it in the middle
  u32 split_idx = BTREE_LEAF_N / 2;

  // Each node will have ~half the values
  left->data.n = split_idx;
  right->data.n = BTREE_LEAF_N - split_idx;

  // Copy over values into right
  memcpy(right->data.vals, &left->data.vals[split_idx],
         right->data.n * sizeof(BTreeRecord));

  // Fix up next pointers
  right->data.next = left->data.next;
  left->data.next = right;

  // The new split key is the lowest key in the right node
  NodePivot pivot = {
      .key = right->data.vals[0].key,
      .left = left,
      .right = right,
  };

  // Insert the new pivot into the parent
  pivot_insert(tree, pivot, parent);

  return pivot;
}

typedef struct {
  Node *head;
  Node *tail;
} Q;

static void qenq(Q *q, Node *n) {
  n->next = NULL;
  if (!q->head || !q->tail) {
    q->head = n;
    q->tail = n;
  } else {
    q->tail->next = n;
    q->tail = n;
  }
}

static Node *qdeq(Q *q) {
  if (q->head == NULL)
    return NULL;
  Node *n = q->head;
  q->head = n->next;
  return n;
}

typedef struct {
  Node *data;
  Node *parent;
} FindResult;

static FindResult find_data_page(const BTree *tree, Bytes key) {
  Node *cur = tree->root;

  // Initialize our parent linked list if the root is an Interior node
  Node *parent = cur->type == Interior ? cur : NULL;
  if (parent)
    parent->next = NULL;

  // Descend through the tree
  for (u32 h = 0; h < tree->height; ++h) { // while (cur->type == Interior)
    // Find i, the index of the first key > key
    u32 i;
    for (i = 0; i < nkeys(&cur->interior); ++i) {
      if (keycmp(key, cur->interior.pivots[i]) < 0)
        break;
    }

    // Descend
    cur = cur->interior.children[i];

    // Track parents
    if (cur->type == Interior) {
      cur->next = parent;
      parent = cur;
    }
  }

  Node *data = cur;
  DCHECK(data);
  DCHECK(data->type == Data);
  return (FindResult){data, parent};
}

// Public API
// ============================================================================

BTreeStatus btree_init(BTree *tree) {
  *tree = (BTree){0};
  tree->root = new_data_node();
  return 0;
}

void btree_deinit(BTree *tree) {
  if (tree->root)
    free(tree->root);
}

BTreeStatus btree_find(const BTree *tree, BTreeKey key, BTreeVal *val) {
  FindResult res = find_data_page(tree, key);
  Node *data = res.data;

  for (u32 i = 0; i < data->data.n; ++i) {
    if (keycmp(key, data->data.vals[i].key) == 0) {
      *val = data->data.vals[i].val;
      return 0;
    }
  }

  return BTree_NOT_FOUND;
}

BTreeStatus btree_iter(const BTree *tree, BTreeKey key, BTreeIter *iter) {
  FindResult res = find_data_page(tree, key);
  Node *data = res.data;

  // Find i, the index of the first key >= key
  u32 i;
  for (i = 0; i < data->data.n; ++i) {
    if (keycmp(key, data->data.vals[i].key) <= 0)
      break;
  }

  *iter = (BTreeIter){
      .tree = tree,
      .node = data,
      .offset = i,
  };
  return 0;
}

BTreeStatus btree_next(BTreeIter *it, BTreeRecord *rec) {
  Node *data = it->node;

  if (data == NULL || it->offset >= data->data.n)
    return BTree_END;

  *rec = data->data.vals[it->offset];

  // Advance the iterator
  it->offset++;
  if (it->offset >= data->data.n) {
    // Advance to the next page
    it->node = data->data.next;
    it->offset = 0;
  }

  return 0;
}

BTreeStatus btree_insert(BTree *tree, BTreeRecord rec) {
  // Find the data node we need to insert into
  FindResult res = find_data_page(tree, rec.key);
  Node *data = res.data;
  Node *parent = res.parent;

  // Split if needed
  if (data->data.n == BTREE_LEAF_N) {
    NodePivot pivot = data_split(tree, data, parent);
    data = (keycmp(rec.key, pivot.key) < 0) ? pivot.left : pivot.right;
  }

  // Insert
  data_insert(&data->data, rec);

  return 0;
}

void btree_debug_print(BTree *tree) {
  fprintf(stderr, "<BTree>\n");

  Q q = {0};
  qenq(&q, tree->root);

  Node *cur;
  while ((cur = qdeq(&q))) {
    print_node(cur);
    if (cur->type == Interior) {
      fprintf(stderr, "(%d)", cur->interior.n);
      for (u32 i = 0; i < cur->interior.n; ++i)
        qenq(&q, cur->interior.children[i]);
    }
  }
  fprintf(stderr, "\n</BTree>\n");
}

BTreeStatus btree_delete(BTree *tree, BTreeKey key) {
  CHECK(false, "unimplemented");
  return 1;
}
