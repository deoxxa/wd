struct wd_node_meta_s {
  int ready, x1, y1, x2, y2;
};

struct wd_node_meta_s *wd_node_meta_get(cmark_node *node);
void wd_node_meta_clear_all(cmark_node *node);
