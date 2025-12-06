cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "src/IMU.cpp"
    INCLUDE_DIRS "src"
    REQUIRES arduino-esp32 LSM6DSV16X
)

project(imu)