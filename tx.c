#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "cdev.h"

int main(void)
{
    int fd;
    char buffer[128];

    printf("Waiting for the device...\n");
    while (access(DEVICE_NAME, F_OK) == -1)
    {
        usleep(100000);
    }

    fd = open(DEVICE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        return 1;
    }

    const char *message = "Hello, world!\n";
    write(fd, message, strlen(message) + 1);
    printf("Sent: %s\n", message);

    while (1)
    {
        ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
        if (len == -1)
        {
            perror("Failed to read from device");
            return 1;
        }

        if (!len)
            continue;

        buffer[len] = '\0';
        printf("Received: %s\n", buffer);
        if (strcmp(buffer, CLOSE_COMMAND) == 0)
        {
            break;
        }

        message = CLOSE_COMMAND;
        write(fd, message, strlen(message) + 1);
        printf("Sent: %s\n", message);
    }
    close(fd);
    return 0;
}