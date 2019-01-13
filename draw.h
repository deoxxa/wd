void draw_line_special(int x, int y, int w, uint16_t fg, uint16_t bg);
void draw_text_special(int x, int y, const char *str, uint16_t fg, uint16_t bg);
void draw_text_specialn(int x, int y, const char *str, size_t len, uint16_t fg,
                        uint16_t bg);
void draw_text(int x, int y, const char *str);
void draw_textn(int x, int y, const char *str, size_t len);
