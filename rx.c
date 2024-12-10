// sample_b.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/sysmacros.h>

#include "cdev.h"

int main()
{
    int fd;
    char buffer[128];
    struct stat st;

    // キャラクタデバイスをオープンして受信
    fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open device err:%s(%d)\n", strerror(errno), errno);
        return 1;
    }

    printf("Device opened, waiting for input...\n");

    // 受信したデータを処理
    while (1)
    {
        ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
        if (len < 0)
        {
            fprintf(stderr, "Failed to read from device, %s(%d)\n", strerror(errno), errno);
            break;
        }

        if (!len)
            continue;
        printf("Received: (%d) %s\n", len, buffer);

        if (strcmp(CLOSE_COMMAND, buffer) == 0)
        {
            const char *msg = CLOSE_COMMAND;
            write(fd, msg, strlen(msg));
            break;
        }

        if (strchr(buffer, '\n') != NULL)
        {
            char reply[128];
            snprintf(reply, sizeof(reply), "Prefix: %s", buffer);
            write(fd, reply, strlen(reply));
            printf("Sent: %s\n", reply);
        }
    }
    close(fd);

    return 0;
}
