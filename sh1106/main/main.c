#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "SH1106"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_GPIO 9
#define I2C_SCL_GPIO 10
#define I2C_CLK_HZ 400000
#define I2C_TIMEOUT_MS 100

#define SH1106_ADDR 0x3C
#define SH1106_WIDTH 128
#define SH1106_HEIGHT 64
#define SH1106_PAGE_COUNT (SH1106_HEIGHT / 8)
#define SH1106_COL_OFFSET 2

static uint8_t s_framebuffer[SH1106_WIDTH * SH1106_PAGE_COUNT];
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_i2c_dev = NULL;
static uint32_t s_frame_counter = 0;
static uint32_t s_fps = 0;
static int64_t s_fps_window_start_us = 0;
static uint32_t s_fps_window_frames = 0;

static const uint8_t s_font_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t s_font_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static const uint8_t s_font_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t s_font_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
static const uint8_t s_font_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
static const uint8_t s_font_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t s_font_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t s_font_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static const uint8_t s_font_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t s_font_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t s_font_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
static const uint8_t s_font_a[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
static const uint8_t s_font_b[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
static const uint8_t s_font_c[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static const uint8_t s_font_d[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
static const uint8_t s_font_e[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
static const uint8_t s_font_f[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
static const uint8_t s_font_g[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
static const uint8_t s_font_h[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
static const uint8_t s_font_i[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
static const uint8_t s_font_j[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
static const uint8_t s_font_k[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t s_font_l[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
static const uint8_t s_font_m[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
static const uint8_t s_font_n[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static const uint8_t s_font_o[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t s_font_p[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
static const uint8_t s_font_q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
static const uint8_t s_font_r[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static const uint8_t s_font_s[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
static const uint8_t s_font_t[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t s_font_u[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t s_font_v[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
static const uint8_t s_font_w[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
static const uint8_t s_font_x[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
static const uint8_t s_font_y[5] = {0x03, 0x04, 0x78, 0x04, 0x03};
static const uint8_t s_font_z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};
static const uint8_t s_font_dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};

static const uint8_t *font5x7_get(char c)
{
	if (c >= 'a' && c <= 'z') {
		c = (char)(c - ('a' - 'A'));
	}

	switch (c) {
	case '0': return s_font_0;
	case '1': return s_font_1;
	case '2': return s_font_2;
	case '3': return s_font_3;
	case '4': return s_font_4;
	case '5': return s_font_5;
	case '6': return s_font_6;
	case '7': return s_font_7;
	case '8': return s_font_8;
	case '9': return s_font_9;
	case 'A': return s_font_a;
	case 'B': return s_font_b;
	case 'C': return s_font_c;
	case 'D': return s_font_d;
	case 'E': return s_font_e;
	case 'F': return s_font_f;
	case 'G': return s_font_g;
	case 'H': return s_font_h;
	case 'I': return s_font_i;
	case 'J': return s_font_j;
	case 'K': return s_font_k;
	case 'L': return s_font_l;
	case 'M': return s_font_m;
	case 'N': return s_font_n;
	case 'O': return s_font_o;
	case 'P': return s_font_p;
	case 'Q': return s_font_q;
	case 'R': return s_font_r;
	case 'S': return s_font_s;
	case 'T': return s_font_t;
	case 'U': return s_font_u;
	case 'V': return s_font_v;
	case 'W': return s_font_w;
	case 'X': return s_font_x;
	case 'Y': return s_font_y;
	case 'Z': return s_font_z;
	case '-': return s_font_dash;
	case ' ': return s_font_space;
	default: return s_font_space;
	}
}

static esp_err_t sh1106_write_command(uint8_t cmd)
{
	uint8_t payload[2] = {0x00, cmd};
	return i2c_master_transmit(s_i2c_dev, payload, sizeof(payload), I2C_TIMEOUT_MS);
}

static esp_err_t sh1106_write_data(const uint8_t *data, size_t len)
{
	uint8_t tx[17];
	tx[0] = 0x40;

	size_t index = 0;
	while (index < len) {
		size_t chunk = len - index;
		if (chunk > 16) {
			chunk = 16;
		}
		memcpy(&tx[1], &data[index], chunk);
		esp_err_t err = i2c_master_transmit(s_i2c_dev, tx, chunk + 1, I2C_TIMEOUT_MS);
		if (err != ESP_OK) {
			return err;
		}
		index += chunk;
	}

	return ESP_OK;
}

static esp_err_t sh1106_i2c_init(void)
{
	i2c_master_bus_config_t bus_config = {
		.i2c_port = I2C_PORT,
		.sda_io_num = I2C_SDA_GPIO,
		.scl_io_num = I2C_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_i2c_bus), TAG, "i2c_new_master_bus failed");

	i2c_device_config_t dev_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = SH1106_ADDR,
		.scl_speed_hz = I2C_CLK_HZ,
	};
	return i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_i2c_dev);
}

static esp_err_t sh1106_init(void)
{
	const uint8_t init_cmds[] = {
		0xAE,
		0xD5, 0x80,
		0xA8, 0x3F,
		0xD3, 0x00,
		0x40,
		0xAD, 0x8B,
		0xA1,
		0xC8,
		0xDA, 0x12,
		0x81, 0x7F,
		0xD9, 0x22,
		0xDB, 0x20,
		0xA4,
		0xA6,
		0xAF,
	};

	for (size_t i = 0; i < sizeof(init_cmds); i++) {
		ESP_RETURN_ON_ERROR(sh1106_write_command(init_cmds[i]), TAG, "init command 0x%02X failed", init_cmds[i]);
	}

	return ESP_OK;
}

static esp_err_t sh1106_flush(void)
{
	for (uint8_t page = 0; page < SH1106_PAGE_COUNT; page++) {
		ESP_RETURN_ON_ERROR(sh1106_write_command((uint8_t)(0xB0 + page)), TAG, "set page failed");
		ESP_RETURN_ON_ERROR(sh1106_write_command((uint8_t)(0x00 | (SH1106_COL_OFFSET & 0x0F))), TAG, "set low col failed");
		ESP_RETURN_ON_ERROR(sh1106_write_command((uint8_t)(0x10 | ((SH1106_COL_OFFSET >> 4) & 0x0F))), TAG, "set high col failed");

		const uint8_t *page_data = &s_framebuffer[page * SH1106_WIDTH];
		ESP_RETURN_ON_ERROR(sh1106_write_data(page_data, SH1106_WIDTH), TAG, "page write failed");
	}
	return ESP_OK;
}

static void gfx_clear(void)
{
	memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
}

static void gfx_pixel(int x, int y, bool on)
{
	if (x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT) {
		return;
	}

	const int index = x + (y / 8) * SH1106_WIDTH;
	const uint8_t bit = (uint8_t)(1U << (y % 8));
	if (on) {
		s_framebuffer[index] |= bit;
	} else {
		s_framebuffer[index] &= (uint8_t)~bit;
	}
}

static void gfx_line(int x0, int y0, int x1, int y1, bool on)
{
	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	while (true) {
		gfx_pixel(x0, y0, on);
		if (x0 == x1 && y0 == y1) {
			break;
		}
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void gfx_rect(int x, int y, int w, int h, bool on)
{
	gfx_line(x, y, x + w - 1, y, on);
	gfx_line(x, y + h - 1, x + w - 1, y + h - 1, on);
	gfx_line(x, y, x, y + h - 1, on);
	gfx_line(x + w - 1, y, x + w - 1, y + h - 1, on);
}

static void gfx_fill_rect(int x, int y, int w, int h, bool on)
{
	for (int yy = y; yy < y + h; yy++) {
		gfx_line(x, yy, x + w - 1, yy, on);
	}
}

static void gfx_circle(int xc, int yc, int r, bool on)
{
	int x = 0;
	int y = r;
	int d = 3 - 2 * r;

	while (y >= x) {
		gfx_pixel(xc + x, yc + y, on);
		gfx_pixel(xc - x, yc + y, on);
		gfx_pixel(xc + x, yc - y, on);
		gfx_pixel(xc - x, yc - y, on);
		gfx_pixel(xc + y, yc + x, on);
		gfx_pixel(xc - y, yc + x, on);
		gfx_pixel(xc + y, yc - x, on);
		gfx_pixel(xc - y, yc - x, on);
		x++;

		if (d > 0) {
			y--;
			d += 4 * (x - y) + 10;
		} else {
			d += 4 * x + 6;
		}
	}
}

static void gfx_char(int x, int y, char c, bool on)
{
	const uint8_t *glyph = font5x7_get(c);
	for (int col = 0; col < 5; col++) {
		uint8_t bits = glyph[col];
		for (int row = 0; row < 7; row++) {
			if ((bits >> row) & 0x01) {
				gfx_pixel(x + col, y + row, on);
			}
		}
	}
}

static void gfx_text(int x, int y, const char *text, bool on)
{
	for (size_t i = 0; text[i] != '\0'; i++) {
		gfx_char(x + (int)i * 6, y, text[i], on);
	}
}

static void gfx_hud(const char *scene)
{
	char line2[22];
	gfx_text(2, 0, scene, true);
	(void)snprintf(line2, sizeof(line2), "FPS %lu CNT %lu", (unsigned long)s_fps, (unsigned long)s_frame_counter);
	gfx_text(2, 8, line2, true);
}

static void run_graphics_tests(void)
{
	gfx_clear();
	gfx_hud("LINES");
	for (int x = 0; x < SH1106_WIDTH; x += 4) {
		gfx_line(0, 16, x, SH1106_HEIGHT - 1, true);
	}
	ESP_ERROR_CHECK(sh1106_flush());
	vTaskDelay(pdMS_TO_TICKS(1200));

	gfx_clear();
	gfx_hud("RECTS");
	for (int i = 0; i < 32; i += 2) {
		gfx_rect(i * 2, i + 16, SH1106_WIDTH - i * 4, SH1106_HEIGHT - i * 2 - 16, true);
	}
	ESP_ERROR_CHECK(sh1106_flush());
	vTaskDelay(pdMS_TO_TICKS(1200));

	gfx_clear();
	gfx_hud("CIRCLES");
	for (int r = 4; r < 34; r += 4) {
		gfx_circle(SH1106_WIDTH / 2, (SH1106_HEIGHT / 2) + 8, r, true);
	}
	ESP_ERROR_CHECK(sh1106_flush());
	vTaskDelay(pdMS_TO_TICKS(1200));

	for (int frame = 0; frame < 80; frame++) {
		const int64_t now_us = esp_timer_get_time();
		if (s_fps_window_start_us == 0) {
			s_fps_window_start_us = now_us;
		}
		s_fps_window_frames++;
		if ((now_us - s_fps_window_start_us) >= 1000000) {
			s_fps = s_fps_window_frames;
			s_fps_window_frames = 0;
			s_fps_window_start_us = now_us;
		}
		s_frame_counter++;

		gfx_clear();
		gfx_hud("BOUNCE");
		int x = (frame * 2) % (SH1106_WIDTH - 24);
		int y = 16 + ((frame * 3) % (SH1106_HEIGHT - 32));
		gfx_fill_rect(x, y, 24, 16, true);
		gfx_rect(0, 15, SH1106_WIDTH, SH1106_HEIGHT - 15, true);
		ESP_ERROR_CHECK(sh1106_flush());
		vTaskDelay(pdMS_TO_TICKS(35));
	}
}

void app_main(void)
{
	ESP_LOGI(TAG, "Starting SH1106 graphics test");
	ESP_ERROR_CHECK(sh1106_i2c_init());
	ESP_ERROR_CHECK(sh1106_init());

	while (true) {
		run_graphics_tests();
	}
}