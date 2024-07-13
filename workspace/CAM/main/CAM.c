#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_spiffs.h"

// Wi-Fi credentials
#define WIFI_SSID "Room"
#define WIFI_PASS "AspireE15"

// Camera configuration
#define CAMERA_PIXEL_FORMAT PIXFORMAT_JPEG
#define CAMERA_FRAME_SIZE FRAMESIZE_VGA
#define CAMERA_FB_COUNT 1

static const char *TAG = "CameraApp";

void wifi_init_sta(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
httpd_handle_t start_webserver(void);
esp_err_t file_get_handler(httpd_req_t *req);

void app_main(void)
{
    size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_size > 0) {
        ESP_LOGI("PSRAM", "PSRAM is available: %d bytes", psram_size);
    } else {
        ESP_LOGE("PSRAM", "No PSRAM available");
    }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/storage",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    // Initialize the camera
    camera_config_t camera_config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = 0,
        .pin_sscb_sda = 26,
        .pin_sscb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 21,
        .pin_d2 = 19,
        .pin_d1 = 18,
        .pin_d0 = 5,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = CAMERA_PIXEL_FORMAT,
        .frame_size = CAMERA_FRAME_SIZE,
        .jpeg_quality = 12,
        .fb_count = CAMERA_FB_COUNT};

    ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return;
    }

    // Capture an image and save to SPIFFS
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic)
    {
        FILE *file = fopen("/storage/image.jpg", "wb");
        if (file)
        {
            fwrite(pic->buf, 1, pic->len, file);
            fclose(file);
            ESP_LOGI(TAG, "Image saved to /storage/image.jpg");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to open file for writing");
        }
        esp_camera_fb_return(pic);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to capture image");
    }

    // Initialize Wi-Fi
    wifi_init_sta();
}

// HTTP GET handler to serve the image
esp_err_t file_get_handler(httpd_req_t *req)
{
    FILE *file = fopen("/storage/image.jpg", "rb");
    if (file == NULL)
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char line[64];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(file);
    httpd_resp_sendstr_chunk(req, NULL); // Signal the end of the response
    return ESP_OK;
}

// Function to start the HTTP server
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t file_uri = {
            .uri = "/image.jpg",
            .method = HTTP_GET,
            .handler = file_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &file_uri);
    }
    return server;
}

// Wi-Fi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        start_webserver();
    }
}

// Function to initialize Wi-Fi
void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}