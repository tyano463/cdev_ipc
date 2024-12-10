#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/rwsem.h>

#include "../cdev.h"

#define DRIVER_NAME "simple_fifo"
#define BUF_SIZE 256

#define MAX_TTY_NUM 10
#define MAX_CHARACTER_DEVICE MAX_TTY_NUM
#define MAX_OPEN_PROCESS 2

#define ERR_RETp(c, p, s, ...)                                 \
    do                                                         \
    {                                                          \
        if (c)                                                 \
        {                                                      \
            printk(KERN_ALERT "FIFO: " s "\n", ##__VA_ARGS__); \
            p;                                                 \
            goto error_return;                                 \
        }                                                      \
    } while (0)

#define ERR_RET(c, s, ...)                                     \
    do                                                         \
    {                                                          \
        if (c)                                                 \
        {                                                      \
            printk(KERN_ALERT "FIFO: " s "\n", ##__VA_ARGS__); \
            goto error_return;                                 \
        }                                                      \
    } while (0)


struct fifo_dev
{
    int proc_id;
    pid_t pid;
};

typedef struct
{
    int id;
    dev_t dev;
    struct cdev cdev;
} qemu_tty_t;

static int to_id(struct file *file);

static char fifo_buf[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS][BUF_SIZE];
static int read_pos[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};
static int write_pos[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};
static struct fifo_dev latest[MAX_CHARACTER_DEVICE][MAX_OPEN_PROCESS] = {0};

static DECLARE_RWSEM(fifo_rwsem);

qemu_tty_t qemu_dev[MAX_TTY_NUM];

static struct class *cls;
static struct cdev cdev;
static long dev_index = 0;
static char rdebug_str[BUF_SIZE];
static char wdebug_str[BUF_SIZE];

static int device_number(struct inode *inode)
{
    int minor = 0;
    ERR_RETn(!inode);
    ERR_RETn(!inode->i_cdev);

    //    printk("FIFO dev:%x\n", inode->i_cdev->dev);
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
    }
    printk(KERN_INFO "FIFO Open: %p pid:%d id:%d d:%d\n", file, fifo_dev->pid, to_id(file), minor);
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

    if (!down_read_trylock(&fifo_rwsem))
    {
        return 0;
    }
    for (int i = 0; i < MAX_OPEN_PROCESS; i++)
    {
        if (i == id)
            continue;
        if (read_pos[minor][i] != write_pos[minor][i])
            printk(KERN_INFO "FIFO: %p (%d)[%d] rp:%d wp:%d cnt:%lu", file, id, minor, read_pos[minor][i], write_pos[minor][i], count);
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
    up_read(&fifo_rwsem);

    if (rpos)
    {
        rdebug_str[rpos] = '\0';
        printk(KERN_INFO "FIFO: (%d) r[%d]: (%d) %s\n", id, minor, bytes_read, rdebug_str);
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

    down_write(&fifo_rwsem);

    while (bytes_written < count && ((write_pos[minor][id] + 1) % BUF_SIZE) != read_pos[minor][id])
    {
        get_user(fifo_buf[minor][id][write_pos[minor][id]], &buf[bytes_written]);
        wdebug_str[wpos++] = fifo_buf[minor][id][write_pos[minor][id]];
        write_pos[minor][id] = (write_pos[minor][id] + 1) % BUF_SIZE;
        bytes_written++;
    }

    up_write(&fifo_rwsem);

    if (wpos)
    {
        wdebug_str[wpos] = '\0';
        printk(KERN_INFO "FIFO: (%d) w[%d]:%s\n", id, minor, wdebug_str);
    }

error_return:
    return bytes_written;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = fifo_open,
    .release = fifo_close,
    .read = fifo_read,
    .write = fifo_write,
};

static int check_class(struct class *cls)
{
    if (IS_ERR(cls))
    {
        return PTR_ERR(cls);
    }
    return 0;
}

static void release_dev(int n)
{
    dev_t dev;
    for (int i = 0; i < n; i++)
    {
        dev = MKDEV(DEVICE_MAJOR_VERSION, i);
        device_destroy(cls, dev);
    }
    cdev_del(&cdev);
    dev = MKDEV(DEVICE_MAJOR_VERSION, DEVICE_MINOR_VERSION);
    unregister_chrdev_region(dev, MAX_TTY_NUM);
    class_destroy(cls);
}

static int __init fifo_init(void)
{
    int i, result;
    dev_t dev;

    cls = class_create(DRIVER_NAME);
    result = check_class(cls);
    ERR_RETn(result != 0);

    dev = MKDEV(DEVICE_MAJOR_VERSION, 0);

    result = register_chrdev_region(dev, MAX_TTY_NUM, DRIVER_NAME);
    ERR_RET(result < 0, "Failed to allocate a device");

    cdev_init(&cdev, &fops);
    cdev.owner = THIS_MODULE;
    result = cdev_add(&cdev, dev, MAX_TTY_NUM);
    ERR_RETp(result < 0, release_dev(0), "Failed to add cdev");

    for (i = 0; i < MAX_TTY_NUM; i++)
    {
        dev = MKDEV(DEVICE_MAJOR_VERSION, i);
        result = device_create(cls, NULL, dev, NULL, "ttyQEMU%d", i) == NULL;
        ERR_RETp(result, release_dev(i), "Failed to create device for minor %d", i);
    }

    printk(KERN_INFO "FIFO: Device has been registered\n");
    result = 0;
error_return:
    return result;
}

static void __exit fifo_exit(void)
{
    release_dev(MAX_TTY_NUM);
    printk(KERN_INFO "FIFO: Device has been unregistered\n");
}

module_init(fifo_init);
module_exit(fifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yano, Takayuki");
MODULE_DESCRIPTION("A simple FIFO character device driver");
