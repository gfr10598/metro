
#ifndef IMU_H
#define IMU_H

#include "LSM6DSV16XSensor.h"

typedef struct __attribute__((packed)) lsm6dsv16x_fifo_record_t
{
    lsm6dsv16x_fifo_data_out_tag_t tag;
    int16_t data[3];
} lsm6dsv16x_fifo_record_t;

class LSMExtension : public LSM6DSV16XSensor
{
public:
    using LSM6DSV16XSensor::LSM6DSV16XSensor;

    LSMExtension(TwoWire &wire) : LSM6DSV16XSensor(&wire)
    {
        gravity[0] = gravity[1] = gravity[2] = 0;
        gravity_lpf[0] = gravity_lpf[1] = gravity_lpf[2] = 0;
        gyro_bias[0] = gyro_bias[1] = gyro_bias[2] = 0;
    }

    LSM6DSV16XStatusTypeDef FIFO_Get_Data(uint8_t *Data);
    LSM6DSV16XStatusTypeDef FIFO_Get_Tag_And_Data(uint8_t *Data);
    LSM6DSV16XStatusTypeDef Read_FIFO_Data(uint16_t max, lsm6dsv16x_fifo_record_t *records, uint16_t *count);

    LSM6DSV16XStatusTypeDef Fast()
    {
        LSM6DSV16XStatusTypeDef status = Enable_G();
        if (status != LSM6DSV16X_OK)
            return status;
        return Enable_X();
    }

    LSM6DSV16XStatusTypeDef Slow()
    {
        LSM6DSV16XStatusTypeDef status = Disable_G();
        if (status != LSM6DSV16X_OK)
            return status;
        return Disable_X();
    }

    // Returns the sin of the angle between new and old gravity vectors, and updates old vector.
    float cross_gravity(const int16_t *gg)
    {
        float g[3] = {(float)gg[0] / 2048.0f, (float)gg[1] / 2048.0f, (float)gg[2] / 2048.0f};

        // compute the cross product of a new gravity vector with the previous one.
        float cross[3] = {0.0f, 0.0f, 0.0f};
        for (size_t i = 0; i < 3; i++)
        {
            cross[i] += gravity[(i + 1) % 3] * g[(i + 2) % 3] - gravity[(i + 2) % 3] * g[(i + 1) % 3];
        }
        // Since we use FS=16g, gravity vector magnitude should be about 2k
        float cross_magnitude = sqrtf(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

        // Update the stored gravity vector to be the new one.
        gravity[0] = g[0];
        gravity[1] = g[1];
        gravity[2] = g[2];

        return cross_magnitude;
    }

    // Returns the sine of the angle between new gravity vector and the lpf filtered gravity history.
    // We might expect when the bell is lifted off the stay to see several cm of movement, which
    // should translate to a sine of 0.01 or so, which is about 164 counts at SFLP FS=2g.
    // But we compute in floating point, and return the approximate sine value.
    float update_gravity_lpf(const int16_t *gg)
    {
        float g[3] = {(float)gg[0] / 2048.0f, (float)gg[1] / 2048.0f, (float)gg[2] / 2048.0f};

        float cross[3] = {0.0f, 0.0f, 0.0f};
        for (size_t i = 0; i < 3; i++)
        {
            cross[i] += (float)(gravity_lpf[(i + 1) % 3] * g[(i + 2) % 3] - gravity_lpf[(i + 2) % 3] * g[(i + 1) % 3]);
        }
        // Since we use FS=16g, gravity vector magnitude should be about 2k
        float cross_magnitude = sqrtf(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

        // Update the stored gravity lpf vector.
        const float alpha = 0.05f;
        for (size_t i = 0; i < 3; i++)
        {
            gravity_lpf[i] += alpha * (g[i] - gravity_lpf[i]);
        }
        return cross_magnitude;
    }

private:
    float gravity[3];
    float gravity_lpf[3];
    float gyro_bias[3];
};

LSMExtension init_lsm(int sda, int scl);

#endif // IMU_H
