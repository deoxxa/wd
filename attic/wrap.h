#define WRAP_MAX(len, width) (len + ((len / width) * 2) + 1)

void wrap(struct buf_s *buf, const char *src, int width);
int wrap_lines(const char *str);
