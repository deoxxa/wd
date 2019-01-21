#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <curl/curl.h>
#include <duktape.h>
#include <termbox.h>
#include <uriparser/Uri.h>

#include "buf.h"
#include "draw.h"
#include "vm_node.h"
#include "wd_node.h"
#include "wd_node_meta.h"
#include "wd_tree.h"

#define WD_VERSION "1.0.0"
#define WS " \t\r\n"

extern cmark_node_type CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW,
    CMARK_NODE_TABLE_CELL;

/* duktape.h defines these as well */
/*
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
*/

enum wd_mode_e { WD_MODE_NORMAL, WD_MODE_URL };

enum wd_error_e {
  WD_ERROR_NONE,
  WD_ERROR_FAILED_PARSE,
  WD_ERROR_SCRIPT_EXCEPTION,
  WD_ERROR_UNKNOWN
};

struct wd_s {
  /* input mode */
  enum wd_mode_e im;

  /* current page */
  char current_url[10000];
  struct buf_s *current_src;
  cmark_node *current_doc;
  duk_context *current_vm;
  enum wd_error_e current_error;
  char current_error_message[1000];
  char current_message[1000];
  int current_scroll;
  cmark_node *current_node;

  /* url input */
  char edit_url[10000];
  unsigned int edit_url_cursor;

  /* it is what it sounds like */
  int loading;
  int quit;
};

struct render_s {
  int top;
  int x1, y1, x2, y2;
  int x, y;
  int start, space;
  int list;
  int strong, emph, code, link, active;
};

void render_node_block(struct wd_s *s, cmark_node *node, struct render_s *r);
void render_node_inline(struct wd_s *s, cmark_node *node, struct render_s *r);
void render_node_inline_text(const char *word, struct render_s *r);
int wd_open(struct wd_s *s, const char *url, struct buf_s *buf);
int wd_open_url(struct wd_s *s, const char *url);
int wd_open_url_http(struct wd_s *s, const char *url);
int wd_open_url_file(struct wd_s *s, const char *url);
void wd_handle_ev(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_always(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_mode_normal(struct wd_s *s, struct tb_event *ev);
void wd_handle_ev_mode_url(struct wd_s *s, struct tb_event *ev);
void wd_exec(struct wd_s *s);
void wd_render(struct wd_s *s);
int main(int argc, char **argv);

void render_node_block(struct wd_s *s, cmark_node *node, struct render_s *r) {
  struct wd_node_meta_s *meta;
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int len, b, i;
  const char *line;

  if ((meta = wd_node_meta_get(node)) == NULL) {
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
        render_node_block(s, child, r);
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
        render_node_inline(s, child, r);
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
        render_node_inline(s, child, r);
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
        render_node_block(s, child, r);
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

        render_node_block(s, child, r);
      }

      if (!meta->ready) {
        meta->x2 = r->x;
        meta->y2 = r->y;
        meta->ready = 1;
      }

      break;
    }
    default: {
      if (t == CMARK_NODE_TABLE) {
        if (!meta->ready) {
          meta->x1 = r->x;
          meta->y1 = r->y;
        }

        for (child = cmark_node_first_child(node); child != NULL;
             child = cmark_node_next(child)) {
          if (cmark_gfm_extensions_get_table_row_is_header(child)) {
            snprintf(buf, sizeof(buf), "this is a table with %d columns",
                     cmark_gfm_extensions_get_table_columns(node));
            draw_text(r->x, r->y - r->y1, buf);

            r->x = 0;
            r->y += 1;

            render_node_inline_text("---", r);

            r->x = 0;
            r->y += 1;
          } else {
            render_node_inline_text("row", r);

            r->x = 0;
            r->y += 1;
          }
        }

        if (!meta->ready) {
          meta->x2 = r->x;
          meta->y2 = r->y;
          meta->ready = 1;
        }

        if (!r->list) {
          r->y += 1;
        }
      } else {
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
      }

      break;
    }
  }
}

void render_node_inline(struct wd_s *s, cmark_node *node, struct render_s *r) {
  struct wd_node_meta_s *meta;
  cmark_node_type t;
  cmark_node *child;
  char buf[100];
  int b1, b2;

  if ((meta = wd_node_meta_get(node)) == NULL) {
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

      b1 = r->emph;
      r->emph = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(s, child, r);
      }
      r->emph = b1;

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

      b1 = r->strong;
      r->strong = 1;
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(s, child, r);
      }
      r->strong = b1;

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

      b1 = r->code;
      r->code = 1;
      render_node_inline_text(cmark_node_get_literal(node), r);
      r->code = b1;

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

      b1 = r->link;
      b2 = r->active;
      r->link = 1;
      if (node == s->current_node) {
        r->active = 1;
      }
      for (child = cmark_node_first_child(node); child != NULL;
           child = cmark_node_next(child)) {
        render_node_inline(s, child, r);
      }
      r->link = b1;
      r->active = b2;

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
    if (r->active) {
      fg |= TB_RED;
      fg |= TB_UNDERLINE;
    } else {
      fg |= TB_BLUE;
      fg |= TB_UNDERLINE;
    }
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

int wd_open(struct wd_s *s, const char *url, struct buf_s *buf) {
  int rc;
  cmark_parser *parser;
  cmark_node *doc;

  rc = 0;
  parser = NULL;
  doc = NULL;

  if ((parser = cmark_parser_new(CMARK_OPT_DEFAULT)) == NULL) {
    s->current_error = WD_ERROR_FAILED_PARSE;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "couldn't construct parser");
    rc = 1;
    goto cleanup;
  }
  cmark_parser_attach_syntax_extension(parser,
                                       cmark_find_syntax_extension("table"));
  cmark_parser_feed(parser, buf->buf, buf->len - 1);

  if ((doc = cmark_parser_finish(parser)) == NULL) {
    s->current_error = WD_ERROR_FAILED_PARSE;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "failed parsing document");
    rc = 1;
    goto cleanup;
  }
  cmark_consolidate_text_nodes(doc);

  strncpy(s->current_url, url, sizeof(s->current_url));

  if (s->current_src != NULL) {
    buf_free(s->current_src);
    s->current_src = NULL;
  }
  s->current_src = buf;

  if (s->current_doc != NULL) {
    wd_node_free(s->current_doc);
    s->current_doc = NULL;
  }
  s->current_doc = doc;

  if (s->current_vm != NULL) {
    duk_destroy_heap(s->current_vm);
    s->current_vm = NULL;
  }

cleanup:
  if (buf != NULL && buf != s->current_src) {
    buf_free(buf);
    buf = NULL;
  }
  if (parser != NULL) {
    cmark_parser_free(parser);
    parser = NULL;
  }
  if (doc != NULL && doc != s->current_doc) {
    wd_node_free(doc);
    doc = NULL;
  }

  if (rc == 0) {
    s->current_scroll = 0;
    s->current_node = 0;

    wd_exec(s);
  }

  return rc;
}

int wd_open_url(struct wd_s *s, const char *url) {
  int rc;
  UriUriA uri_frag, uri_base, uri_abs;
  UriParserStateA uri_state;
  char target[10000], scheme[100];

  rc = 0;
  memset(&uri_frag, 0, sizeof(uri_frag));
  memset(&uri_base, 0, sizeof(uri_base));
  memset(&uri_abs, 0, sizeof(uri_abs));
  memset(&uri_state, 0, sizeof(uri_state));
  memset(target, 0, sizeof(target));
  memset(scheme, 0, sizeof(scheme));

  if (strlen(s->current_url) > 0) {
    uri_state.uri = &uri_base;
    if (uriParseUriA(&uri_state, s->current_url) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't parse base URL");
      rc = 1;
      goto cleanup;
    }

    uri_state.uri = &uri_frag;
    if (uriParseUriA(&uri_state, url) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't parse target URL fragment");
      rc = 1;
      goto cleanup;
    }

    if (uriAddBaseUriA(&uri_abs, &uri_frag, &uri_base) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't resolve absolute target");
      rc = 1;
      goto cleanup;
    }

    if (uriToStringA(target, &uri_abs, sizeof(target) - 1, NULL) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't format target url");
      rc = 1;
      goto cleanup;
    }
  } else {
    uri_state.uri = &uri_abs;
    if (uriParseUriA(&uri_state, url) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't parse target URL fragment");
      rc = 1;
      goto cleanup;
    }

    if (uriToStringA(target, &uri_abs, sizeof(target) - 1, NULL) != URI_SUCCESS) {
      s->current_error = WD_ERROR_FAILED_PARSE;
      snprintf(s->current_error_message, sizeof(s->current_error_message), "couldn't format target url");
      rc = 1;
      goto cleanup;
    }
  }

  if (uri_abs.scheme.first != NULL) {
    memcpy(scheme, uri_abs.scheme.first, MIN(uri_abs.scheme.afterLast - uri_abs.scheme.first, sizeof(scheme) - 1));
  }

  if (strncmp(scheme, "http", 4) == 0) {
    if ((rc = wd_open_url_http(s, target)) != 0) {
      goto cleanup;
    }
  } else if (strncmp(scheme, "https", 5) == 0) {
    if ((rc = wd_open_url_http(s, target)) != 0) {
      goto cleanup;
    }
  } else if (strncmp(scheme, "file", 4) == 0) {
    if ((rc = wd_open_url_file(s, target)) != 0) {
      goto cleanup;
    }
  } else {
    s->current_error = WD_ERROR_FAILED_PARSE;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "unrecognised url scheme `%s' for %s; try https://, http://, or file://", scheme, target);

    rc = 1;
    goto cleanup;
  }

cleanup:
  uriFreeUriMembersA(&uri_frag);
  uriFreeUriMembersA(&uri_base);
  uriFreeUriMembersA(&uri_abs);
  return rc;
}

int wd_open_url_http(struct wd_s *s, const char *url) {
  int rc;
  CURL *ch;
  CURLcode res;
  struct buf_s *buf;

  rc = 0;
  ch = NULL;
  buf = buf_new(1024 * 100);

  if ((ch = curl_easy_init()) == NULL) {
    s->current_error = WD_ERROR_UNKNOWN;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "failed initialising curl");
    goto cleanup;
  }

  curl_easy_setopt(ch, CURLOPT_URL, url);
  curl_easy_setopt(ch, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, buf_write_cb);
  curl_easy_setopt(ch, CURLOPT_USERAGENT, "wd/" WD_VERSION);

  if ((res = curl_easy_perform(ch)) != CURLE_OK) {
    s->current_error = WD_ERROR_UNKNOWN;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "failed performing request for %s", url);
    rc = 1;
    goto cleanup;
  }

  rc = wd_open(s, url, buf);

cleanup:
  if (ch != NULL) {
    curl_easy_cleanup(ch);
  }

  return rc;
}

int wd_open_url_file(struct wd_s *s, const char *url) {
  int rc;
  FILE *fd;
  struct buf_s *buf;
  char b[1024], file[10050];
  size_t nread;

  rc = 0;
  fd = NULL;
  buf = buf_new(1024 * 100);
  memset(file, 0, sizeof(file));

  if (uriUriStringToUnixFilenameA(url, file) != URI_SUCCESS) {
    s->current_error = WD_ERROR_UNKNOWN;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "couldn't get filename from file url: %s", url);
    rc = 1;
    goto cleanup;
  }

  if ((fd = fopen(file, "r")) == NULL) {
    s->current_error = WD_ERROR_UNKNOWN;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "failed opening file: %s", file);
    rc = 1;
    goto cleanup;
  }

  while (!feof(fd)) {
    if ((nread = fread(b, 1, sizeof(b), fd)) > 0) {
      buf_append(buf, b, nread);
    }
  }
  if (ferror(fd)) {
    s->current_error = WD_ERROR_UNKNOWN;
    snprintf(s->current_error_message, sizeof(s->current_error_message),
             "failed reading file: %s", file);
    rc = 1;
    goto cleanup;
  }

  rc = wd_open(s, url, buf);

cleanup:
  if (fd != NULL) {
    fclose(fd);
    fd = NULL;
  }

  return rc;
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
      wd_node_meta_clear_all(s->current_doc);
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
    case TB_KEY_ESC:
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
    case TB_KEY_HOME:
      s->current_scroll = 0;
      break;
    case TB_KEY_END:
      s->current_scroll =
          wd_node_meta_get(s->current_doc)->y2 - (tb_height() - 2);
      break;
    case TB_KEY_ARROW_UP:
      s->current_scroll = MAX(s->current_scroll - 1, 0);
      break;
    case TB_KEY_ARROW_DOWN:
      s->current_scroll =
          MIN(s->current_scroll + 1,
              wd_node_meta_get(s->current_doc)->y2 - (tb_height() - 2));
      break;
    case TB_KEY_PGUP:
      s->current_scroll = MAX(s->current_scroll - (tb_height() - 2), 0);
      break;
    case TB_KEY_SPACE:
    case TB_KEY_PGDN:
      s->current_scroll =
          MIN(s->current_scroll + (tb_height() - 2),
              wd_node_meta_get(s->current_doc)->y2 - (tb_height() - 2));
      break;
    case TB_KEY_ENTER:
      if (s->current_node != NULL && cmark_node_get_type(s->current_node) == CMARK_NODE_LINK) {
        wd_open_url(s, cmark_node_get_url(s->current_node));
      }
      break;
  }

  switch (ev->ch) {
    case '[':
      s->current_node = wd_tree_find_prev_thing(s->current_doc, s->current_node);
      break;
    case ']':
      s->current_node = wd_tree_find_next_thing(s->current_doc, s->current_node);
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
  int p;
  char buf[10050];
  struct render_s r;
  struct wd_node_meta_s *m;

  p = 0;
  memset(buf, 0, sizeof(buf));
  memset(&r, 0, sizeof(r));
  m = NULL;

  r.y = 1;
  r.x1 = 0;
  r.y1 = s->current_scroll;
  r.x2 = tb_width();
  r.y2 = s->current_scroll + tb_height() - 2;

  tb_clear();

  tb_select_output_mode(TB_OUTPUT_NORMAL);

  if (s->current_doc != NULL) {
    render_node_block(s, s->current_doc, &r);
  } else {
    draw_text(0, 1, "No document loaded - hit CTRL+O to open a URL");
  }

  if (s->im == WD_MODE_URL) {
    snprintf(buf, sizeof(buf), "URL: %s", s->edit_url);
    draw_line_special(0, 0, tb_width(), TB_BLACK, TB_WHITE);
    draw_text_special(0, 0, buf, TB_BLACK, TB_WHITE);
    tb_change_cell(5 + s->edit_url_cursor, 0, s->edit_url[s->edit_url_cursor],
                   TB_WHITE, TB_BLACK);
  } else {
    p = 0;

    p += snprintf(&(buf[p]), sizeof(buf) - p, "URL:");

    if (s->current_url != NULL) {
      p += snprintf(&(buf[p]), sizeof(buf) - p, " %s", s->current_url);
    } else {
      p += snprintf(&(buf[p]), sizeof(buf) - p, " (none)");
    }

    if (s->current_src != NULL) {
      p += snprintf(&(buf[p]), sizeof(buf) - p, " | %ld bytes", s->current_src->len);
    }

    p += snprintf(&(buf[p]), sizeof(buf) - p, " | scroll=%d", s->current_scroll);

    if (s->current_node != NULL) {
      p += snprintf(&(buf[p]), sizeof(buf) - p, " | selected=%s", cmark_node_get_type_string(s->current_node));

      if ((m = wd_node_meta_get(s->current_node)) != NULL) {
        p += snprintf(&(buf[p]), sizeof(buf) - p, " (%d,%d)",m->x1, m->y1);
      }
    }

    draw_line_special(0, 0, tb_width(), TB_YELLOW | TB_BOLD, TB_BLUE);
    draw_text_special(0, 0, buf, TB_YELLOW | TB_BOLD, TB_BLUE);
  }

  if (s->current_error != WD_ERROR_NONE) {
    snprintf(buf, sizeof(buf), "Error 0x%02x: %s", s->current_error,
             s->current_error_message);

    draw_line_special(0, tb_height() - 2, tb_width(), TB_WHITE | TB_BOLD,
                      TB_RED);
    draw_text_special(0, tb_height() - 2, buf, TB_WHITE | TB_BOLD, TB_RED);
  } else if (strlen(s->current_message) > 0) {
    snprintf(buf, sizeof(buf), "Message: %s", s->current_message);

    draw_line_special(0, tb_height() - 2, tb_width(), TB_BLACK, TB_YELLOW);
    draw_text_special(0, tb_height() - 2, buf, TB_BLACK, TB_YELLOW);
  }

  draw_line_special(0, tb_height() - 1, tb_width(), TB_YELLOW | TB_BOLD,
                    TB_BLUE);
  draw_text_special(0, tb_height() - 1,
                    "^O=Open ^X=Exit ^R=Refresh ESC=Close Message "
                    "UP/DOWN/PGUP/PGDN/HOME/END/SPACE=Scroll [/]=Prev/Next Link ENTER=Activate Link",
                    TB_YELLOW | TB_BOLD, TB_BLUE);

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

void wd_exec(struct wd_s *s) {
  cmark_event_type ev;
  cmark_iter *iter;
  cmark_node *node;
  const char *info, *code;
  duk_context *vm;

  iter = NULL;
  vm = NULL;

  if (s->current_doc == NULL) {
    goto exit;
  }

  vm = duk_create_heap_default();

  duk_push_pointer(vm, s);
  duk_put_global_string(vm, "window");

  wd_vm__Node__define(vm);

  duk_get_global_string(vm, "Node");
  duk_push_pointer(vm, s->current_doc);
  duk_new(vm, 1);
  duk_put_global_string(vm, "document");

  duk_push_c_function(vm, wd_vm_set_message, 1);
  duk_put_global_string(vm, "set_message");

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

    if (duk_peval_string(vm, code) != 0) {
      s->current_error = WD_ERROR_SCRIPT_EXCEPTION;
      strncpy(s->current_error_message, duk_safe_to_string(vm, -1),
              sizeof(s->current_error_message));
    }

    duk_pop(vm);
  }

  s->current_vm = vm;

exit:
  if (iter != NULL) {
    cmark_iter_free(iter);
    iter = NULL;
  }

  if (vm != NULL && vm != s->current_vm) {
    duk_destroy_heap(vm);
    vm = NULL;
  }
}

int main(int argc, char **argv) {
  struct wd_s s;
  struct tb_event ev;
  char buf[10000];

  memset(&s, 0, sizeof(struct wd_s));
  memset(buf, 0, sizeof(buf));

  cmark_gfm_core_extensions_ensure_registered();

  curl_global_init(CURL_GLOBAL_DEFAULT);

  if (tb_init() != 0) {
    perror("tb_init");

    return 1;
  }

  if (argc > 1) {
    if (strstr(argv[1], "://") == NULL) {
      if (strcmp(argv[1], "/") != 0 && strcmp(argv[1], "./") != 0 && strcmp(argv[1], "../") != 0) {
        snprintf(buf, sizeof(buf), "file://./%s", argv[1]);
        wd_open_url(&s, buf);
      } else {
        snprintf(buf, sizeof(buf), "file://%s", argv[1]);
        wd_open_url(&s, buf);
      }
    } else {
      wd_open_url(&s, argv[1]);
    }
  }

  while (!s.quit) {
    wd_render(&s);
    tb_poll_event(&ev);
    wd_handle_ev(&s, &ev);
  }

  tb_shutdown();

  if (s.current_src != NULL) {
    buf_free(s.current_src);
    s.current_src = NULL;
  }

  if (s.current_doc != NULL) {
    wd_node_free(s.current_doc);
    s.current_doc = NULL;
  }

  if (s.current_vm != NULL) {
    duk_destroy_heap(s.current_vm);
    s.current_vm = NULL;
  }

  return 0;
}
