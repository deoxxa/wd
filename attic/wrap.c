#include <ctype.h>
#include <string.h>

#include "buf.h"

void wrap(struct buf_s *buf, const char *src, int width);
int wrap_lines(const char *str);

void wrap(struct buf_s *buf, const char *src, int width) {
  int wlen, llen, rlen, i;

  wlen = 0;
  llen = 0;

  while (*src) {
    if ((wlen = strspn(src, " \r\t"))) {
      src += wlen;
      rlen += wlen;
    } else if (*src == '\n') {
      buf_append(buf, "\n", 1);
      src++;
      llen = 0;
    } else if ((wlen = strcspn(src, " \n\t"))) {
      if (llen + wlen >= width) {
        if (wlen > width) {
          for (i = 0; i < wlen; i += width - 1) {
            buf_append(buf, "\n", 1);
            if (wlen - i > width - 1) {
              buf_append(buf, &(src[i]), width - 1);
              buf_append(buf, "-", 1);
            } else {
              buf_append(buf, &(src[i]), wlen - i);
            }
          }
        } else {
          buf_append(buf, "\n", 1);
          buf_append(buf, src, wlen);
          llen = wlen;
        }
      } else {
        if (llen > 0) {
          buf_append(buf, " ", 1);
          llen += 1;
        }

        buf_append(buf, src, wlen);
        llen += wlen;
      }

      src += wlen;
    }
  }
}

int wrap_lines(const char *str) {
  char c;
  int n;

  n = 1;

  while ((c = *str++)) {
    if (c == '\n') {
      n += 1;
    }
  }

  return n;
}
