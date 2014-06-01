/*
 * STMicroelectronics LPS331AP Utility for Linux
 *
 * Copyright (C) 2014 Tetsuya Kimata <kimata@green-rabbit.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * USAGE:
 * $ make
 * $ ./sense_lps331ap I2C_BUS DEVICE_ADDR_MODE
 *
 * if you use BagleBone Black and use P9_19-20, I2C_BUS is `1'.
 * if you set SA0 to GND, DEVICE_ADDR_MODE is `0', otherwise `1'.
 *
 */

#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define ARRAY_SIZE_OF(array) (sizeof(array)/sizeof((array)[0]))

#define I2C_DEV_ADDR_0      0x5C
#define I2C_DEV_ADDR_1      0x5D

#define WAIT_COUNT          10

typedef enum {
    CMD_WHO_AM_I,
    CMD_CTRL1,
    CMD_CTRL2,
    CMD_PRESS_OUT,
    CMD_TEMP_OUT,
    CMD_WAIT_BOTH
} LPS331AP_COMMAND;

typedef enum {
    REG_WHO_AM_I            = 0x0F,
    REG_CTRL_REG1           = 0x20,
    REG_CTRL_REG2           = 0x21,
    REG_PRESS_OUT_XL        = 0x28,
    REG_PRESS_OUT_L         = 0x29,
    REG_PRESS_OUT_H         = 0x2A,
    REG_TEMP_OUT_L          = 0x2B,
    REG_TEMP_OUT_H          = 0x2C,
    REG_STATUS              = 0x27
} LPS331AP_REG;

typedef enum {
    PD_DOWN                 = 0x0 << 7,
    PD_ACTIVE               = 0x1 << 7
} CTRL_REG1_PD;

typedef enum {
    ODR_OUT_FREQ_ONE_ONE    = 0x0 << 4,
    ODR_OUT_FREQ_1HZ_1HZ    = 0x1 << 4,
    ODR_OUT_FREQ_7HZ_1HZ    = 0x2 << 4,
    ODR_OUT_FREQ_13HZ_1HZ   = 0x3 << 4,
    ODR_OUT_FREQ_25HZ_1HZ   = 0x4 << 4,
    ODR_OUT_FREQ_7HZ_7HZ    = 0x5 << 4,
    ODR_OUT_FREQ_13HZ_13HZ  = 0x6 << 4,
    ODR_OUT_FREQ_25HZ_25HZ  = 0x7 << 4
} CTRL_REG1_ODR;

typedef enum {
    ONE_SHOT_WAIT           = 0x0 << 0,
    ONE_SHOT_START          = 0x1 << 0
} CTRL_REG2_ONE_SHOT;

int exec_read(int fd, LPS331AP_REG reg, uint8_t buf[], uint8_t buf_size)
{
    if ((write(fd, &reg, 1)) != 1) {
        fprintf(stderr, "ERROR: i2c write\n");
        exit(EXIT_FAILURE);
    }
    if (read(fd, buf, 1) != 1) { 
        fprintf(stderr, "ERROR: i2c read\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int exec_write(int fd, LPS331AP_REG reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    if ((write(fd, buf, 2)) != 2) {
        fprintf(stderr, "ERROR: i2c write\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int exec_command(int fd, LPS331AP_COMMAND cmd, uint8_t write_val, uint32_t *read_val)
{
    uint8_t buf[2];
    uint32_t value;

    memset(buf, 0, sizeof(buf));

    switch (cmd) {
    case CMD_WHO_AM_I:
        exec_read(fd, REG_WHO_AM_I, buf, ARRAY_SIZE_OF(buf));
        if (buf[0] != 0xBB) {
            fprintf(stderr, "ERROR: invalid value\n");
            exit(EXIT_FAILURE);
        }
        break;
    case CMD_CTRL1:
        exec_write(fd, REG_CTRL_REG1, write_val);
        break;
    case CMD_CTRL2:
        exec_write(fd, REG_CTRL_REG2, write_val);
        break;
    case CMD_WAIT_BOTH:
        for (uint8_t i = 0; i < WAIT_COUNT; i++) {
            exec_read(fd, REG_STATUS, buf, ARRAY_SIZE_OF(buf));
            if ((buf[0] & 0x3) == 0x3) {
                break;
            }
            usleep(100000); // wait 100ms
        }
        if ((buf[0] & 0x3) != 0x3) {
            fprintf(stderr, "ERROR: data not ready\n");
            exit(EXIT_FAILURE);
        }
        break;
    case CMD_PRESS_OUT:
        value = 0;
        exec_read(fd, REG_PRESS_OUT_XL, buf, ARRAY_SIZE_OF(buf));
        value = buf[0] << 0 | value;
        exec_read(fd, REG_PRESS_OUT_L, buf, ARRAY_SIZE_OF(buf));
        value = buf[0] << 8 | value;
        exec_read(fd, REG_PRESS_OUT_H, buf, ARRAY_SIZE_OF(buf));
        value = buf[0] << 16 | value;
        *read_val = value;
        break;
    case CMD_TEMP_OUT:
        value = 0;
        exec_read(fd, REG_TEMP_OUT_L, buf, ARRAY_SIZE_OF(buf));
        value = buf[0] << 0 | value;
        exec_read(fd, REG_TEMP_OUT_H, buf, ARRAY_SIZE_OF(buf));
        value = buf[0] << 8 | value;
        *read_val = value;
        break;
    }


    usleep(10000); // wait 10ms

    return 0;
}

float calc_press(uint32_t value)
{
    return value / 4096.0;
}

float calc_temp(uint32_t value)
{
    return 42.5 + (int16_t)value/480;
}

int main(int argc, char **argv) {
    int fd; 
    uint8_t bus;
    uint8_t dev_addr_i;
    uint8_t dev_addr;
    char i2c_dev_path[64];
    uint32_t press;
    uint32_t temp;

    bus = 0;
    dev_addr_i = 0;

    if (argc != 1) {
        bus = (uint8_t)strtol(argv[1], NULL, 0);
    }
    if (argc > 3) {
        dev_addr_i = (uint8_t)strtol(argv[2], NULL, 0);
    }
    dev_addr = (dev_addr_i == 1) ? I2C_DEV_ADDR_1 : I2C_DEV_ADDR_0;

    sprintf(i2c_dev_path, "/dev/i2c-%d", bus);
    if ((fd = open(i2c_dev_path, O_RDWR)) < 0) {
        printf("ERROR: Faild to open i2c port\n");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, I2C_SLAVE, dev_addr) < 0) {
        printf("ERROR: Unable to get bus access\n");
        return EXIT_FAILURE;
    }

    exec_command(fd, CMD_WHO_AM_I, 0, NULL);
    exec_command(fd, CMD_CTRL1, PD_ACTIVE|ODR_OUT_FREQ_ONE_ONE, NULL);
    exec_command(fd, CMD_CTRL2, ONE_SHOT_START, NULL);
    exec_command(fd, CMD_WAIT_BOTH, 0, NULL);
    exec_command(fd, CMD_PRESS_OUT, 0, &press);
    exec_command(fd, CMD_TEMP_OUT, 0, &temp);
    
    printf("PRESS: %.2f\n", calc_press(press));
    printf("TEMP: %.2f\n", calc_temp(temp));

    return EXIT_SUCCESS;
}

// Local Variables:
// mode: c
// c-basic-offset: 4
// tab-width: 4
// indent-tabs-mode: nil
// End:
