#include <stdlib.h>
#include <string.h>

#include <termbox.h>

void draw_line_special(int x, int y, int w, uint16_t fg, uint16_t bg);
void draw_text_special(int x, int y, const char *str, uint16_t fg, uint16_t bg);
void draw_text_specialn(int x, int y, const char *str, size_t len, uint16_t fg,
                        uint16_t bg);
void draw_text(int x, int y, const char *str);
void draw_textn(int x, int y, const char *str, size_t len);

void draw_line_special(int x, int y, int w, uint16_t fg, uint16_t bg) {
  int i;

  for (i = 0; i < w; i++) {
    tb_change_cell(x + i, y, ' ', fg, bg);
  }
}

void draw_text_specialn(int x, int y, const char *str, size_t len, uint16_t fg,
                        uint16_t bg) {
  char c;
  unsigned int i, w;

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