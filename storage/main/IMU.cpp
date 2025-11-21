#include "IMU.h"

/**
 * @brief  Get the LSM6DSV16X FIFO raw data

 * @param  Data FIFO raw data array [6]
 * @retval 0 in case of success, an error code otherwise
 */
LSM6DSV16XStatusTypeDef LSMExtension::FIFO_Get_Data(uint8_t *Data)
{
    return (LSM6DSV16XStatusTypeDef)lsm6dsv16x_read_reg(&reg_ctx, LSM6DSV16X_FIFO_DATA_OUT_X_L, Data, 6);
}

LSM6DSV16XStatusTypeDef LSMExtension::FIFO_Get_Tag_And_Data(uint8_t *Data)
{
    return (LSM6DSV16XStatusTypeDef)lsm6dsv16x_read_reg(&reg_ctx, LSM6DSV16X_FIFO_DATA_OUT_TAG, Data, 7);
}

/**
 * @brief  Read multiple records from the LSM6DSV16X FIFO
 * @param  count Count of records to read
 * @param  records Array of records
 * @retval 0 in case of success, an error code otherwise
 */
LSM6DSV16XStatusTypeDef LSMExtension::Read_FIFO_Data(uint16_t max, void *records, uint16_t *count)
{
    int status = FIFO_Get_Num_Samples(count);
    if (status != LSM6DSV16X_OK)
        return LSM6DSV16X_ERROR;
    if (*count == 0)
    {
        return LSM6DSV16X_OK;
    }
    if (*count > max)
        *count = max;
    // If we read more than this, i2c doesn't seem to actually read all the data.
    if (*count > 32)
        *count = 32;
    lsm6dsv16x_read_reg(&reg_ctx, LSM6DSV16X_FIFO_DATA_OUT_TAG, (uint8_t *)records, *count * 7);
    return LSM6DSV16X_OK;
}

#define FIFO_SAMPLE_THRESHOLD 20
#define FLASH_BUFF_LEN 8192
// If we send the raw data to the other processor for output to WiFi,
// we can likely keep up with 3840 samples/sec.
#define SENSOR_ODR 1920

LSMExtension init_lsm(int sda, int scl)
{
    // Initialize i2c.
    // We need to read roughly 7*2*2khz = 28k bytes per second from the LSM6DSV16X.
    // Because we are reading many bytes at a time, 1MHz I2C can provide perhaps
    // 100k bytes/sec.  So we will be running around 30% duty cycle just reading the data.
    Wire.begin(sda, scl);
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

    status |= LSM.Enable_Gravity_Vector();
    status |= LSM.Enable_Gyroscope_Bias();
    status |= LSM.Set_SFLP_Batch(false, true, true);
    status |= LSM.Set_SFLP_ODR(LSM6DSV16X_SFLP_15Hz);

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
        uint8_t data[32 * 7];
        uint16_t samples_read = 0;
        LSM.Read_FIFO_Data(32, &data[0], &samples_read);
        printf("%d Samples available  %d Samples read\n", samples, samples_read);
        for (uint16_t i = 0; i < samples_read; i++)
        {
            printf("Record %d: Cnt=0x%02X  Tag=0x%02X Data=%02X %02X %02X %02X %02X %02X\n", i,
                   (data[i * 7 + 0] >> 1) && 0x03,
                   data[i * 7 + 0] >> 3,
                   data[i * 7 + 1], data[i * 7 + 2], data[i * 7 + 3],
                   data[i * 7 + 4], data[i * 7 + 5], data[i * 7 + 6]);
        }
    }

    return LSM;
}
