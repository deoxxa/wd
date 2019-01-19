#include <cmark-gfm.h>

#include "wd_node.h"
#include "wd_node_meta.h"

void wd_node_free(cmark_node *node) {
  if (node == NULL) {
    return;
  }

  wd_node_meta_clear_all(node);
  cmark_node_free(node);
}
