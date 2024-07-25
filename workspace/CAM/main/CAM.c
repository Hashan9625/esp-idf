#include <stdio.h>
#include <string.h>
#include "esp_system.h"
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
#include "spi_flash_mmap.h"
#include <nvs_flash.h>

// Wi-Fi credentials
#define WIFI_SSID "Room"
#define WIFI_PASS "AspireE15"

// Camera configuration
#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static const char *TAG = "CameraApp";

void wifi_init_sta(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
httpd_handle_t start_webserver(void);
esp_err_t file_get_handler(httpd_req_t *req);
void takePhoto(void);

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
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
    .xclk_freq_hz = 15000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_GRAYSCALE,
    .frame_size = FRAMESIZE_96X96,
    .jpeg_quality = 10,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    //.grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

void app_main(void)
{
    size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_size > 0)
    {
        ESP_LOGI("PSRAM", "PSRAM is available: %d bytes", psram_size);
    }
    else
    {
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

    esp_err_t result = esp_vfs_spiffs_register(&spiffs_conf);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load file (%s)", esp_err_to_name(result));
        return;
    }

    // Open the image file to check if it exists
    FILE *f = fopen("/storage/dv.jpg", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to load dv.jpg");
    }
    else
    {
        fclose(f);
    }

    if (ESP_OK != init_camera())
    {
        ESP_LOGE(TAG, "Camera init failed ");
        return;
    }

    // Initialize Wi-Fi
    wifi_init_sta();

    takePhoto();
}

void takePhoto(void)
{
    // Capture an image and save to SPIFFS
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic)
    {
        ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);

        FILE *file = fopen("/storage/dv.pgm", "wb");
        if (file)
        {
            // Write the PGM header
            fprintf(file, "P5\n%d %d\n255\n", 96, 96);
            fwrite(pic->buf, 1, pic->len, file);
            fclose(file);
            ESP_LOGI(TAG, "Image saved to /storage/dv.pgm");
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
}


// HTTP GET handler to serve the image file
esp_err_t file_get_handler(httpd_req_t *req)
{
    takePhoto();

    FILE *f = fopen("/storage/dv.pgm", "r");
    if (f == NULL)
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set the content type to PGM
    httpd_resp_set_type(req, "image/x-portable-graymap");

    // Write the PGM header
    char header[32];
    int header_len = snprintf(header, sizeof(header), "P5\n%d %d\n255\n", 96, 96);
    httpd_resp_send_chunk(req, header, header_len);

    // Read and send the file content in chunks
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(f);

    // Signal the end of the response
    httpd_resp_send_chunk(req, NULL, 0);
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
            .uri = "/dv.jpg",
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