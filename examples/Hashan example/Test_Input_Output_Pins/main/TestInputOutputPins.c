#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#define TAG "Main :"

char isOutPin(char pin)
{
    // Skip GPIO pins 6 to 11 (Flash pins)
    if (pin >= 6 && pin <= 11)
    {
        return 0;
    }
    else if (pin == 24 || pin == 28 || pin == 29 || pin == 30 || pin == 31)
    {
        // skip tested pins
        return 0;
    }

    return 1;
}

void checkOutputPin(void)
{
    int i;

    // Loop to configure valid GPIO pins
    for (i = 0; i <= GPIO_NUM_33; i++)
    {
        // Skip GPIO pins 6 to 11 (Flash pins)
        if (!isOutPin(i))
        {
            continue;
        }

        ESP_LOGI(TAG, "Start configuring pin %d", i);
        gpio_set_direction(i, GPIO_MODE_OUTPUT);
    }

    while (1)
    {
        // Set all valid pins to HIGH
        for (i = 0; i <= GPIO_NUM_33; i++)
        {
            if (!isOutPin(i))
            {
                continue;
            }

            ESP_LOGI(TAG, "Blink pin %d", i);
            gpio_set_level(i, 1);
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        // Set all valid pins to LOW
        for (i = 0; i <= GPIO_NUM_33; i++)
        {
            if (!isOutPin(i))
            {
                continue;
            }

            gpio_set_level(i, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Add delay here as well
    }
}

char isInputPin(char pin)
{
    // Skip GPIO pins 6 to 11 (Flash pins)
    if (pin >= 6 && pin <= 11)
    {
        return 0;
    }
    else if (pin == 24 || pin == 28 || pin == 29 || pin == 30 || pin == 31)
    {
        // skip tested pins
        return 0;
    }

    return 1;
}

void checkInputPinsWithPullUp(void)
{
    int i;
    // Loop to configure valid GPIO pins as input with pull-up resistors
    for (i = 0; i <= GPIO_NUM_39; i++)
    {
        // Skip GPIO pins 6 to 11 (Flash pins)
        if (!isInputPin(i))
        {
            continue;
        }

        ESP_LOGI(TAG, "Configuring pin %d as input with pull-up", i);
        
        // Set pin as input
        gpio_set_direction(i, GPIO_MODE_INPUT);
        
        // Enable internal pull-up resistor
        gpio_set_pull_mode(i, GPIO_PULLUP_ONLY);
    }

    while (1)
    {
        ESP_LOGI(TAG, "\n\nChecking input pin states...\n");

        // Loop to check the state of each input pin
        for (i = 0; i <= GPIO_NUM_39; i++)
        {
            // Skip flash memory pins (6-11)
            if (!isInputPin(i))
            {
                continue;
            }

            // Read the pin state (1 = HIGH, 0 = LOW)
            int pinState = gpio_get_level(i);

            // Log the state of the pin
            ESP_LOGI(TAG, "Pin %d state: %d", i, pinState);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));  // Delay for 500 ms
    }
}

void app_main(void)
{
     checkOutputPin();
    // checkInputPinsWithPullUp();
}
