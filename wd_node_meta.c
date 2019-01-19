#include <stdlib.h>
#include <string.h>

#include <cmark-gfm.h>

#include "wd_node_meta.h"

struct wd_node_meta_s *wd_node_meta_get(cmark_node *node) {
  struct wd_node_meta_s *meta;

  if ((meta = cmark_node_get_user_data(node)) == NULL) {
    meta = malloc(sizeof(struct wd_node_meta_s));
    memset(meta, 0, sizeof(struct wd_node_meta_s));
    cmark_node_set_user_data(node, meta);
  }

  return meta;
}

void wd_node_meta_clear_all(cmark_node *node) {
  cmark_node *child;
  struct wd_node_meta_s *meta;

  if ((meta = cmark_node_get_user_data(node)) != NULL) {
    free(meta);
    meta = NULL;
    cmark_node_set_user_data(node, NULL);
  }

  for (child = cmark_node_first_child(node); child != NULL;
       child = cmark_node_next(child)) {
    wd_node_meta_clear_all(child);
  }
}
