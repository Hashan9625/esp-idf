#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define I2C_MASTER_SCL_IO 22           /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 21           /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0       /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0    /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0    /*!< I2C master doesn't need buffer */
#define ESP_SLAVE_ADDR 0x28            /*!< ESP32 slave address, you can set any 7bit value */

static const char *TAG = "i2c_scanner";

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void i2c_scan(void) {
    int i;
    esp_err_t espRc;
    printf("Starting I2C scan:\n");
    for (i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        espRc = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        if (espRc == ESP_OK) {
            printf("Found device at address: 0x%02x\n", i);
        }
    }
    printf("I2C scan completed.\n");
}

void app_main(void) {
    ESP_ERROR_CHECK(i2c_master_init());
    i2c_scan();
}
