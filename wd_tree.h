// #include <cmark-gfm.h>

cmark_node *wd_tree_next(cmark_node *root, cmark_node *curr);
cmark_node *wd_tree_last(cmark_node *curr);
cmark_node *wd_tree_prev(cmark_node *root, cmark_node *curr);
cmark_node *wd_tree_find_next_thing(cmark_node *root, cmark_node *curr);
cmark_node *wd_tree_find_prev_thing(cmark_node *root, cmark_node *curr);
