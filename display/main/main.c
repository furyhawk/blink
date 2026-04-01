#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_PORT                I2C_NUM_0
#define I2C_SDA_GPIO            9   /* ESP32-S3-DevKitC-1: SDA on GPIO 9 */
#define I2C_SCL_GPIO            10   /* ESP32-S3-DevKitC-1: SCL on GPIO 10 */
#define I2C_FREQ_HZ             100000

#define SSD1327_RST_GPIO         -1  /* Set to -1 when reset pin is not connected */

#define SSD1327_I2C_ADDR        0x3C
#define SSD1327_WIDTH           128
#define SSD1327_HEIGHT          128
#define SSD1327_FB_SIZE         (SSD1327_WIDTH * SSD1327_HEIGHT / 2)

static const char *TAG = "ssd1327_demo";
static uint8_t framebuffer[SSD1327_FB_SIZE];

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t i2c_dev;

static esp_err_t i2c_init(void)
{
	i2c_master_bus_config_t bus_cfg = {
		.i2c_port    = I2C_PORT,
		.sda_io_num  = I2C_SDA_GPIO,
		.scl_io_num  = I2C_SCL_GPIO,
		.clk_source  = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &i2c_bus), TAG, "i2c bus init failed");

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address  = SSD1327_I2C_ADDR,
		.scl_speed_hz    = I2C_FREQ_HZ,
	};
	return i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
}

static esp_err_t ssd1327_reset(void)
{
	int rst_gpio = SSD1327_RST_GPIO;
	if (rst_gpio < 0) {
		/* Optional hardware reset line is not connected. */
		vTaskDelay(pdMS_TO_TICKS(10));
		return ESP_OK;
	}

	ESP_RETURN_ON_ERROR(gpio_reset_pin(rst_gpio), TAG, "gpio_reset_pin RST failed");
	ESP_RETURN_ON_ERROR(gpio_set_direction(rst_gpio, GPIO_MODE_OUTPUT), TAG, "gpio_set_direction RST failed");

	gpio_set_level(rst_gpio, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(rst_gpio, 1);
	vTaskDelay(pdMS_TO_TICKS(10));

	return ESP_OK;
}

static esp_err_t ssd1327_write_command(uint8_t cmd)
{
	const uint8_t packet[2] = {0x00, cmd};
	return i2c_master_transmit(i2c_dev, packet, sizeof(packet), 100);
}

static esp_err_t ssd1327_write_commands(const uint8_t *cmds, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		ESP_RETURN_ON_ERROR(ssd1327_write_command(cmds[i]), TAG, "command 0x%02X failed", cmds[i]);
	}
	return ESP_OK;
}

static esp_err_t ssd1327_set_addr_window(void)
{
	const uint8_t cmd_seq[] = {
		0x15, 0x00, 0x7F,
		0x75, 0x00, 0x7F,
	};
	return ssd1327_write_commands(cmd_seq, sizeof(cmd_seq));
}

static esp_err_t ssd1327_send_framebuffer(const uint8_t *buf, size_t len)
{
	ESP_RETURN_ON_ERROR(ssd1327_set_addr_window(), TAG, "set window failed");

	uint8_t packet[17];
	packet[0] = 0x40;

	size_t sent = 0;
	while (sent < len) {
		size_t chunk = len - sent;
		if (chunk > 16) {
			chunk = 16;
		}
		memcpy(&packet[1], &buf[sent], chunk);
		ESP_RETURN_ON_ERROR(
			i2c_master_transmit(i2c_dev, packet, chunk + 1, 100),
			TAG,
			"framebuffer write failed at offset %u",
			(unsigned int)sent
		);
		sent += chunk;
	}

	return ESP_OK;
}

static esp_err_t ssd1327_init(void)
{
	ESP_RETURN_ON_ERROR(ssd1327_reset(), TAG, "reset failed");

	const uint8_t init_seq[] = {
		0xFD, 0x12, /* Command lock */
		0xAE,       /* Display OFF */
		0xA8, 0x5F, /* multiplex ratio: 0x5F * 1/64duty */
		0xA1, 0x00, /* Display Start Line */
		0xA2, 0x00, /* Display Offset */
		0xA0, 0x51, /* remap */
		0xAB, 0x01, /* Enable Internal VDD */
		0x81, 0x53, /* Contrast */
		0xB1, 0x51, /* Phase Length */
		0xB3, 0x01, /* Front Clock Divider */
		0xB9,       /* Linear LUT */
		0xBC, 0x08, /* Precharge Voltage */
		0xBE, 0x07, /* VCOMH Voltage */
		           /* 0xD1, 0x82, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, */
		0xB6, 0x01, /* Second Precharge Period */
		0xD5, 0x62, /* Enable 2D Scroll */
		// 0xB5, 0x03, /* Enable GPIO */
		0xA4,       /* Display ON */
	};

	return ssd1327_write_commands(init_seq, sizeof(init_seq));
}

static void ssd1327_fill(uint8_t gray)
{
	uint8_t packed = (uint8_t)((gray << 4) | gray);
	memset(framebuffer, packed, sizeof(framebuffer));
}

static void ssd1327_draw_pixel(int x, int y, uint8_t gray)
{
	if (x < 0 || x >= SSD1327_WIDTH || y < 0 || y >= SSD1327_HEIGHT) {
		return;
	}

	gray &= 0x0F;
	size_t idx = (size_t)y * (SSD1327_WIDTH / 2) + (size_t)x / 2;
	if ((x & 1) == 0) {
		framebuffer[idx] = (uint8_t)((framebuffer[idx] & 0x0F) | (gray << 4));
	} else {
		framebuffer[idx] = (uint8_t)((framebuffer[idx] & 0xF0) | gray);
	}
}

static void ssd1327_make_test_pattern(void)
{
	ssd1327_fill(0x0);

	for (int y = 0; y < SSD1327_HEIGHT; y++) {
		for (int x = 0; x < SSD1327_WIDTH; x++) {
			uint8_t gray = (uint8_t)((x * 15) / (SSD1327_WIDTH - 1));

			if ((x / 16 + y / 16) % 2 == 0) {
				gray = (uint8_t)(15 - gray);
			}

			ssd1327_draw_pixel(x, y, gray);
		}
	}
}

/* Simple graphics library - replaceable with LVGL later */

/* Draw a horizontal line */
static void draw_hline(int x1, int x2, int y, uint8_t gray)
{
	if (y < 0 || y >= SSD1327_HEIGHT) return;
	if (x1 > x2) {
		int tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	x1 = (x1 < 0) ? 0 : x1;
	x2 = (x2 >= SSD1327_WIDTH) ? (SSD1327_WIDTH - 1) : x2;

	for (int x = x1; x <= x2; x++) {
		ssd1327_draw_pixel(x, y, gray);
	}
}

/* Draw a rectangle */
static void draw_rect(int x1, int y1, int x2, int y2, uint8_t gray, int filled)
{
	if (x1 > x2) {
		int tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	if (y1 > y2) {
		int tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	if (filled) {
		for (int y = y1; y <= y2; y++) {
			draw_hline(x1, x2, y, gray);
		}
	} else {
		draw_hline(x1, x2, y1, gray);
		draw_hline(x1, x2, y2, gray);
		for (int y = y1 + 1; y < y2; y++) {
			ssd1327_draw_pixel(x1, y, gray);
			ssd1327_draw_pixel(x2, y, gray);
		}
	}
}

/* Initialize graphics (stub for LVGL integration point) */
static esp_err_t graphics_init(void)
{
	/* TODO: Initialize LVGL here when component is available */
	ESP_LOGI(TAG, "Graphics initialized");
	return ESP_OK;
}

/* Create demo UI with simple graphics */
static void create_ui(void)
{
	ssd1327_fill(0x0);

	/* Draw title box */
	draw_rect(2, 2, 125, 40, 0xF, 0);
	draw_rect(5, 45, 122, 80, 0xF, 0);
	draw_rect(10, 85, 118, 125, 0x8, 1);

	ssd1327_send_framebuffer(framebuffer, sizeof(framebuffer));
}

void app_main(void)
{
	ESP_ERROR_CHECK(i2c_init());
	ESP_ERROR_CHECK(ssd1327_init());
	ESP_ERROR_CHECK(graphics_init());

	create_ui();

	ESP_LOGI(TAG, "SSD1327 graphics demo started");

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}