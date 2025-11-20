cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "src/LSM6DSV16XSensor.cpp"
         "src/lsm6dsv16x_reg.c"
    INCLUDE_DIRS "src"
    REQUIRES arduino-esp32
)

project(LSM6DSV16XSensor)