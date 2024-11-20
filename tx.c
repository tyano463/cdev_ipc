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

    // キャラクタデバイスが作成されるまで待つ
    printf("Waiting for the device...\n");
    while (access(DEVICE_NAME, F_OK) == -1)
    {
        usleep(100000); // 100ms 待つ
    }

    // キャラクタデバイスをオープン
    fd = open(DEVICE_NAME, O_WRONLY);
    if (fd == -1)
    {
        perror("Failed to open device");
        return 1;
    }

    // サンプル_A が送信する文字列
    const char *message = "Hello, world!\n";
    write(fd, message, strlen(message) + 1);
    printf("Sent: %s\n", message);

    // 送信後、受信モードに切り替える
    close(fd);
    fd = open(DEVICE_NAME, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open device for reading");
        return 1;
    }

    // サンプル_B からの返信を受信
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

        buffer[len] = '\0'; // 文字列を終端
        printf("Received: %s\n", buffer);
    }
    close(fd);
    return 0;
}