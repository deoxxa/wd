#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <cmark.h>
#include <curl/curl.h>
#include <duktape.h>
#include <termbox.h>

#include "buf.h"

#define WD_VERSION "1.0.0"
#define WS " \t\r\n"

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

  // url input
  char edit_url[10000];
  int edit_url_cursor;

  // it is what it sounds like
  int loading;
  int quit;
};

struct renderer_s {
  int x, y;
  int start, space;
  int list;
  int strong, emph, code, link;
};

void draw_line_special(int x, int y, int w, uint16_t fg, uint16_t bg);
void draw_text_special(int x, int y, const char *str, uint16_t fg, uint16_t bg);
void draw_text_specialn(int x, int y, const char *str, size_t len, uint16_t fg,
                        uint16_t bg);
void draw_text(int x, int y, const char *str);
void draw_textn(int x, int y, const char *str, size_t len);
void render_url_input(const char *url, int p);
void render_node_block(cmark_node *node, struct renderer_s *r);
void render_node_inline(cmark_node *node, struct renderer_s *r);
void render_node_inline_text(const char *word, struct renderer_s *r);
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

void draw_line_special(int x, int y, int w, uint16_t fg, uint16_t bg) {
  int i;

  for (i = 0; i < w; i++) {
    tb_change_cell(x + i, y, ' ', fg, bg);
  }
}

void draw_text_specialn(int x, int y, const char *str, size_t len, uint16_t fg,
                        uint16_t bg) {
  char c;
  int i, w;

  for (i = 0, w = 0; i < len; i++) {
    c = str[i];

    if (c == '\n') {
      y++;
      w = 0;
    } else {
      tb_change_cell(x + w++, y, c, fg, bg);
    }
  }
}

void draw_text_special(int x, int y, const char *str, uint16_t fg,
                       uint16_t bg) {
  draw_text_specialn(x, y, str, strlen(str), fg, bg);
}

void draw_textn(int x, int y, const char *str, size_t len) {
  draw_text_specialn(x, y, str, len, TB_DEFAULT, TB_DEFAULT);
}

void draw_text(int x, int y, const char *str) {
  draw_textn(x, y, str, strlen(str));
}

void render_node_block(cmark_node *node, struct renderer_s *r) {
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int len, b, i;
  const char *line;

  switch (t = cmark_node_get_type(node)) {
    case CMARK_NODE_DOCUMENT: {
      r->start = 1;
      r->space = 0;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_block(child, r);
      }
      r->y += 1;
      break;
    }
    case CMARK_NODE_HEADING: {
      for (i = 0; i < cmark_node_get_heading_level(node); i++) {
        buf[i] = '#';
        buf[i + 1] = ' ';
        buf[i + 2] = 0;
      }

      r->start = 0;
      r->space = 0;
      b = r->strong;
      render_node_inline_text(buf, r);
      r->strong = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->strong = b;
      if (!r->list) {
        r->y += 1;
      }
      r->y += 1;
      r->x = 0;
      break;
    }
    case CMARK_NODE_CODE_BLOCK: {
      r->start = 1;
      r->space = 0;
      snprintf(buf, sizeof(buf), "```%s", cmark_node_get_fence_info(node));
      draw_text(0, r->y, buf);
      r->y += 1;
      line = cmark_node_get_literal(node);
      while (*line) {
        len = strcspn(line, "\n");
        draw_textn(0, r->y, line, len);
        r->y += 1;
        line += len + 1;
      }
      draw_text(0, r->y, "```");
      if (!r->list) {
        r->y += 1;
      }
      r->y += 1;
      r->x = 0;
      break;
    }
    case CMARK_NODE_PARAGRAPH: {
      r->start = 1;
      r->space = 0;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      if (!r->list) {
        r->y += 1;
      }
      r->y += 1;
      r->x = 0;
      break;
    }
    case CMARK_NODE_LIST: {
      b = r->list;
      r->start = 1;
      r->space = 0;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        r->list++;
        render_node_block(child, r);
      }
      r->list = b;
      r->x = 0;
      r->y += 1;
      break;
    }
    case CMARK_NODE_ITEM: {
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
      break;
    }
    default: {
      r->start = 1;
      r->space = 0;
      len = snprintf(buf, sizeof(buf), "unhandled block `%s' node",
                     cmark_node_get_type_string(node));
      draw_text(r->x, r->y, buf);
      r->y += 2;
      r->x = 0;
      break;
    }
  }
}

void render_node_inline(cmark_node *node, struct renderer_s *r) {
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int len, b;

  switch (t = cmark_node_get_type(node)) {
    case CMARK_NODE_EMPH: {
      b = r->emph;
      r->emph = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->emph = b;

      break;
    }
    case CMARK_NODE_STRONG: {
      b = r->strong;
      r->strong = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->strong = b;

      break;
    }
    case CMARK_NODE_CODE: {
      b = r->code;
      r->code = 1;
      render_node_inline_text(cmark_node_get_literal(node), r);
      r->code = b;

      break;
    }
    case CMARK_NODE_TEXT: {
      render_node_inline_text(cmark_node_get_literal(node), r);

      break;
    }
    case CMARK_NODE_LINK: {
      b = r->link;
      r->link = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(child, r);
      }
      r->link = 0;

      break;
    }
    case CMARK_NODE_SOFTBREAK: {
      if (r->start) {
        r->start = 0;
      } else {
        render_node_inline_text(" ", r);
      }
      break;
    }
    default: {
      len = snprintf(buf, sizeof(buf), "unhandled inline `%s' node",
                     cmark_node_get_type_string(node));
      draw_text(r->x, r->y, buf);
      r->x += len;
      break;
    }
  }
}

void render_node_inline_text(const char *word, struct renderer_s *r) {
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
        draw_text_special(r->x, r->y, " ", fg, bg);
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

    draw_text_specialn(r->x, r->y, word, len, fg, bg);

    r->x += len;

    word += len;
  }
}

void wd_node_free(cmark_node *node) {
  if (node == NULL) {
    return;
  }

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
  struct renderer_s r;

  memset(&r, 0, sizeof(r));
  r.y = 1;

  tb_clear();

  tb_select_output_mode(TB_OUTPUT_NORMAL);

  if (s->im == WD_MODE_URL) {
    snprintf(buf, sizeof(buf), "URL: %s", s->edit_url);
    draw_line_special(0, 0, tb_width(), TB_BLACK, TB_WHITE);
    draw_text_special(0, 0, buf, TB_BLACK, TB_WHITE);
    tb_change_cell(5 + s->edit_url_cursor, 0, s->edit_url[s->edit_url_cursor],
                   TB_WHITE, TB_BLACK);
  } else if (s->current_src != NULL) {
    snprintf(buf, sizeof(buf), "URL: %s (%ld bytes)", s->current_url,
             s->current_src->len);
    draw_line_special(0, 0, tb_width(), TB_WHITE | TB_BOLD, TB_BLUE);
    draw_text_special(0, 0, buf, TB_WHITE | TB_BOLD, TB_BLUE);
  } else {
    draw_line_special(0, 0, tb_width(), TB_WHITE | TB_BOLD, TB_BLUE);
    draw_text_special(0, 0, "URL:", TB_WHITE | TB_BOLD, TB_BLUE);
  }

  if (s->current_doc != NULL) {
    render_node_block(s->current_doc, &r);
  } else {
    draw_text(0, 1, "No document loaded - hit CTRL+O to open a URL");
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
                    "^O=Open ^X=Exit ^R=Refresh ^M=Close Message",
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

duk_ret_t wd_vm_random(duk_context *ctx) { return 0; }

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
