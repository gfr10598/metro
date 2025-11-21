#include "LSMextension.h"

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