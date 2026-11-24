#include <math.h>

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "pupil.h"

#define LCD_HOST SPI3_HOST
#define PIN_SCLK 18
#define PIN_MOSI 23
#define PIN_DC 21
#define PIN_RST 22
#define EYE1_CS 5
#define EYE2_CS 25

#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define DRAW_BUF_PIXELS (180 * 180)
#define LCD_PCLK_HZ (40 * 1000 * 1000)

static const char *TAG = "magikarp";
static SemaphoreHandle_t s_flush_done;

static bool IRAM_ATTR flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_flush_done, &wake);
    return wake == pdTRUE;
}

static void wait_flush(void)
{
    xSemaphoreTake(s_flush_done, portMAX_DELAY);
}

static uint16_t wire565(uint16_t color)
{
    return (uint16_t)((color << 8) | (color >> 8));
}

static int clamp_int(int value, int low, int high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void pupil_rect(int diameter, float offset_x, float offset_y, int *x0, int *y0, int *x1, int *y1)
{
    const float cx = 120.0f + offset_x;
    const float cy = 120.0f + offset_y;
    const float rad = diameter * 0.5f;

    *x0 = clamp_int((int)floorf(cx - rad - 0.5f) - 1, 0, LCD_WIDTH);
    *y0 = clamp_int((int)floorf(cy - rad - 0.5f) - 1, 0, LCD_HEIGHT);
    *x1 = clamp_int((int)ceilf(cx + rad + 0.5f) + 1, 0, LCD_WIDTH);
    *y1 = clamp_int((int)ceilf(cy + rad + 0.5f) + 1, 0, LCD_HEIGHT);
}

static void draw_region(esp_lcd_panel_handle_t panel, uint16_t *buf, int x0, int y0, int x1, int y1,
                        int diameter, float offset_x, float offset_y, int mirror)
{
    const int width = x1 - x0;
    const int height = y1 - y0;
    if (width <= 0 || height <= 0) {
        return;
    }

    int y = y0;
    while (y < y1) {
        int rows = (y1 - y);
        if (width * rows > DRAW_BUF_PIXELS) {
            rows = DRAW_BUF_PIXELS / width;
        }
        if (rows < 1) {
            rows = 1;
        }

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < width; col++) {
                const uint16_t color = pupil_scene_color(x0 + col, y + row, diameter, offset_x, offset_y, mirror);
                buf[row * width + col] = wire565(color);
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, x0, y, x1, y + rows, buf));
        wait_flush();
        y += rows;
    }
}

static void fill_sclera(esp_lcd_panel_handle_t panel, uint16_t *buf)
{
    const uint16_t white = wire565(PUPIL_COLOR_SCLERA);
    const int rows = DRAW_BUF_PIXELS / LCD_WIDTH;

    for (int i = 0; i < rows * LCD_WIDTH; i++) {
        buf[i] = white;
    }
    for (int y = 0; y < LCD_HEIGHT; y += rows) {
        int y1 = y + rows;
        if (y1 > LCD_HEIGHT) {
            y1 = LCD_HEIGHT;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y1, buf));
        wait_flush();
    }
}

static void start_bus(void)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DRAW_BUF_PIXELS * (int)sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
}

static esp_lcd_panel_handle_t start_panel(int cs, int reset_gpio)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = cs,
        .dc_gpio_num = PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = reset_gpio,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    /* 90 degrees counter-clockwise from the panel's native orientation. */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return panel;
}

typedef struct {
    esp_lcd_panel_handle_t panel;
    int mirror;
    int shown_diameter;
    int shown_x;
    int shown_y;
    int prev_x0;
    int prev_y0;
    int prev_x1;
    int prev_y1;
} eye_t;

static void refresh_eye(eye_t *eye, uint16_t *buf, const pupil_pose_t *pose)
{
    const int raw_x = (int)lroundf(pose->offset_x);
    const int raw_y = (int)lroundf(pose->offset_y);
    const int offset_x = eye->mirror ? -raw_x : raw_x;

    if (pose->diameter == eye->shown_diameter && offset_x == eye->shown_x && raw_y == eye->shown_y) {
        return;
    }

    int x0, y0, x1, y1;
    pupil_rect(pose->diameter, offset_x, raw_y, &x0, &y0, &x1, &y1);
    if (eye->shown_diameter >= 0) {
        x0 = x0 < eye->prev_x0 ? x0 : eye->prev_x0;
        y0 = y0 < eye->prev_y0 ? y0 : eye->prev_y0;
        x1 = x1 > eye->prev_x1 ? x1 : eye->prev_x1;
        y1 = y1 > eye->prev_y1 ? y1 : eye->prev_y1;
    }
    draw_region(eye->panel, buf, x0, y0, x1, y1, pose->diameter, raw_x, raw_y, eye->mirror);
    pupil_rect(pose->diameter, offset_x, raw_y, &eye->prev_x0, &eye->prev_y0, &eye->prev_x1, &eye->prev_y1);
    eye->shown_diameter = pose->diameter;
    eye->shown_x = offset_x;
    eye->shown_y = raw_y;
}

void app_main(void)
{
    s_flush_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(s_flush_done ? ESP_OK : ESP_ERR_NO_MEM);

    start_bus();
    eye_t eyes[2] = {
        {
            .panel = start_panel(EYE1_CS, PIN_RST),
            .mirror = 0,
            .shown_diameter = -1,
        },
        {
            /* Shared RES is already high. A second GPIO reset would clear eye 1. */
            .panel = start_panel(EYE2_CS, -1),
            .mirror = 1,
            .shown_diameter = -1,
        },
    };

    uint16_t *buf = heap_caps_malloc(DRAW_BUF_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(buf ? ESP_OK : ESP_ERR_NO_MEM);
    fill_sclera(eyes[0].panel, buf);
    fill_sclera(eyes[1].panel, buf);

    const int64_t start_us = esp_timer_get_time();
    ESP_LOGI(TAG, "wide eyes started");

    while (1) {
        const int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        pupil_pose_t pose;
        pupil_pose(elapsed_ms, &pose);
        refresh_eye(&eyes[0], buf, &pose);
        refresh_eye(&eyes[1], buf, &pose);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
