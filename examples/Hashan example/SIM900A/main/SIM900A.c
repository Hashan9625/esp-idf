#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "string.h"

static const int RX_BUF_SIZE = 1024;
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
#define BUTTON_PIN GPIO_NUM_14 // Button connected to GPIO 14
#define DEBOUNCE_DELAY_MS 50

static uint32_t last_interrupt_time = 0;
QueueHandle_t buttonQueue;

void initUART(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void button_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    if ((current_time - last_interrupt_time) > DEBOUNCE_DELAY_MS) {
        xQueueSendFromISR(buttonQueue, &gpio_num, NULL);
        last_interrupt_time = current_time;
    }
}


void initGPIO(void) {
    gpio_config_t io_conf;
    // Configure button pin as input
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Trigger on the rising edge
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Disable pull-down
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;      // Enable pull-up
    gpio_config(&io_conf);

    // Create a queue to handle button presses
    buttonQueue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, (void *)BUTTON_PIN);
}



int sendData(const char *logName, const char *data) {
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes: %s", txBytes, data);
    return txBytes;
}

int sendByte(const char *logName, char byte) {
    const int txBytes = uart_write_bytes(UART_NUM_1, &byte, 1);
    ESP_LOGI(logName, "Wrote 1 byte: 0x%02X", byte);
    return txBytes;
}

static void tx_task(void *arg) {
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    sendData(TX_TASK_TAG, "AT+CMGF=1\r\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    sendData(TX_TASK_TAG, "AT+CMGS=\"0711762975\"\r\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    sendData(TX_TASK_TAG, "Hello world\r\n");
    vTaskDelay(100 / portTICK_PERIOD_MS);

    sendByte(TX_TASK_TAG, (char)26); // Ctrl+Z to send the message
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static void rx_task(void *arg) {
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

static void button_task(void *arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(buttonQueue, &io_num, portMAX_DELAY)) {
            ESP_LOGI("BUTTON_TASK", "Button pressed, sending SMS...");
            tx_task(NULL);
        }

         // Get the current number of items in the queue
        UBaseType_t itemsInQueue = uxQueueMessagesWaiting(buttonQueue);
        ESP_LOGI("BUTTON_TASK", "Number of items in buttonQueue: %d", itemsInQueue);
    }
}

void app_main(void) {
    initUART();
    initGPIO();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(button_task, "button_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
}
