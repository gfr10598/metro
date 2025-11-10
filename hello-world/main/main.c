/* Power management hacking
1. When we enter light sleep, the USB serial connection drops. This is expected behavior.
2. The red LED power domain is turned off during light sleep, so the built-in LED will not light up.
   It will blink for a moment during IDLE (~ 3 msec), which is enough to verify functionality.
3. The neopixel is ill-behaved during light sleep.  The trace can be cut to reduce power consumption.
4. light sleep requires CONFIG_FREERTOS_USE_TICKLESS_IDLE to be enabled.


*/

#include <stdio.h>
#include <rom/ets_sys.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_sleep.h>

#include <esp_http_server.h>
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "i2c_bus.h"
#include "max17048.h"
#include <esp_pm.h>

#include "neopixel.h"

#define I2C_MASTER_SCL_IO 48          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 47          /*!< gpio number for I2C master data  */
#define MAX17048_I2CADDR_DEFAULT 0x36 /*!< I2C address for MAX17048 */

i2c_bus_handle_t i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    i2c_bus_handle_t bus = i2c_bus_create(I2C_NUM_0, &conf);
    return bus;
}

void check_battery(i2c_bus_handle_t i2c_bus)
{
    max17048_handle_t max17048 = NULL;

    if (i2c_bus == NULL)
    {
        printf("Failed to create I2C bus\n");
        return;
    }

    // Step2: Init max17048
    max17048 = max17048_create(i2c_bus, MAX17048_I2CADDR_DEFAULT);
    if (max17048 == NULL)
    {
        printf("Failed to create max17048 handle\n");
        return;
    }

    // Step2: Get voltage and battery percentage
    float voltage = 0, percent = 0;
    esp_err_t ret = max17048_get_cell_voltage(max17048, &voltage);
    if (ret != ESP_OK)
    {
        printf("Failed to get battery voltage\n");
        return;
    }
    ret = max17048_get_cell_percent(max17048, &percent);
    if (ret != ESP_OK)
    {
        printf("Failed to get battery percent\n");
        return;
    }
    printf("Battery Voltage: %.3f V  Percent: %.3f %%\n", voltage, percent);
}

void pm_config(bool light_sleep_enable)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 10,
        .light_sleep_enable = light_sleep_enable};
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK)
    {
        printf("Failed to configure power management: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = esp_sleep_enable_timer_wakeup(100000); // 100 ms
    if (ret != ESP_OK)
    {
        printf("Failed to enable timer wakeup: %s\n", esp_err_to_name(ret));
        return;
    }
    esp_pm_dump_locks(stdout);
}

// This can't be slow, because FREERTOS holds the max CPU lock.
void speed_test(char *lbl)
{
    int start = esp_timer_get_time();
    for (volatile int i = 0; i < 1000000; i++)
        ;

    int end = esp_timer_get_time();
    printf("%s function took %d us\n", lbl, end - start);
}

void wifi_http(void)
{
    // WiFi initialization code would go here
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Start the httpd server
    ESP_ERROR_CHECK(httpd_start(&server, &config));
}

#define BLINK_GPIO GPIO_NUM_13

void app_main(void)
{

    pm_config(true);

    i2c_bus_handle_t i2c_bus = i2c_init();
    check_battery(i2c_bus);

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // wifi_http();

    esp_task_wdt_add(NULL);
    for (int i = 0;; i++)
    {
        check_battery(i2c_bus);
        // toggle the LED
        // The light sleep causes a blink for about 3 msec before and after if the GPIO is on.
        // So we need two off periods for an absent blink.
        gpio_set_level(BLINK_GPIO, i % 3 > 1 ? 1 : 0);

        // printf("Hello, World! %d   %d\n", i, configTICK_RATE_HZ);
        //  An occasional vTaskDelay is required for the watchdog.
        // printf("Sleeping for 2000 ms...\n");
        // esp_sleep_enable_timer_wakeup(2000000); // 100ms
        // esp_light_sleep_start();
        // printf("Sleeping for 2000 ms...\n");
        // esp_sleep_enable_timer_wakeup(2000000); // 100ms
        // esp_light_sleep_start();

        vTaskDelay(1000 / portTICK_PERIOD_MS); // for watchdog
        esp_task_wdt_reset();
    }
    vTaskSuspend(NULL);
}