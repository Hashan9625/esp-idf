#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h" // Include the header for heap capabilities

static const char *TAG = "mtApp-- ";

void app_main(void)
{
    size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_size > 0)
    {
        ESP_LOGI(TAG, "PSRAM is available: %d bytes", psram_size);
    }
    else
    {
        ESP_LOGE(TAG, "No PSRAM available");
    }
}
