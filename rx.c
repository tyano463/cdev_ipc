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

    // キャラクタデバイスを作成
    if (stat(DEVICE_NAME, &st) == 0)
    {
        unlink(DEVICE_NAME);
        usleep(100000); // 100ms 待つ
    }

    if (mknod(DEVICE_NAME, S_IFCHR | 0666, makedev(511, 0)) == -1)
    {
        perror("mknod failed");
        return 1;
    }

    // デバイスファイルが作成されるまで待つ（簡易的な待機）
    int cnt = 0;
    printf("Waiting for the device...\n");
    while (stat(DEVICE_NAME, &st) != 0)
    {
        usleep(100000); // 100ms 待つ
        cnt++;
    }
    // printf("Device: %lx\n", (unsigned long)st.st_dev);
    // printf("Inode: %ld\n", (long)st.st_ino);
    // printf("Mode: %o\n", st.st_mode);
    // printf("Number of links: %ld\n", (long)st.st_nlink);
    // printf("UID: %d\n", st.st_uid);
    // printf("GID: %d\n", st.st_gid);
    // printf("Device type: %lx\n", (unsigned long)st.st_rdev);
    // printf("Block size: %ld\n", (long)st.st_blksize);
    // printf("Number of blocks: %ld\n", (long)st.st_blocks);
    // printf("Last access time: %ld\n", (long)st.st_atime);
    // printf("Last modification time: %ld\n", (long)st.st_mtime);
    // printf("Last status change time: %ld\n", (long)st.st_ctime);

    // // st_rdev に含まれるデバイスの種類
    // if (S_ISCHR(st.st_mode))
    // {
    //     printf("It is a character device\n");
    // }
    // else if (S_ISBLK(st.st_mode))
    // {
    //     printf("It is a block device\n");
    // }
    // else
    // {
    //     printf("It is not a character or block device\n");
    // }
    // dev_t dev = st.st_rdev;
    // printf("Major: %d, Minor: %d\n", major(dev), minor(dev));

    usleep(100000);
    // キャラクタデバイスをオープンして受信
    fd = open(DEVICE_NAME, O_RDONLY);
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

        // 改行で区切られた文字列を処理
        if (strchr(buffer, '\n') != NULL)
        {
            // プレフィックスを付けて返す
            char reply[128];
            snprintf(reply, sizeof(reply), "Prefix: %s", buffer);
            close(fd);
            fd = open(DEVICE_NAME, O_WRONLY);
            write(fd, reply, strlen(reply));
            close(fd);
            fd = open(DEVICE_NAME, O_RDONLY);
            printf("Sent: %s\n", reply);
        }
    }

    return 0;
}
