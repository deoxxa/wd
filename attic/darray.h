#include <stdlib.h>
#include <string.h>

#define DARRAY_OF(type, name)           \
  struct name##_s {                     \
    type *buf;                          \
    int len;                            \
  };                                    \
  struct name##_s *name##_new();        \
  void name##_free(struct name##_s *b); \
  void name##_append(struct name##_s *b, type *data, int len);

#define DARRAY_OF_IMPL_NEW(type, name)   \
  struct name##_s *name##_new() {        \
    struct name##_s *r;                  \
    r = malloc(sizeof(struct name##_s)); \
    r->buf = malloc(sizeof(type));       \
    memset(r->buf, 0, sizeof(type));     \
    r->len = 0;                          \
    return r;                            \
  }

#define DARRAY_OF_IMPL_FREE(type, name)  \
  void name##_free(struct name##_s *b) { \
    if (b == NULL) {                     \
      return;                            \
    }                                    \
    free(b->buf);                        \
    free(b);                             \
  }

#define DARRAY_OF_IMPL_APPEND(type, name)                       \
  void name##_append(struct name##_s *b, type *data, int len) { \
    b->buf = realloc(b->buf, b->len + len + 1);                 \
    memcpy(&(b->buf[b->len]), data, len);                       \
    b->len += len;                                              \
    memset(&(b->buf[b->len]), 0, sizeof(type));                 \
  }

#define DARRAY_OF_IMPL_GET(type, name)                     \
  int name##_get(struct name##_s *b, int idx, type *out) { \
    if (idx >= b->len - 1) {                               \
      return 1;                                            \
    }                                                      \
    *out = b->buf[idx];                                    \
    return 0;                                              \
  }

#define DARRAY_OF_IMPL(type, name)  \
  DARRAY_OF_IMPL_NEW(type, name)    \
  DARRAY_OF_IMPL_FREE(type, name)   \
  DARRAY_OF_IMPL_APPEND(type, name) \
  DARRAY_OF_IMPL_GET(type, name)
