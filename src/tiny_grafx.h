#ifndef TINYGRAFXH_
#define TINYGRAFXH_

#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_PIXSEL (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)
#define FONT_WIDTH      8
#define FONT_HEIGHT     8 

#define BLACK   0
#define WHITE   1
#define INVERT  2

// manipulate the graphics
#define swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }

void buffer_clear();
void buffer_read(uint8_t *data, int16_t size);
void set_pixel(int16_t x, int16_t y, uint16_t color) ;
int16_t get_pixel(int16_t x, int16_t y);
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);
void draw_vertical_line(int16_t x, int16_t y, int16_t h, int16_t color);
void draw_horizontal_line(int16_t x, int16_t y, int16_t w, int16_t color);
void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t color);
void draw_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t color);
void draw_circle(int16_t x0, int16_t y0, int16_t r, int16_t color);
void draw_fill_circle(int16_t x0, int16_t y0, int16_t r, int16_t color);

// Display a character string
void draw_char(int16_t x, int16_t y, uint8_t c, int16_t color, int16_t fontsize);
void display_text(int16_t x, int16_t y, uint8_t *text, int16_t length, int16_t color, int16_t fontsize);

#endif /* TINYGRAFXH_ */
