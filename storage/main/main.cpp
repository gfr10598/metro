/* Power management hacking
1. When we enter light sleep, the USB serial connection drops. This is expected behavior.
2. GPIOs are generally disabled during light sleep, except for those configured with
   gpio_hold_en().  The built-in LED is on GPIO13, so we enable hold on it.
3. The neopixel is ill-behaved during light sleep.  The trace can be cut to reduce power consumption.
4. light sleep requires CONFIG_FREERTOS_USE_TICKLESS_IDLE to be enabled.

5. About 500uA is saved when sd card is removed.  Look into flash power hacks.
   together with 100ms vTaskDelay, this gets power down to about 1500uA.
   This still leaves more than 1mA unaccounted for.

Future:
https://esp32.com/viewtopic.php?t=32177 - adc power savings

*/

#include <stdio.h>
#include <string>

#include <rom/ets_sys.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_sleep.h>

#include "esp_event.h"
#include "esp_netif.h"

#include "nvs_flash.h"
#include "i2c_bus.h"
#include "max17048.h"
#include <esp_pm.h>
#include "driver/gpio.h"
#include "driver/sdspi_host.h"

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "LSM6DSV16XSensor.h"
#include "LSMextension.h"

#define I2C_MASTER_SCL_IO 48          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 47          /*!< gpio number for I2C master data  */
#define MAX17048_I2CADDR_DEFAULT 0x36 /*!< I2C address for MAX17048 */

i2c_bus_handle_t i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 400 * 1000},
        .clk_flags = 0,

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
    // printf("Battery Voltage: %.3f V  Percent: %.3f %%\n", voltage, percent);
}

#define FIFO_SAMPLE_THRESHOLD 20
#define FLASH_BUFF_LEN 8192
// If we send the raw data to the other processor for output to WiFi,
// we can likely keep up with 3840 samples/sec.
#define SENSOR_ODR 1920

LSM6DSV16XSensor init_lsm()
{
    // Initialize i2c.
    // We need to read roughly 7*2*2khz = 28k bytes per second from the LSM6DSV16X.
    // Because we are reading many bytes at a time, 1MHz I2C can provide perhaps
    // 100k bytes/sec.  So we will be running around 30% duty cycle just reading the data.
    Wire.begin(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    Wire.setClock(400000);
    printf("I2C initialized\n");
    LSMExtension LSM(&Wire);
    printf("LSM (extension) created\n");
    for (int i = 0; i < 10; i++)
    {

        gpio_set_level(GPIO_NUM_13, 0);
        //         digitalWrite(GPIO_NUM_13, LOW);  This is arduino, which requires different init.
        delay(100);
        gpio_set_level(GPIO_NUM_13, 1);
        // digitalWrite(GPIO_NUM_13, HIGH);
        delay(100);
    }

    if (LSM6DSV16X_OK != LSM.begin())
    {
        printf("LSM.begin() Error\n");
    }
    int32_t status = 0;

    // TODO - add LP mode stuff to sense whether bell is moving.
    // status |= Enable_Gravity_Vector();
    // status |= Enable_Gyroscope_Bias();
    // status |= Enable_Rotation_Vector();
    // status |= Set_SFLP_Batch(true, true, true);

    // The gyroscopes have 16 bit signed range, so +/- 32768 counts.
    // At 2000 dps, this give 16.384 LSB/dps, or about 61 dps per count.
    // The 2024 data is collected with FS = 1000 dps.
    // The potential error of 1/32 of a degree per second is not actually observed,
    // because noise in the signal means that quantization error is distributed randomly.
    // So, we expect jitter of about 1/32/sqrt(2000) per root hz.  So, at 1 hz, the jitter
    // is reduced to about 1/1500 degree, which is much smaller than the other sources of error.

    // We should probably be just fine using FS=2000, which gives more headroom for shocks.

    status |= LSM.Set_G_FS(1000); // Need minimum of 600 dps.
    status |= LSM.Set_X_FS(16);   // To handle large impulses from clapper.
    status |= LSM.Set_X_ODR(SENSOR_ODR);
    status |= LSM.Set_G_ODR(SENSOR_ODR);
    status |= LSM.Set_Temp_ODR(LSM6DSV16X_TEMP_BATCHED_AT_1Hz875);

    // Set FIFO to timestamp data at 20 Hz
    status |= LSM.FIFO_Enable_Timestamp();
    status |= LSM.FIFO_Set_Timestamp_Decimation(LSM6DSV16X_TMSTMP_DEC_32);
    status |= LSM.FIFO_Set_Mode(LSM6DSV16X_BYPASS_MODE);

    // Configure FIFO BDR for acc and gyro
    status |= LSM.FIFO_Set_X_BDR(SENSOR_ODR);
    status |= LSM.FIFO_Set_G_BDR(SENSOR_ODR);

    // Set FIFO in Continuous mode
    status |= LSM.FIFO_Set_Mode(LSM6DSV16X_STREAM_MODE);

    status |= LSM.Enable_G();
    status |= LSM.Enable_X();

    if (status != LSM6DSV16X_OK)
    {
        printf("LSM6DSV16X Sensor failed to configure FIFO\n");
        while (1)
            ;
    }
    else
    {
        printf("LSM enabled\n");
        delay(3); // Should allow about 12 samples.
        uint16_t samples = 0;
        LSM.FIFO_Get_Num_Samples(&samples);
        uint8_t data[16 * 7];
        uint16_t samples_read = 0;
        LSM.Read_FIFO_Data(16, &data[0], &samples_read);
        printf("%d Samples available  %d Samples read\n", samples, samples_read);
    }

    return LSM;
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
void speed_test(int iterations)
{
    int start = esp_timer_get_time();
    for (volatile int i = 0; i < iterations;)
        i = i + 1;

    int end = esp_timer_get_time();
    printf("Speed test of %d iterations took %d us\n", iterations, end - start);
}

bool init_SD()
{
    int SD_CS = GPIO_NUM_45;
    int SCK = 39;
    int MISO = 21;
    int MOSI = 42;

    SPI.begin(SCK, MISO, MOSI, SD_CS);
    SPI.setDataMode(SPI_MODE0);

    if (!SD.begin(SD_CS)) // GPIO45 is CS
    {
        printf("Card Mount Failed\n");
        return false;
    }

    return true;
}
uint8_t data[8192] = {0}; // Not on the stack!!

void try_writing(File f)
{
    auto start = millis();
    // The write takes 11 msec, but does not actually write until close()
    // It scales roughly with size.  8k takes 20 msec.
    f.write(&data[0], 4096);
    yield();
    printf("Wrote 4096 bytes in %lu ms\n", millis() - start);
}

#define BLINK_GPIO GPIO_NUM_13

extern "C" void app_main(void)
{
    printf("Compiled at %s %s\n", __DATE__, __TIME__);
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_ERROR_CHECK(nvs_flash_init());
    init_lsm();

    // When false, light sleep is disabled, and power consumption is 16 mA
    // When true, light sleep is enabled, and power consumption is 4 mA
    pm_config(false);

    if (!init_SD())
    {
        printf("SD initialization failed\n");
        vTaskSuspend(NULL);
    }
    else
    {
        printf("SD initialization succeeded\n");
    }

    File f = SD.open("/testfile.bin", FILE_WRITE);
    if (!f)
    {
        printf("Failed to open file for writing\n");
        vTaskSuspend(NULL);
    }
    printf("File opened for writing\n");

    i2c_bus_handle_t i2c_bus = i2c_init();
    // check_battery(i2c_bus);

    // wifi_http();

    // esp_task_wdt_add(NULL);
    // gpio_reset_pin(GPIO_NUM_47); // SDA
    // gpio_reset_pin(GPIO_NUM_48); // SCL
    // gpio_set_direction(GPIO_NUM_47, GPIO_MODE_INPUT);
    // gpio_set_direction(GPIO_NUM_48, GPIO_MODE_INPUT);

    for (int i = 0;; i++)
    {
        // This pegs the power consumption >>30 mA
        // speed_test(500000);
        // check_battery(i2c_bus);
        // toggle the LED
        gpio_set_level(BLINK_GPIO, i % 3 > 1 ? 1 : 0);
        // gpio_hold_en(BLINK_GPIO);

        for (int j = 0; j < 10; j++)
        {
            // This results in about 40kB/sec, which is about what we need.
            // This results in very spiky current consumption, averaging about 20mA.
            // This does not even include an i2c activity, and would result in about
            // 100 hours of battery life.
            // (If we disable light sleep, it goes to about 30mA average.)
            // Since the power consumption of the original USB design is about 30mA,
            // we can assume that the I2C activity has little impact, aside from
            // keeping the CPU from entering light sleep.
            printf("%d ", j);
            // try_writing(f);
            // check_battery(i2c_bus);

            // for (int k = 0; k < 10; k++) {
            //     // This runs the i2c bus 100 times per second, which isn't a lot.
            //     // But, it bumps up the power consumption from 2 to 5mA.
            //     check_battery(i2c_bus);
            // }
            // 100ms allows lower power - 2mA continuous, instead
            // of 4mA with short drops to 2mA
            // But 10ms leads to higher power - 6mA continuous
            vTaskDelay(90 / portTICK_PERIOD_MS);
        }
        // vTaskDelay(1000 / portTICK_PERIOD_MS); // for watchdog
        // gpio_hold_dis(BLINK_GPIO);
        // esp_task_wdt_reset();
        if (i % 10 == 0)
        {
            auto start = millis();
            // Close takes about 9-10 msec to flush the data, apparently regardless
            // of the amount that has been written.
            // The average power spikes briefly to about 25mA (on the analog meter).
            f.flush();
            printf("Flush data, which took %lu ms\n", millis() - start);

            // // The open takes about 10 msec
            // f = SD.open("/testfile.bin", FILE_WRITE);
            // if (!f)
            // {
            //     printf("Failed to open file for writing\n");
            //     vTaskSuspend(NULL);
            // }
        }
        printf("Loop %d complete\n", i);
    }
}