#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <cmark.h>
#include <duktape.h>

#include "buf.h"
#include "wd_node.h"

#include "vm_node.h"

duk_ret_t wd_vm__Node__constructor(duk_context *ctx);
duk_ret_t wd_vm__Node__get__type(duk_context *ctx);
duk_ret_t wd_vm__Node__get__text(duk_context *ctx);
duk_ret_t wd_vm__Node__set__text(duk_context *ctx);
duk_ret_t wd_vm__Node__get__children(duk_context *ctx);
duk_ret_t wd_vm__Node__get__innerText(duk_context *ctx);
duk_ret_t wd_vm__Node__set__innerText(duk_context *ctx);
duk_ret_t wd_vm__Node__method__getNodesByType(duk_context *ctx);

void wd_vm__Node__define(duk_context *ctx) {
  duk_push_c_function(ctx, wd_vm__Node__constructor, 1);

  duk_push_object(ctx);

  duk_push_string(ctx, "type");
  duk_push_c_function(ctx, wd_vm__Node__get__type, 0);
  duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER);

  duk_push_string(ctx, "text");
  duk_push_c_function(ctx, wd_vm__Node__get__text, 0);
  duk_push_c_function(ctx, wd_vm__Node__set__text, 1);
  duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

  duk_push_string(ctx, "innerText");
  duk_push_c_function(ctx, wd_vm__Node__get__innerText, 0);
  duk_push_c_function(ctx, wd_vm__Node__set__innerText, 1);
  duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

  duk_push_string(ctx, "children");
  duk_push_c_function(ctx, wd_vm__Node__get__children, 0);
  duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER);

  duk_push_c_function(ctx, wd_vm__Node__method__getNodesByType, 1);
  duk_put_prop_string(ctx, -2, "getNodesByType");

  duk_put_prop_string(ctx, -2, "prototype");

  duk_put_global_string(ctx, "Node");
}

duk_ret_t wd_vm__Node__constructor(duk_context *ctx) {
  cmark_node *node;

  if (!duk_is_constructor_call(ctx)) {
    return DUK_RET_TYPE_ERROR;
  }

  node = duk_require_pointer(ctx, 0);

  duk_push_this(ctx);
  duk_push_pointer(ctx, node);
  duk_put_prop_string(ctx, -2, "_ptr");

  return 1;
}

duk_ret_t wd_vm__Node__get__type(duk_context *ctx) {
  cmark_node *node;

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  duk_push_string(ctx, cmark_node_get_type_string(node));

  return 1;
}

duk_ret_t wd_vm__Node__get__text(duk_context *ctx) {
  cmark_node *node;

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  duk_push_string(ctx, cmark_node_get_literal(node));

  return 1;
}

duk_ret_t wd_vm__Node__set__text(duk_context *ctx) {
  cmark_node *node;

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  cmark_node_set_literal(node, duk_require_string(ctx, 0));

  return 1;
}

duk_ret_t wd_vm__Node__get__children(duk_context *ctx) {
  int i;
  cmark_node *node, *child;

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  duk_push_array(ctx);

  i = 0;
  for (child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
    duk_get_global_string(ctx, "Node");
    duk_push_pointer(ctx, child);
    duk_new(ctx, 1);
    duk_put_prop_index(ctx, -2, i++);
  }

  return 1;
}

duk_ret_t wd_vm__Node__method__getNodesByType(duk_context *ctx) {
  cmark_node *node, *child;
  cmark_iter *iter;
  cmark_event_type ev;
  const char *type;
  int i;

  type = duk_require_string(ctx, 0);

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  duk_push_array(ctx);

  i = 0;
  iter = cmark_iter_new(node);

  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    if (ev != CMARK_EVENT_ENTER) {
      continue;
    }

    child = cmark_iter_get_node(iter);

    if (strcmp(type, cmark_node_get_type_string(child)) != 0) {
      continue;
    }

    duk_get_global_string(ctx, "Node");
    duk_push_pointer(ctx, child);
    duk_new(ctx, 1);
    duk_put_prop_index(ctx, -2, i++);
  }

  return 1;
}

duk_ret_t wd_vm__Node__get__innerText(duk_context *ctx) {
  cmark_node *node;
  cmark_iter *iter;
  cmark_event_type ev;
  const char *text;
  struct buf_s *buf;
  int i;

  buf = buf_new(0);

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  i = 0;
  iter = cmark_iter_new(node);

  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    if (ev != CMARK_EVENT_ENTER) {
      continue;
    }

    if ((text = cmark_node_get_literal(cmark_iter_get_node(iter))) == NULL) {
      continue;
    }

    if (i++ != 0 && strlen(text) > 0 && buf->len > 1) {
      buf_append(buf, " ", 1);
    }
    if (strlen(text) > 0) {
      buf_append(buf, text, strlen(text));
    }
  }

  duk_push_string(ctx, buf->buf);

  buf_free(buf);

  return 1;
}

duk_ret_t wd_vm__Node__set__innerText(duk_context *ctx) {
  cmark_node *node, *child;
  const char *text;

  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "_ptr");
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_pop_2(ctx);

  text = duk_require_string(ctx, 0);

  for (child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
    cmark_node_unlink(child);
    wd_node_free(child);
  }

  child = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(child, text);
  cmark_node_append_child(node, child);

  return 1;
}
