#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>

#include "../cdev.h"

#define DRIVER_NAME "simple_fifo"
#define BUF_SIZE 256

#define MAX_CHARACTER_DEVICE 2
#define MAX_OPEN_PROCESS 2

#define ERR_RETn(c)            \
    do                         \
    {                          \
        if (c)                 \
            goto error_return; \
    } while (0)

struct fifo_dev
{
    int proc_id;
    pid_t pid;
};

static char fifo_buf[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS][BUF_SIZE];
static int read_pos[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};
static int write_pos[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};
static struct fifo_dev latest[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};

// デバイスファイルを保持する構造体
static dev_t dev;
static struct cdev cdev;
static struct class *cls;
static long dev_index = 0;
static char rdebug_str[64];
static char wdebug_str[64];

static int device_number(struct inode *inode)
{
    int minor = 0;
    ERR_RETn(!inode);
    ERR_RETn(!inode->i_cdev);

    minor = MINOR(inode->i_cdev->dev) % MAX_CHARACTER_DEVICE;

error_return:
    return minor;
}

static int fifo_open(struct inode *inode, struct file *file)
{
    struct fifo_dev *fifo_dev;
    int ret;
    file->private_data = NULL;

    ret = -EINVAL;
    ERR_RETn(!inode);
    ERR_RETn(!file);

    pid_t pid = task_pid_nr(current);
    int minor = device_number(inode);

    fifo_dev = kmalloc(sizeof(struct fifo_dev), GFP_KERNEL);

    ret = -ENOMEM;
    ERR_RETn(!fifo_dev);

    file->private_data = fifo_dev;

    fifo_dev->pid = 0;
    for (int i = 0; i < MAX_OPEN_PROCESS; i++)
    {
        if (latest[minor][i].pid == pid)
        {
            fifo_dev->pid = pid;
            fifo_dev->proc_id = latest[minor][i].proc_id;
        }
    }

    if (!fifo_dev->pid)
    {
        fifo_dev->pid = pid;
        fifo_dev->proc_id = dev_index++ % MAX_OPEN_PROCESS;
        latest[minor][fifo_dev->proc_id].proc_id = fifo_dev->proc_id;
        latest[minor][fifo_dev->proc_id].pid = fifo_dev->pid;
        if (dev_index > 100)
            dev_index = 1;
        printk(KERN_INFO "FIFO Open: pid:%d id:%d dev:%d\n", fifo_dev->pid, fifo_dev->proc_id, minor);
    }
    ret = 0;
error_return:
    return ret;
}

static int fifo_close(struct inode *inode, struct file *file)
{
    if (file->private_data)
        kfree(file->private_data);
    // printk(KERN_INFO "FIFO: Closed\n");
    return 0;
}

static int to_id(struct file *file)
{
    struct fifo_dev *fifo_dev = (struct fifo_dev *)file->private_data;
    if (fifo_dev)
        return fifo_dev->proc_id;
    else
        return -1;
}

static ssize_t fifo_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    int bytes_read = 0;
    int id = to_id(file);

    ERR_RETn(id < 0);
    int minor = device_number(file->f_path.dentry->d_inode);
    int rpos = 0;

    for (int i = 0; i < MAX_OPEN_PROCESS; i++)
    {
        if (i == id)
            continue;
        while (read_pos[minor][i] != write_pos[minor][i] && bytes_read < count)
        {
            put_user(fifo_buf[minor][i][read_pos[minor][i]], &buf[bytes_read]);
            rdebug_str[rpos++] = fifo_buf[minor][i][read_pos[minor][i]];
            fifo_buf[minor][i][read_pos[minor][i]] = '\0';
            read_pos[minor][i] = (read_pos[minor][i] + 1) % BUF_SIZE;
            bytes_read++;
        }
        if (bytes_read)
            break;
    }

    if (rpos) {
        rdebug_str[rpos] = '\0';
        printk(KERN_INFO "FIFO: (%d) r[%d]:%s\n", id, minor, rdebug_str);
    }

error_return:
    return bytes_read;
}

static ssize_t fifo_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int bytes_written = 0;
    int id = to_id(file);
    int wpos = 0;

    ERR_RETn(id < 0);
    ERR_RETn(!file);
    ERR_RETn(!file->f_path.dentry);
    ERR_RETn(!file->f_path.dentry->d_inode);

    int minor = device_number(file->f_path.dentry->d_inode);

    while (bytes_written < count && ((write_pos[minor][id] + 1) % BUF_SIZE) != read_pos[minor][id])
    {
        get_user(fifo_buf[minor][id][write_pos[minor][id]], &buf[bytes_written]);
        wdebug_str[wpos++] = fifo_buf[minor][id][write_pos[minor][id]];
        write_pos[minor][id] = (write_pos[minor][id] + 1) % BUF_SIZE;
        bytes_written++;
    }

    if (wpos) {
        wdebug_str[wpos] = '\0';
        printk(KERN_INFO "FIFO: (%d) w[%d]:%s\n", id, minor, wdebug_str);
    }

error_return:
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
    dev = MKDEV(DEVICE_MAJOR_VERSION, DEVICE_MINOR_VERSION);

    // デバイス番号の動的割り当て
    if (register_chrdev_region(dev, 1, DRIVER_NAME) < 0)
    {
        printk(KERN_ALERT "FIFO: Failed to allocate a device number\n");
        return -1;
    }

    // cdev構造体の初期化
    cdev_init(&cdev, &fops);
    cdev_add(&cdev, dev, 1);

    // デバイスクラスの作成
    cls = class_create(DRIVER_NAME);
    if (IS_ERR(cls))
    {
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(cls);
    }

    // デバイスファイルの作成
    if (device_create(cls, NULL, dev, NULL, DRIVER_NAME) == NULL)
    {
        class_destroy(cls);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    printk(KERN_INFO "FIFO: Device has been registered\n");
    return 0;
}

// モジュールのクリーンアップ関数
static void __exit fifo_exit(void)
{
    device_destroy(cls, dev);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "FIFO: Device has been unregistered\n");
}

module_init(fifo_init);
module_exit(fifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple FIFO character device driver");
