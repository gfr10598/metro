
#ifndef IMU_H
#define IMU_H

#include "LSM6DSV16XSensor.h"

class LSMExtension : public LSM6DSV16XSensor
{
public:
    using LSM6DSV16XSensor::LSM6DSV16XSensor;

    LSM6DSV16XStatusTypeDef FIFO_Get_Data(uint8_t *Data);
    LSM6DSV16XStatusTypeDef FIFO_Get_Tag_And_Data(uint8_t *Data);
    LSM6DSV16XStatusTypeDef Read_FIFO_Data(uint16_t max, void *records, uint16_t *count);
};

LSMExtension init_lsm(int sda, int scl);

#endif // IMU_H
