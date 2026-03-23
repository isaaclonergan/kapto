// main.c - ESP32-S3 OV3660 UVC webcam (QVGA MJPEG)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "usb_device_uvc.h"
#include "usb_descriptors.c"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "esp_heap_caps.h"

static const char *TAG = "uvc_cam";

// Camera pins (OV3660 / your wiring)
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

// Resolution and buffer sizes (match descriptors!)
#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720
#define FRAME_RATE 30
#define UVC_BUF_SIZE (400 * 1024) // 64KB, should be > max JPEG size

// XCLK: many modules prefer 20 MHz; try 20MHz first
#define XCLK_FREQ_HZ 20000000

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = XCLK_FREQ_HZ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 6,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
		.fb_location = CAMERA_FB_IN_PSRAM
};

// UVC DMA buffer (global so callbacks can access)
static uint8_t *uvc_buffer = NULL;

// static UVC fb used for returning a pointer to uvc_buffer
static uvc_fb_t static_uvc_fb;
static uint64_t frame_count = 0;


// Indicator pin (optional)
#define CAMERA_USAGE_PIN GPIO_NUM_14
#define INDICATOR_PIN GPIO_NUM_2

// ---- UVC callbacks ----
static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int fps, void *cb_ctx) {
    (void)cb_ctx;
    ESP_LOGI(TAG, "UVC start requested: fmt=%d %dx%d @%d", format, width, height, fps);
    gpio_set_level(CAMERA_USAGE_PIN, 1);
    return ESP_OK;
}

static void camera_stop_cb(void *cb_ctx) {
    (void)cb_ctx;
    ESP_LOGI(TAG, "UVC stop requested");
    //gpio_set_level(CAMERA_USAGE_PIN, 0);
}

static uvc_fb_t* camera_fb_get_cb(void *cb_ctx) {
    (void)cb_ctx;

    camera_fb_t *fb_cam = esp_camera_fb_get();
    if (!fb_cam) {
        ESP_LOGW(TAG, "esp_camera_fb_get returned NULL");
        return NULL;
    }

    // Only handle JPEG frames (we configured PIXFORMAT_JPEG)
    if (fb_cam->format != PIXFORMAT_JPEG) {
        ESP_LOGW(TAG, "camera returned non-JPEG format: %d", fb_cam->format);
        esp_camera_fb_return(fb_cam);
        return NULL;
    }

    // Sanity: length must fit into uvc buffer
    if (fb_cam->len > UVC_BUF_SIZE) {
        ESP_LOGE(TAG, "frame %d bytes > UVC_BUF_SIZE %d, dropping", fb_cam->len, UVC_BUF_SIZE);
        esp_camera_fb_return(fb_cam);
        return NULL;
    }

    // Copy JPEG bytes into DMA-able buffer (fast memcpy)
    memcpy(uvc_buffer, fb_cam->buf, fb_cam->len);

    // Fill out the static uvc_fb that points at uvc_buffer
    static_uvc_fb.buf = uvc_buffer;
    static_uvc_fb.len = fb_cam->len;
    static_uvc_fb.width = fb_cam->width;
    static_uvc_fb.height = fb_cam->height;
    static_uvc_fb.format = fb_cam->format;
		static_uvc_fb.timestamp = fb_cam->timestamp;

    // Return camera's internal buffer to the driver immediately!
    esp_camera_fb_return(fb_cam);

    // Return pointer to our stable DMA buffer for UVC to send
    return &static_uvc_fb;
}

// Espressif UVC doesn't require you to free camera FB here because we already returned it.
// Keep this as no-op (safe).
static void camera_fb_return_cb(uvc_fb_t* fb, void* cb_ctx) {
    (void)fb;
    (void)cb_ctx;
    // No action required: uvc_buffer is owned by us.
}

// ----------------------

void app_main(void) {
    esp_err_t ret;

    // configure indicator pin
    gpio_set_direction(CAMERA_USAGE_PIN, GPIO_MODE_OUTPUT);
		gpio_set_direction(INDICATOR_PIN, GPIO_MODE_OUTPUT);
   
		gpio_set_level(INDICATOR_PIN, 1);

		ESP_LOGI(TAG, "Initializing camera...");
    ret = esp_camera_init(&camera_config);
		ret = ESP_OK;
		if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Small OV3660 tuning - necessary on many modules
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_hmirror(s, 1);
        s->set_vflip(s, 1);
        s->set_brightness(s, 0);
        s->set_saturation(s, 0);
        s->set_sharpness(s, 0);
        ESP_LOGI(TAG, "Sensor configured: id=%d", s->id);
    } else {
        ESP_LOGW(TAG, "sensor_t is NULL");
    }

    // Allocate DMA-capable UVC buffer (MUST be DMA-capable for USB)
		uvc_buffer = (uint8_t*)malloc(UVC_BUF_SIZE);
    if (!uvc_buffer) {
        return;
    }

    // Configure UVC device
    uvc_device_config_t uvc_cfg = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = UVC_BUF_SIZE,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = NULL
    };

    uvc_device_config(0, &uvc_cfg);
    uvc_device_init();
		gpio_set_level(INDICATOR_PIN, 1);

    // Main loop - keep alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

