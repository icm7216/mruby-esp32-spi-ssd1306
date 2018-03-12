#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/value.h>
#include <mruby/variable.h>
#include <mruby/data.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

#include "tiny_grafx.h"

// D/C pin mode, command or data
enum {
    DC_CMD,
    DC_DATA
};

// DMA channel
#define NO_DMA  0
#define DMA_CH1 1
#define DMA_CH2 2

// NO_DMA mode transaction data size is up to 32 bytes at a time.
#define NO_DMA_TRANSACTION_DATA_SIZE 32 

// default SSD1306 wiring and SPI configuration
#define PIN_NUM_CS   5
#define PIN_NUM_DC   16
#define PIN_NUM_RST  17
#define PIN_NUM_MOSI 23
#define PIN_NUM_SCK  18
#define PIN_NUM_MISO 19
#define CLOCK_SPEED_HZ (10*1000*1000)   // SPI Clock freq=10 MHz
#define SPI_MODE 0
#define DMA DMA_CH1                     // default DMA channel = 1

// SPI HOST, only HSPI or VSPI
#define HOST VSPI_HOST

// SPI Object
typedef struct spi_config_t {
  uint8_t num_cs;           // Chip Select pin num
  uint8_t num_dc;           // Data/Command select pin num
  uint8_t num_rst;          // RESET pin num
  uint8_t num_mosi;         // MOSI pin num
  uint8_t num_sck;          // SPI Clock pin num
  uint8_t num_miso;         // MISO pin num
  uint32_t spi_freq;        // SPI clock frequency [Hz]
  uint8_t spi_mode;         // SPI mode (0-3)
  uint8_t dma_ch;           // No DMA or DMA channel (1 or 2)
  spi_device_handle_t spi;  // Handle for a device on a SPI bus
} spi_config_t;

static const char *TAG = "SPI_SSD1306";


// ----- Common graphics methods -----
// mruby binding of manipulate the graphics

static mrb_value
ssd1306_clear(mrb_state *mrb, mrb_value self)
{
  buffer_clear();
  return self;
}

static mrb_value
ssd1306_set_pixel(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y;  
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "ii", &x, &y);

	set_pixel(x, y, color); 
  return mrb_nil_value();
}

static mrb_value
ssd1306_get_pixel(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y;  
  int16_t pixel;
  mrb_get_args(mrb, "ii", &x, &y);
	pixel = get_pixel(x, y);

  return mrb_fixnum_value(pixel);
}

static mrb_value
ssd1306_draw_line(mrb_state *mrb, mrb_value self)
{
  mrb_int x0, y0, x1, y1;
  
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  
  mrb_get_args(mrb, "iiii", &x0, &y0, &x1, &y1);

  if ((color < BLACK) || (color > INVERT)) {
    color = WHITE;
  }
  draw_line(x0, y0, x1, y1, color);
  
  return mrb_nil_value();
}

static mrb_value
ssd1306_draw_vertical_line(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, h;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &h);

	draw_vertical_line(x, y, h, color);
	return mrb_nil_value();
}

static mrb_value
ssd1306_draw_horizontal_line(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &w);

	draw_horizontal_line(x, y, w, color);
	return mrb_nil_value();
}

static mrb_value
ssd1306_draw_rect(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w, h;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iiii", &x, &y, &w, &h);

	draw_rect(x, y, w, h, color);
	return mrb_nil_value();
}

static mrb_value
ssd1306_draw_fill_rect(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w, h;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iiii", &x, &y, &w, &h);

	draw_fill_rect(x, y, w, h, color);
	return mrb_nil_value();
}

static mrb_value
ssd1306_draw_circle(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, r;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &r);

	draw_circle(x, y, r, color);
	return mrb_nil_value();
}

static mrb_value
ssd1306_draw_fill_circle(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y, r;
  int16_t color;
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &r);

	draw_fill_circle(x, y, r, color);
	return mrb_nil_value();
}

// mruby binding of Display a character string
static mrb_value
ssd1306_text(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y;
  mrb_value data;
  int16_t color, fontsize;

  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  fontsize = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fontsize")));
  mrb_get_args(mrb, "iiS", &x, &y, &data);
  display_text(x, y, RSTRING_PTR(data), RSTRING_LEN(data), color, fontsize);
  // ESP_LOGI(TAG, "color:%d, size:%d, text:%s", color, fontsize, RSTRING_PTR(data));

  return mrb_nil_value();
}
// ----- Common graphics methods -----




// ----- SSD1306 methods and functions -----

// This function is called (in irq context!) just before a transmission starts.
// It will set the D/C line to the value indicated in the user field.
static void
spi_pre_transfer_callback(spi_transaction_t *t)
{
  uint32_t dc = (uint32_t)t->user;
  gpio_set_level(PIN_NUM_DC, dc);
}

// reset the D/C line
static void
spi_post_transfer_callback(spi_transaction_t *t)
{
  gpio_set_level(PIN_NUM_DC, 0);
}

// Send buffer data to the OLED
// NOTE: NO_DMA mode can transmit up to 32 bytes at a time.
static void
send_data(spi_config_t *spicfg, const uint8_t *data, int16_t len, int32_t dc)
{
  esp_err_t err;
  spi_transaction_t tx;

  if (spicfg->dma_ch == 0) {
    // NO_DMA mode
    int16_t max_len, tx_len, left_len;
    void *cur_data = (void *)data;
    max_len = NO_DMA_TRANSACTION_DATA_SIZE;
    left_len = len;

    while (left_len > 0) {
      tx_len = (left_len > max_len) ? max_len : left_len;
      memset(&tx, 0, sizeof(tx));
      tx.length = tx_len * 8;   // tx_len is in bytes, transaction length is in bits.
      tx.tx_buffer = cur_data;  // Transmit data
      tx.user = (void*)dc;      // D/C needs to be set to 1
      err = spi_device_queue_trans(spicfg->spi, &tx, 1000 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "send_data: spi_device_queue_trans error=%d", err);
      }
      
      spi_transaction_t *rx;
      err = spi_device_get_trans_result(spicfg->spi, &rx, 1000 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "send_data: spi_device_get_trans_result error=%d", err);
      }
      left_len -= tx_len;
      cur_data += tx_len;
    }
  } else {
    // Use DMA mode
    memset(&tx, 0, sizeof(tx));
    tx.length = len * 8;        // len is in bytes, transaction length is in bits.
    tx.tx_buffer = data;        // Transmit data
    tx.user = (void*)dc;        // D/C needs to be set to 1
    err = spi_device_queue_trans(spicfg->spi, &tx, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "send_data: spi_device_queue_trans error=%d", err);
    }

    spi_transaction_t *rx;
    err = spi_device_get_trans_result(spicfg->spi, &rx, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "send_data: spi_device_get_trans_result error=%d", err);
    }
  }
}

// Send buffer to display
static void
ssd1306_display(spi_config_t *spicfg)
{
  uint8_t *buffer;

  if (spicfg->dma_ch == 0) {
    // NO DMA
    buffer = (uint8_t *)malloc(DISPLAY_PIXSEL);
  } else {
    // Use DMA_CH1 or DMA_CH2
    buffer = (uint8_t *)heap_caps_malloc(DISPLAY_PIXSEL, MALLOC_CAP_DMA);
  }

  if (buffer != NULL) {
    memset(buffer, 0, DISPLAY_PIXSEL);
    buffer_read(buffer, DISPLAY_PIXSEL);
    send_data(spicfg, buffer, DISPLAY_PIXSEL, DC_DATA);
  }

  if (spicfg->dma_ch == 0) {
    free(buffer);
  } else {
    heap_caps_free(buffer);
  }
}

// display the frame buffer
static mrb_value
ssd1306_spi_display(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = DATA_PTR(self);
  ssd1306_display(spicfg);

  return mrb_nil_value();
}

// Initialize the SPI manter
static void
spi_bus_init(spi_config_t *spicfg)
{
  spi_bus_config_t buscfg = {
    .miso_io_num = spicfg->num_miso,
    .mosi_io_num = spicfg->num_mosi,
    .sclk_io_num = spicfg->num_sck,
    .quadwp_io_num = -1,  // WP (Write Protect) signal, or -1 if not used.
    .quadhd_io_num = -1   // HD (HolD) signal, or -1 if not used.
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = spicfg->spi_freq,
    .mode = spicfg->spi_mode,
    .spics_io_num = spicfg->num_cs,
    .queue_size = 1,
    .pre_cb = spi_pre_transfer_callback,
    .post_cb = spi_post_transfer_callback
  };

  esp_err_t err;
  spi_device_handle_t spi;

  // Initialize the SPI bus
  err = spi_bus_initialize(HOST, &buscfg, spicfg->dma_ch);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "spi_bus_init: spi_bus_initialize error=%d", err);
  }

  // Attach the OLED to the SPI bus
  err = spi_bus_add_device(HOST, &devcfg, &spi);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "spi_bus_init: spi_bus_add_device error=%d", err);
  }

  // Save the SPI device handle to SPI Object.
  spicfg->spi = spi;
}

static void
spi_deinit(spi_config_t *spicfg)
{
  free(spicfg->spi);
}

// SSD1306 init commands
DRAM_ATTR static const uint8_t ssd1306_init_cmds[] = {
  0xAE,               // display OFF
  0xA8, 0x3F,         // MUX ratio (0x3F = 64d -1d)
  0xD3, 0x00,         // set display offset (no offset)
  0x40,               // set display start line
  0xA1,               // re-map, SEG0 is mapped to column address 127
  0xC8,               // scan direction, reverse up-bottom
  0xDA, 0x12,         // set COM pins (Alternative configuration, Disable L/R remap)
  0x81, 0x7F,         // set contrast
  0x2E,               // stop scrolling
  0xA4,               // resume ram content display
  0xD5, 0x00,         // set osc frequency
  0x8D, 0x14,         // enable charge pump
  0x20, 0x00,         // ADDR_MODE, 0x00 = Horizontal Mode
  0x21, 0x00, 0x7F,   // COLUMN_ADDR, 0x00 = start, 0x7f = end
  0x22, 0x00, 0x07,   // PAGE_ADDR, 0x00 = start, 0x7f = end
  0xAF                // display ON
};

// SSD1306 Initialize
static void
ssd1306_init(spi_config_t *spicfg)
{
  // Initialize non-SPI GPIOs
  gpio_set_direction(spicfg->num_dc, GPIO_MODE_OUTPUT);
  gpio_set_direction(spicfg->num_rst, GPIO_MODE_OUTPUT);
  gpio_set_direction(spicfg->num_cs, GPIO_MODE_OUTPUT);

  // Reset the display
  gpio_set_level(spicfg->num_rst, 1);
  gpio_set_level(spicfg->num_rst, 0);
  vTaskDelay(10 / portTICK_PERIOD_MS);
  gpio_set_level(spicfg->num_rst, 1);

  // Send all commands
  send_data(spicfg, ssd1306_init_cmds, sizeof(ssd1306_init_cmds), DC_CMD);

  // Clear the OLED frame buffer
  buffer_clear();
}

// mruby data_type
static const struct mrb_data_type mrb_spi_config_type = {
  "spi_config_type", mrb_free
};

// Initialize the SPI device for SSD1306
static mrb_value
spi_init(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = (spi_config_t *)DATA_PTR(self);
  if (spicfg) {
    mrb_free(mrb, spicfg);
  }
  DATA_PTR(self) = NULL;

  // Get config param
  mrb_int cs, dc, rst, mosi, sck, miso, freq, spi_mode, dma_ch;
  mrb_get_args(mrb, "iiiiiiiii", &cs, &dc, &rst, &mosi, &sck, &miso, &freq, &spi_mode, &dma_ch);

  // SSD1306 SPI bus config
  spicfg = (spi_config_t *)mrb_malloc(mrb, sizeof(spi_config_t));
  spicfg->num_cs   = cs;
  spicfg->num_dc   = dc;
  spicfg->num_rst  = rst;
  spicfg->num_mosi = mosi;
  spicfg->num_sck  = sck;
  spicfg->num_miso = miso;
  spicfg->spi_freq = freq;
  spicfg->spi_mode = spi_mode;
  spicfg->dma_ch   = dma_ch;
  DATA_TYPE(self) = &mrb_spi_config_type;
  DATA_PTR(self)  = spicfg;

  // Initialize the SPI
  spi_bus_init(spicfg);

  // Initialize the SSD1306
  ssd1306_init(spicfg);

  return self;
}

// Object duplication method
static mrb_value
spi_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value src;

  mrb_get_args(mrb, "o", &src);
  if (mrb_obj_equal(mrb, copy, src)) return copy;
  
  if (!mrb_obj_is_instance_of(mrb, src, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  
  if (!DATA_PTR(copy)) {
    DATA_PTR(copy) = (spi_config_t *)mrb_malloc(mrb, sizeof(spi_config_t));
    DATA_TYPE(copy) = &mrb_spi_config_type;
  }

  *(spi_config_t *)DATA_PTR(copy) = *(spi_config_t *)DATA_PTR(src);
  
  return copy;
}

// View config setting
static mrb_value
spi_view_config(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = DATA_PTR(self);
  mrb_value spi_param = mrb_ary_new_capa(mrb, 9);
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_cs));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_dc));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_rst));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_mosi));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_sck));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_miso));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->spi_freq));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->spi_mode));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->dma_ch));

  return spi_param;
}

void
mrb_mruby_esp32_spi_ssd1306_gem_init(mrb_state* mrb)
{
  struct RClass *oled = mrb_define_module(mrb, "OLED");
  mrb_define_const(mrb, oled, "BLACK", mrb_fixnum_value(BLACK));
  mrb_define_const(mrb, oled, "WHITE", mrb_fixnum_value(WHITE));
  mrb_define_const(mrb, oled, "INVERT", mrb_fixnum_value(INVERT));

  struct RClass *ssd1306 = mrb_define_class_under(mrb, oled, "SSD1306SPI", mrb->object_class);
  MRB_SET_INSTANCE_TT(ssd1306, MRB_TT_DATA);

  // ssd1306 spi method
  mrb_define_method(mrb, ssd1306, "_init", spi_init, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssd1306, "initialize_copy", spi_init_copy, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, ssd1306, "config?", spi_view_config, MRB_ARGS_NONE());

  // manipulate graphics
  mrb_define_method(mrb, ssd1306, "clear", ssd1306_clear, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssd1306, "set_pixel", ssd1306_set_pixel, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, ssd1306, "get_pixel", ssd1306_get_pixel, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, ssd1306, "line", ssd1306_draw_line, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, ssd1306, "vline", ssd1306_draw_vertical_line, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, ssd1306, "hline", ssd1306_draw_horizontal_line, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, ssd1306, "rect", ssd1306_draw_rect, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, ssd1306, "fill_rect", ssd1306_draw_fill_rect, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, ssd1306, "circle", ssd1306_draw_circle, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, ssd1306, "fill_circle", ssd1306_draw_fill_circle, MRB_ARGS_REQ(3));

  // Display a character string
  mrb_define_method(mrb, ssd1306, "display", ssd1306_spi_display, MRB_ARGS_NONE());
  mrb_define_method(mrb, ssd1306, "text", ssd1306_text, MRB_ARGS_REQ(3));

  struct RClass *constants = mrb_define_module_under(mrb, ssd1306, "Constants");
  mrb_define_const(mrb, constants, "CS",        mrb_fixnum_value(PIN_NUM_CS));
  mrb_define_const(mrb, constants, "DC",        mrb_fixnum_value(PIN_NUM_DC));
  mrb_define_const(mrb, constants, "RST",       mrb_fixnum_value(PIN_NUM_RST));
  mrb_define_const(mrb, constants, "MOSI",      mrb_fixnum_value(PIN_NUM_MOSI));
  mrb_define_const(mrb, constants, "SCK",       mrb_fixnum_value(PIN_NUM_SCK));
  mrb_define_const(mrb, constants, "MISO",      mrb_fixnum_value(PIN_NUM_MISO));
  mrb_define_const(mrb, constants, "SPI_FREQ",  mrb_fixnum_value(CLOCK_SPEED_HZ));
  mrb_define_const(mrb, constants, "SPI_MODE",  mrb_fixnum_value(SPI_MODE));
  mrb_define_const(mrb, constants, "DMA",       mrb_fixnum_value(DMA));
  mrb_define_const(mrb, constants, "NO_DMA",    mrb_fixnum_value(NO_DMA));
  mrb_define_const(mrb, constants, "DMA_CH1",   mrb_fixnum_value(DMA_CH1));
  mrb_define_const(mrb, constants, "DMA_CH2",   mrb_fixnum_value(DMA_CH2));
}

void
mrb_mruby_esp32_spi_ssd1306_gem_final(mrb_state* mrb)
{
}

