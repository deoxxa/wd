#include <stdlib.h>
#include <string.h>

struct buf_s {
  char *buf;
  size_t len, max;
};

struct buf_s *buf_new(size_t max);
void buf_free(struct buf_s *b);
void buf_append(struct buf_s *b, const char *data, size_t len);
size_t buf_write_cb(void *data, size_t len, size_t nmemb, void *ptr);

struct buf_s *buf_new(size_t max) {
  struct buf_s *r;
  r = malloc(sizeof(struct buf_s));
  r->buf = malloc(1);
  r->buf[0] = 0;
  r->len = 0;
  r->max = max;
  return r;
}

void buf_free(struct buf_s *b) {
  if (b == NULL) {
    return;
  }

  free(b->buf);
  free(b);
}

void buf_append(struct buf_s *b, const char *data, size_t len) {
  b->buf = realloc(b->buf, b->len + len + 1);
  memcpy(&(b->buf[b->len]), data, len);
  b->len += len;
  b->buf[b->len] = 0;
}

size_t buf_write_cb(void *data, size_t len, size_t nmemb, void *ptr) {
  struct buf_s *b;
  size_t rlen, nlen;

  b = ptr;
  rlen = len * nmemb;
  nlen = b->len + rlen;

  /* maximum buffer size exceeded */
  if (b->max != 0 && nlen > b->max) {
    return 0;
  }

  if ((b->buf = realloc(b->buf, b->len + rlen + 1)) == NULL) {
    return 0;
  }

  memcpy(&(b->buf[b->len]), data, rlen);
  b->len += rlen;
  b->buf[b->len] = 0;

  return rlen;
}