#include <cmark-gfm.h>

#include "wd_tree.h"

cmark_node *wd_tree_next(cmark_node *root, cmark_node *curr) {
  cmark_node *node;

  node = NULL;

  if ((node = cmark_node_first_child(curr ? curr : root)) != NULL) {
    return node;
  }

  if ((node = cmark_node_next(curr)) != NULL) {
    return node;
  }

  node = curr;

  while (1) {
    node = cmark_node_parent(node);
    if (node == root || node == NULL) {
      return NULL;
    }

    if (cmark_node_next(node) == NULL) {
      continue;
    }

    return cmark_node_next(node);
  }
}

cmark_node *wd_tree_last(cmark_node *curr) {
  cmark_node *node, *n;

  node = curr;
  while ((n = cmark_node_last_child(node)) != NULL) {
    node = n;
  }

  return node;
}

cmark_node *wd_tree_prev(cmark_node *root, cmark_node *curr) {
  cmark_node *node;

  node = NULL;

  if (curr == NULL) {
    return wd_tree_last(root);
  }

  if ((node = cmark_node_previous(curr)) != NULL) {
    return wd_tree_last(node);
  }

  node = cmark_node_parent(curr);
  if (node == NULL || node == root) {
    return NULL;
  }

  return node;
}

cmark_node *wd_tree_find_next_thing(cmark_node *root, cmark_node *curr) {
  cmark_node *node;

  node = curr;

  while (1) {
    if ((node = wd_tree_next(root, node)) == NULL) {
      break;
    }

    if (cmark_node_get_type(node) == CMARK_NODE_LINK) {
      return node;
    }
  }

  return NULL;
}

cmark_node *wd_tree_find_prev_thing(cmark_node *root, cmark_node *curr) {
  cmark_node *node;

  node = curr;

  while (1) {
    if ((node = wd_tree_prev(root, node)) == NULL) {
      break;
    }

    if (cmark_node_get_type(node) == CMARK_NODE_LINK) {
      return node;
    }
  }

  return NULL;
}
