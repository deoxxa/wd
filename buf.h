struct buf_s {
  char *buf;
  size_t len, max;
};

struct buf_s *buf_new(size_t max);
void buf_free(struct buf_s *b);
void buf_append(struct buf_s *b, const char *data, size_t len);
size_t buf_write_cb(void *data, size_t len, size_t nmemb, void *ptr);
