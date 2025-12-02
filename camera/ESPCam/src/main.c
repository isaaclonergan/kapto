#include "tusb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "usb_device_uvc.h"
#include "usb_descriptors.h"
#include "usb_descriptors.c"  // Keep this as is in your working project
#include <string.h>
#include <assert.h>
#include "driver/gpio.h"  // For GPIO control
													//
													//
#include "red_jpeg.h"
static int frame_count = 0;

#define TAG "UVC_CAM"
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define FRAME_RATE 15
#define UVC_BUF_SIZE (64 * 1024)  // 64 KB

static uint8_t *uvc_buf;

// Configure IO14 for camera usage detection
#define CAMERA_USAGE_PIN GPIO_NUM_14

// Callback when the host starts the camera
static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int fps, void *cb_ctx) {
    
    // Turn on IO14 to indicate camera usage
    gpio_set_level(CAMERA_USAGE_PIN, 1);

    return ESP_OK;
}

// Callback when the host stops the camera
static void camera_stop_cb(void *cb_ctx) {
    
    // Turn off IO14 to indicate camera is no longer in use
    gpio_set_level(CAMERA_USAGE_PIN, 0);
}

// Callback to get a frame
static uvc_fb_t* camera_fb_get_cb(void *cb_ctx) {
    static uvc_fb_t fb;
    frame_count++;

		vTaskDelay(pdMS_TO_TICKS(66));

    // Copy JPEG into UVC buffer
    size_t len = sizeof(imag_jpg);
    if (len > UVC_BUF_SIZE) len = UVC_BUF_SIZE; // truncate if too big
    memcpy(uvc_buf, imag_jpg, len);

    fb.buf = uvc_buf;
    fb.len = len;
    fb.width = FRAME_WIDTH;
    fb.height = FRAME_HEIGHT;
    fb.format = UVC_FORMAT_JPEG;


    return &fb;
}

// Callback when frame buffer is returned (not used here)
static void camera_fb_return_cb(uvc_fb_t* fb, void* cb_ctx) {
    (void)fb;
    (void)cb_ctx;
}

void app_main(void) {

    // Configure IO14 as output
    gpio_set_direction(CAMERA_USAGE_PIN, GPIO_MODE_OUTPUT);

    // Allocate UVC buffer
    uvc_buf = heap_caps_malloc(UVC_BUF_SIZE, MALLOC_CAP_DEFAULT);
    assert(uvc_buf);

    // Configure UVC device
    uvc_device_config_t config = {
        .uvc_buffer = uvc_buf,
        .uvc_buffer_size = UVC_BUF_SIZE,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = NULL
    };

    ESP_ERROR_CHECK(uvc_device_config(0, &config));
    ESP_ERROR_CHECK(uvc_device_init());


    // Keep feeding frames continuously
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(66));
    }
}

