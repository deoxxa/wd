#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <cmark.h>
#include <curl/curl.h>
#include <duktape.h>
#include <termbox.h>

#include "buf.h"
#include "draw.h"

#define WD_VERSION "1.0.0"
#define WS " \t\r\n"

// duktape.h defines these as well
// #define MIN(a, b) ((a) < (b) ? (a) : (b))
// #define MAX(a, b) ((a) > (b) ? (a) : (b))

enum wd_mode_e { WD_MODE_NORMAL, WD_MODE_URL };

enum wd_error_e { WD_ERROR_NONE, WD_ERROR_FAILED_PARSE, WD_ERROR_UNKNOWN };

struct wd_s {
  // input mode
  enum wd_mode_e im;

  // current page
  char current_url[10000];
  struct buf_s *current_src;
  cmark_node *current_doc;
  duk_context *current_vm;
  enum wd_error_e current_err;
  char current_message[1000];
  int current_scroll;

  // url input
  char edit_url[10000];
  int edit_url_cursor;

  // it is what it sounds like
  int loading;
  int quit;
};

struct render_s {
  int top;
  int x1, y1, x2, y2;
  int x, y;
  int start, space;
  int list;
  int strong, emph, code, link;
};

struct node_meta_s {
  int ready, x1, y1, x2, y2;
};

struct node_meta_s *node_meta_get(cmark_node *node);
void node_meta_clear_all(cmark_node *node);
void render_node_block(cmark_node *node, struct render_s *r);
void render_node_inline(cmark_node *node, struct render_s *r);
void render_node_inline_text(const char *word, struct render_s *r);
void wd_node_free(cmark_node *node);
void wd_open_url(struct wd_s *s, const char *url);
void wd_open_url_http(struct wd_s *s, const char *url);
void wd_open_url_file(struct wd_s *s, const char *url);
void wd_handle_ev(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_always(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_mode_normal(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_mode_url(struct wd_s *s, struct tb_event *ev);
void wd_exec(struct wd_s *s);
void wd_render(struct wd_s *s);
int main(int argc, char **argv);

struct node_meta_s *node_meta_get(cmark_node *node) {
  struct node_meta_s *meta;

  if ((meta = cmark_node_get_user_data(node)) == NULL) {
    meta = malloc(sizeof(struct node_meta_s));
    memset(meta, 0, sizeof(struct node_meta_s));
    cmark_node_set_user_data(node, meta);
  }

  return meta;
}

void node_meta_clear_all(cmark_node *node) {
  cmark_node *child;
  struct node_meta_s *meta;

  if ((meta = cmark_node_get_user_data(node)) != NULL) {
    free(meta);
    meta = NULL;
  }

  for (child = cmark_node_first_child(node); child != NULL;
       child = cmark_node_next(child)) {
    node_meta_clear_all(child);
  }
}

void render_node_block(cmark_node *node, struct render_s *r) {
  struct node_meta_s *meta;
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int len, b, i;
  const char *line;

  if ((meta = node_meta_get(node)) == NULL) {
    return;
  }

  if (meta->ready && meta->y1 > r->y2) {
    return;
  }

  switch (t = cmark_node_get_type(node)) {
    case CMARK_NODE_DOCUMENT: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      r->start = 1;
      r->space = 0;

      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_block(child, r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->x = 0;
      r->y += 1;

      break;
    }
    case CMARK_NODE_HEADING: {
      for (i = 0; i < cmark_node_get_heading_level(node); i++) {
        buf[i] = '#';
        buf[i + 1] = ' ';
        buf[i + 2] = 0;
      }

      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      r->start = 0;
      r->space = 0;

      b = r->strong;
      r->strong = 1;

      render_node_inline_text(buf, r);
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }

      r->strong = b;

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->x = 0;
      r->y += 1;
      if (!r->list) {
        r->y += 1;
      }

      break;
    }
    case CMARK_NODE_CODE_BLOCK: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      r->start = 1;
      r->space = 0;

      snprintf(buf, sizeof(buf), "```%s", cmark_node_get_fence_info(node));
      draw_text(0, r->y - r->y1, buf);
      r->y += 1;

      line = cmark_node_get_literal(node);
      while (*line) {
        len = strcspn(line, "\n");
        draw_textn(0, r->y - r->y1, line, len);
        r->y += 1;
        line += len + 1;
      }

      draw_text(0, r->y - r->y1, "```");

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->x = 0;
      r->y += 1;
      if (!r->list) {
        r->y += 1;
      }

      break;
    }
    case CMARK_NODE_PARAGRAPH: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      r->start = 1;
      r->space = 0;

      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->x = 0;
      r->y += 1;
      if (!r->list) {
        r->y += 1;
      }

      break;
    }
    case CMARK_NODE_LIST: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      b = r->list;

      r->start = 1;
      r->space = 0;

      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        r->list++;
        render_node_block(child, r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->list = b;

      r->x = 0;
      r->y += 1;

      break;
    }
    case CMARK_NODE_ITEM: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        if (cmark_node_get_list_type(cmark_node_parent(node)) ==
            CMARK_ORDERED_LIST) {
          snprintf(buf, sizeof(buf), "%d. ", r->list);
          render_node_inline_text(buf, r);
        } else {
          render_node_inline_text("* ", r);
        }

        render_node_block(child, r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    default: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      r->start = 1;
      r->space = 0;

      len = snprintf(buf, sizeof(buf), "unhandled block `%s' node",
                     cmark_node_get_type_string(node));
      draw_text(r->x, r->y - r->y1, buf);

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      r->x = 0;
      r->y += 1;
      if (!r->list) {
        r->y += 1;
      }

      break;
    }
  }
}

void render_node_inline(cmark_node *node, struct render_s *r) {
  struct node_meta_s *meta;
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int b;

  if ((meta = node_meta_get(node)) == NULL) {
    return;
  }

  if (meta->ready && meta->y1 > r->y2) {
    return;
  }

  switch (t = cmark_node_get_type(node)) {
    case CMARK_NODE_EMPH: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      b = r->emph;
      r->emph = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->emph = b;

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    case CMARK_NODE_STRONG: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      b = r->strong;
      r->strong = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->strong = b;

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    case CMARK_NODE_CODE: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      b = r->code;
      r->code = 1;
      render_node_inline_text(cmark_node_get_literal(node), r);
      r->code = b;

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    case CMARK_NODE_TEXT: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      render_node_inline_text(cmark_node_get_literal(node), r);

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    case CMARK_NODE_LINK: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      b = r->link;
      r->link = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->link = 0;

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    case CMARK_NODE_SOFTBREAK: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      if (r->start) {
        r->start = 0;
      } else {
        render_node_inline_text(" ", r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    default: {
      if (!meta->ready) {
        meta->x1 = r->x;
        meta->y1 = r->y;
      }

      snprintf(buf, sizeof(buf), "unhandled inline `%s' node",
               cmark_node_get_type_string(node));
      render_node_inline_text(buf, r);

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
  }
}

void render_node_inline_text(const char *word, struct render_s *r) {
  int len, fg, bg;

  fg = TB_DEFAULT;
  bg = TB_DEFAULT;

  if (r->link) {
    fg |= TB_BLUE;
    fg |= TB_UNDERLINE;
  } else if (r->emph) {
    fg |= TB_CYAN;
  }

  if (r->code) {
    fg |= TB_REVERSE;
  } else {
    if (r->strong) {
      fg |= TB_BOLD;
    }
  }

  while (*word) {
    if ((len = strspn(word, WS)) > 0) {
      if (r->start) {
        r->start = 0;
      } else {
        draw_text_special(r->x, r->y - r->y1, " ", fg, bg);
        r->x += 1;
      }

      word += len;
      continue;
    }

    r->start = 0;

    len = strcspn(word, WS);

    if (r->x + len >= tb_width() && !r->start) {
      r->y += 1;
      r->x = 0;
      r->start = 1;
      continue;
    }

    draw_text_specialn(r->x, r->y - r->y1, word, len, fg, bg);

    r->x += len;

    word += len;
  }
}

void wd_node_free(cmark_node *node) {
  if (node == NULL) {
    return;
  }

  node_meta_clear_all(node);
  cmark_node_free(node);
}

void wd_open_url(struct wd_s *s, const char *url) {
  if (strncmp(url, "http://", 7) == 0) {
    wd_open_url_http(s, url);
    wd_exec(s);
  } else if (strncmp(url, "file://", 7) == 0) {
    wd_open_url_file(s, url);
    wd_exec(s);
  } else {
    s->current_err = WD_ERROR_FAILED_PARSE;
  }
}

void wd_open_url_http(struct wd_s *s, const char *url) {
  CURL *ch;
  CURLcode res;
  cmark_node *doc;
  struct buf_s *buf;

  ch = NULL;
  doc = NULL;
  buf = buf_new(1024 * 100);

  if ((ch = curl_easy_init()) == NULL) {
    s->current_err = WD_ERROR_UNKNOWN;
    goto cleanup;
  }

  curl_easy_setopt(ch, CURLOPT_URL, url);
  curl_easy_setopt(ch, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, buf_write_cb);
  curl_easy_setopt(ch, CURLOPT_USERAGENT, "wd/" WD_VERSION);

  if ((res = curl_easy_perform(ch)) != CURLE_OK) {
    s->current_err = WD_ERROR_UNKNOWN;
    goto cleanup;
  }

  if ((doc = cmark_parse_document(buf->buf, buf->len - 1, CMARK_OPT_DEFAULT)) ==
      NULL) {
    s->current_err = WD_ERROR_FAILED_PARSE;
    goto cleanup;
  }

  strncpy(s->current_url, url, sizeof(s->current_url));

  free(s->current_src);
  s->current_src = buf;

  wd_node_free(s->current_doc);
  s->current_doc = doc;

  if (s->current_vm != NULL) {
    duk_destroy_heap(s->current_vm);
    s->current_vm = NULL;
  }

cleanup:
  if (ch != NULL) {
    curl_easy_cleanup(ch);
  }
  if (buf != NULL && buf != s->current_src) {
    buf_free(buf);
  }
  if (doc != NULL && doc != s->current_doc) {
    wd_node_free(doc);
  }
}

void wd_open_url_file(struct wd_s *s, const char *url) {
  FILE *fd;
  cmark_node *doc;
  struct buf_s *buf;
  char b[1024];
  size_t nread;

  doc = NULL;
  buf = buf_new(1024 * 100);

  if ((fd = fopen(&(url[7]), "r")) == NULL) {
    s->current_err = WD_ERROR_UNKNOWN;
    goto cleanup;
  }

  while (!feof(fd)) {
    if ((nread = fread(b, 1, sizeof(b), fd)) > 0) {
      buf_append(buf, b, nread);
    }
  }
  if (ferror(fd)) {
    s->current_err = WD_ERROR_UNKNOWN;
    goto cleanup;
  }

  if ((doc = cmark_parse_document(buf->buf, buf->len - 1, CMARK_OPT_DEFAULT)) ==
      NULL) {
    s->current_err = WD_ERROR_FAILED_PARSE;
    goto cleanup;
  }

  strncpy(s->current_url, url, sizeof(s->current_url));

  free(s->current_src);
  s->current_src = buf;

  wd_node_free(s->current_doc);
  s->current_doc = doc;

  if (s->current_vm != NULL) {
    duk_destroy_heap(s->current_vm);
    s->current_vm = NULL;
  }

cleanup:
  if (fd != NULL) {
    fclose(fd);
  }
  if (buf != NULL && buf != s->current_src) {
    buf_free(buf);
  }
  if (doc != NULL && doc != s->current_doc) {
    wd_node_free(doc);
  }
}

void wd_handle_ev(struct wd_s *s, struct tb_event *ev) {
  wd_handle_ev_always(s, ev);

  switch (s->im) {
    case WD_MODE_NORMAL:
      wd_handle_ev_mode_normal(s, ev);
      break;
    case WD_MODE_URL:
      wd_handle_ev_mode_url(s, ev);
      break;
  }
}

void wd_handle_ev_always(struct wd_s *s, struct tb_event *ev) {
  if (ev->type == TB_EVENT_RESIZE) {
    if (s->current_doc != NULL) {
      node_meta_clear_all(s->current_doc);
    }

    wd_render(s);
  }
}

void wd_handle_ev_mode_normal(struct wd_s *s, struct tb_event *ev) {
  switch (ev->key) {
    case TB_KEY_CTRL_C:
      if (s->loading) {
        s->loading = 0;
      }
      break;
    case TB_KEY_CTRL_M:
      memset(s->current_message, 0, sizeof(s->current_message));
      break;
    case TB_KEY_CTRL_O:
      strncpy(s->edit_url, s->current_url, sizeof(s->edit_url));
      s->edit_url_cursor = strlen(s->current_url);
      s->im = WD_MODE_URL;
      break;
    case TB_KEY_CTRL_R:
      wd_open_url(s, s->current_url);
      break;
    case TB_KEY_CTRL_X:
      s->quit = 1;
      break;
    case TB_KEY_ARROW_UP:
      if (s->current_scroll > 0) {
        s->current_scroll--;
      }
      break;
    case TB_KEY_ARROW_DOWN:
      if (s->current_scroll < node_meta_get(s->current_doc)->y2 - (tb_height() - 3)) {
        s->current_scroll++;
      }
      break;
    case TB_KEY_PGUP:
      s->current_scroll = MAX(s->current_scroll - (tb_height() - 3), 0);
      break;
    case TB_KEY_PGDN:
      s->current_scroll = MIN(s->current_scroll + (tb_height() - 3), node_meta_get(s->current_doc)->y2 - (tb_height() - 3));
      break;
  }
}

void wd_handle_ev_mode_url(struct wd_s *s, struct tb_event *ev) {
  switch (ev->key) {
    case TB_KEY_ENTER:
      wd_open_url(s, s->edit_url);
      s->im = WD_MODE_NORMAL;
      break;
    case TB_KEY_CTRL_X:
      s->im = WD_MODE_NORMAL;
      break;
    case TB_KEY_ARROW_LEFT:
      if (s->edit_url_cursor > 0) {
        s->edit_url_cursor--;
      }
      break;
    case TB_KEY_ARROW_RIGHT:
      if (s->edit_url_cursor < strlen(s->edit_url)) {
        s->edit_url_cursor++;
      }
      break;
    case TB_KEY_BACKSPACE:
    case TB_KEY_BACKSPACE2:
      if (s->edit_url_cursor > 0) {
        s->edit_url[--s->edit_url_cursor] = 0;
      }
      break;
    default:
      if (isprint(ev->ch) && s->edit_url_cursor < sizeof(s->edit_url) - 1) {
        s->edit_url[s->edit_url_cursor++] = ev->ch;
      }
      break;
  }
}

void wd_render(struct wd_s *s) {
  char buf[10050];
  struct render_s r;

  memset(&r, 0, sizeof(r));
  r.y = 1;
  r.x1 = 0;
  r.y1 = s->current_scroll;
  r.x2 = tb_width();
  r.y2 = s->current_scroll + tb_height() - 2;

  tb_clear();

  tb_select_output_mode(TB_OUTPUT_NORMAL);

  if (s->current_doc != NULL) {
    render_node_block(s->current_doc, &r);
  } else {
    draw_text(0, 1, "No document loaded - hit CTRL+O to open a URL");
  }

  if (s->im == WD_MODE_URL) {
    snprintf(buf, sizeof(buf), "URL: %s", s->edit_url);
    draw_line_special(0, 0, tb_width(), TB_BLACK, TB_WHITE);
    draw_text_special(0, 0, buf, TB_BLACK, TB_WHITE);
    tb_change_cell(5 + s->edit_url_cursor, 0, s->edit_url[s->edit_url_cursor],
                   TB_WHITE, TB_BLACK);
  } else if (s->current_src != NULL) {
    snprintf(buf, sizeof(buf), "URL: %s (%ld bytes; scroll=%d)", s->current_url,
             s->current_src->len, s->current_scroll);
    draw_line_special(0, 0, tb_width(), TB_WHITE | TB_BOLD, TB_BLUE);
    draw_text_special(0, 0, buf, TB_WHITE | TB_BOLD, TB_BLUE);
  } else {
    draw_line_special(0, 0, tb_width(), TB_WHITE | TB_BOLD, TB_BLUE);
    draw_text_special(0, 0, "URL:", TB_WHITE | TB_BOLD, TB_BLUE);
  }

  if (s->current_err != WD_ERROR_NONE) {
    snprintf(buf, sizeof(buf), "Error code 0x%02x", s->current_err);

    draw_line_special(0, tb_height() - 2, tb_width(), TB_WHITE | TB_BOLD,
                      TB_RED);
    draw_text_special(0, tb_height() - 2, buf, TB_WHITE | TB_BOLD, TB_RED);
  } else if (strlen(s->current_message) > 0) {
    snprintf(buf, sizeof(buf), "Message: %s", s->current_message);

    draw_line_special(0, tb_height() - 2, tb_width(), TB_BLACK, TB_YELLOW);
    draw_text_special(0, tb_height() - 2, buf, TB_BLACK, TB_YELLOW);
  }

  draw_line_special(0, tb_height() - 1, tb_width(), TB_WHITE | TB_BOLD,
                    TB_BLUE);
  draw_text_special(0, tb_height() - 1,
                    "^O=Open ^X=Exit ^R=Refresh ^M=Close Message UP/DOWN/PGUP/PGDN=Scroll",
                    TB_WHITE | TB_BOLD, TB_BLUE);

  tb_present();
}

duk_ret_t wd_vm_set_message(duk_context *ctx) {
  struct wd_s *s;
  const char *message;

  message = duk_get_string_default(ctx, -1, "");

  duk_get_global_string(ctx, "window");
  s = duk_get_pointer(ctx, -1);
  duk_pop(ctx);

  strncpy(s->current_message, message, sizeof(s->current_message));

  return DUK_ERR_NONE;
}

duk_ret_t wd_vm_get_node_type(duk_context *ctx) {
  cmark_node *node;
  node = duk_get_pointer_default(ctx, -1, NULL);
  duk_push_string(ctx, cmark_node_get_type_string(node));
  return 1;
}

void wd_exec(struct wd_s *s) {
  cmark_event_type ev;
  cmark_iter *iter;
  cmark_node *node;
  const char *info, *code;
  duk_context *vm;

  if (s->current_doc == NULL) {
    goto exit;
  }

  vm = duk_create_heap_default();

  duk_push_pointer(vm, s);
  duk_put_global_string(vm, "window");

  duk_push_pointer(vm, s->current_doc);
  duk_put_global_string(vm, "document");

  duk_push_c_function(vm, wd_vm_set_message, 1);
  duk_put_global_string(vm, "set_message");

  duk_push_c_function(vm, wd_vm_get_node_type, 1);
  duk_put_global_string(vm, "get_node_type");

  iter = cmark_iter_new(s->current_doc);

  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    node = cmark_iter_get_node(iter);

    if (ev != CMARK_EVENT_ENTER) {
      continue;
    }

    if (cmark_node_get_type(node) != CMARK_NODE_CODE_BLOCK) {
      continue;
    }

    info = cmark_node_get_fence_info(node);
    code = cmark_node_get_literal(node);

    if (strcmp(info, "javascript") != 0 || strncmp(code, "/*@wd@*/", 8) != 0) {
      continue;
    }

    duk_peval_string(vm, code);
  }

  s->current_vm = vm;

exit:
  if (vm != NULL && vm != s->current_vm) {
    duk_destroy_heap(vm);
  }
}

int main(int argc, char **argv) {
  struct wd_s s;
  struct tb_event ev;

  memset(&s, 0, sizeof(struct wd_s));

  curl_global_init(CURL_GLOBAL_DEFAULT);

  if (tb_init() != 0) {
    perror("tb_init");

    return 1;
  }

  if (argc > 1) {
    wd_open_url(&s, argv[1]);
  }

  while (!s.quit) {
    wd_render(&s);
    tb_poll_event(&ev);
    wd_handle_ev(&s, &ev);
  }

  tb_shutdown();

  return 0;
}
