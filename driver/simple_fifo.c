#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>

#define DEVICE_NAME "simple_fifo"
#define BUF_SIZE 256

// キャラクタデバイスの内部バッファ
static char fifo_buf[BUF_SIZE];
static int read_pos = 0;  // 読み取り位置
static int write_pos = 0; // 書き込み位置

// デバイスファイルを保持する構造体
static dev_t dev_number;
static struct cdev cdev;
static struct class *cls;

// デバイスのオープン
static int fifo_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "FIFO: Opened\n");
    return 0;
}

// デバイスのクローズ
static int fifo_close(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "FIFO: Closed\n");
    return 0;
}

// 読み取り
static ssize_t fifo_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    int bytes_read = 0;

    // バッファにデータがあるか確認
    while (read_pos != write_pos && bytes_read < count)
    {
        put_user(fifo_buf[read_pos], &buf[bytes_read]);
        fifo_buf[read_pos] = '\0';
        read_pos = (read_pos + 1) % BUF_SIZE;
        bytes_read++;
    }

    if (bytes_read > 0)
    {
        printk(KERN_INFO "FIFO: Read %d bytes\n", bytes_read);
    }
    else
    {
        printk(KERN_INFO "FIFO: No data to read\n");
    }

    return bytes_read;
}

// 書き込み
static ssize_t fifo_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int bytes_written = 0;

    // バッファが満杯でないか確認
    while (bytes_written < count && ((write_pos + 1) % BUF_SIZE) != read_pos)
    {
        get_user(fifo_buf[write_pos], &buf[bytes_written]);
        write_pos = (write_pos + 1) % BUF_SIZE;
        bytes_written++;
    }

    if (bytes_written > 0)
    {
        printk(KERN_INFO "FIFO: Written %d bytes\n", bytes_written);
    }
    else
    {
        printk(KERN_INFO "FIFO: Buffer full\n");
    }

    return bytes_written;
}

// ファイル操作構造体
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = fifo_open,
    .release = fifo_close,
    .read = fifo_read,
    .write = fifo_write,
};

// モジュールの初期化関数
static int __init fifo_init(void)
{
    // デバイス番号の動的割り当て
    if (alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME) < 0)
    {
        printk(KERN_ALERT "FIFO: Failed to allocate a device number\n");
        return -1;
    }

    // cdev構造体の初期化
    cdev_init(&cdev, &fops);
    cdev_add(&cdev, dev_number, 1);

    // デバイスクラスの作成
    cls = class_create(DEVICE_NAME);
    if (IS_ERR(cls))
    {
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(cls);
    }

    // デバイスファイルの作成
    if (device_create(cls, NULL, dev_number, NULL, DEVICE_NAME) == NULL)
    {
        class_destroy(cls);
        unregister_chrdev_region(dev_number, 1);
        return -1;
    }

    printk(KERN_INFO "FIFO: Device has been registered\n");
    return 0;
}

// モジュールのクリーンアップ関数
static void __exit fifo_exit(void)
{
    device_destroy(cls, dev_number);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "FIFO: Device has been unregistered\n");
}

module_init(fifo_init);
module_exit(fifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple FIFO character device driver");
