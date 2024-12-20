#ifndef __CDEV_H__
#define __CDEV_H__

#define DEVICE_MAJOR_VERSION 511
#define DEVICE_MINOR_VERSION 0

#define DEVICE_NAME "/dev/ttyQEMU0"

#define CLOSE_COMMAND "CLOSE\n"

#define ERR_RETn(c)            \
    do                         \
    {                          \
        if (c)                 \
            goto error_return; \
    } while (0)

#endif