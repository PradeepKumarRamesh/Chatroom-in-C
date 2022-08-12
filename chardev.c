#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>

#define BUFLEN 100
#define FIFO_SIZE 65535
struct kfifo_rec_ptr_2 kfifo_buffer_1, kfifo_buffer_2, kfifo_buffer_0;
spinlock_t lock1, lock2, lock0;
int pcount = 0;
int processid1 = 0, processid2 = 0, processid0 = 0;

static ssize_t device_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char *module_buf;
	module_buf = (char *)kmalloc(BUFLEN, GFP_USER);
	if(!module_buf) 
		return -1; 
	char *module_buf1;
	module_buf1 = (char *)kmalloc(len, GFP_USER);
	if(!module_buf1) 
		return -1;
	copy_from_user(module_buf, buf, len);
	if(get_current()->tgid == processid0){
		sprintf(module_buf1, "P1> %s", module_buf);
		if(processid1 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_1, module_buf1, strlen(module_buf1), &lock1);
		}
		if(processid2 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_2, module_buf1, strlen(module_buf1), &lock2);
		}
	}
	else if(get_current()->tgid == processid1){
		sprintf(module_buf1, "P2> %s", module_buf);
		if(processid0 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_0, module_buf1, strlen(module_buf1), &lock0);
		}
		if(processid2 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_2, module_buf1, strlen(module_buf1), &lock2);
		}
	}
	else if(get_current()->tgid == processid2){
		sprintf(module_buf1, "P3> %s", module_buf);
		if(processid0 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_0, module_buf1, strlen(module_buf1), &lock0);
		}
		if(processid1 != 0){
			kfifo_in_spinlocked(&kfifo_buffer_1, module_buf1, strlen(module_buf1), &lock1);
		}
	}
	kfree(module_buf);
	kfree(module_buf1);
	return len; 
}

static ssize_t device_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	char *module_buf;
	module_buf = (char *)kmalloc(len, GFP_USER);
	if(!module_buf) 
		return -1;
	unsigned int ret;
	sprintf(module_buf, "");
	if(get_current()->tgid == processid0){
		if(!kfifo_is_empty(&kfifo_buffer_0)) {
        	ret = kfifo_out_spinlocked(&kfifo_buffer_0, module_buf, len, &lock0);
			module_buf[ret] = '\0';
		}
	}
	else if(get_current()->tgid == processid1){
		if(!kfifo_is_empty(&kfifo_buffer_1)) {
        	ret = kfifo_out_spinlocked(&kfifo_buffer_1, module_buf, len, &lock1);
			module_buf[ret] = '\0';
		}
	}
	else if(get_current()->tgid == processid2){
		if(!kfifo_is_empty(&kfifo_buffer_2)) {
        	ret = kfifo_out_spinlocked(&kfifo_buffer_2, module_buf, len, &lock2);
			module_buf[ret] = '\0';
		}
	}
	len = copy_to_user(buf, module_buf, strlen(module_buf));
	kfree(module_buf);
	return len; 
}

static int device_open(struct inode *inode, struct file *file)
{
	char module_buf[BUFLEN];
	sprintf(module_buf, "You are P%d", pcount+1);
	if(pcount > 3){
		printk(KERN_ERR "Only 3 process allowed\n");
		return -1;
	}
	else if(pcount==0){
		if(kfifo_alloc(&kfifo_buffer_0, FIFO_SIZE, GFP_KERNEL)){
			printk(KERN_ERR "error kfifo_alloc\n");
			return -1;
		}
		spin_lock_init(&lock0);
		processid0 = get_current()->tgid;
		kfifo_in_spinlocked(&kfifo_buffer_0, module_buf, sizeof(module_buf), &lock0);
    }
	else if(pcount==1){
		if(kfifo_alloc(&kfifo_buffer_1, FIFO_SIZE, GFP_KERNEL)){
			printk(KERN_ERR "error kfifo_alloc\n");
			return -1;
		}
		spin_lock_init(&lock1);
		processid1 = get_current()->tgid;
		kfifo_in_spinlocked(&kfifo_buffer_1, module_buf, sizeof(module_buf), &lock1);
    }
	else if(pcount==2){
		if(kfifo_alloc(&kfifo_buffer_2, FIFO_SIZE, GFP_KERNEL)){
			printk(KERN_ERR "error kfifo_alloc\n");
			return -1;
		}
		spin_lock_init(&lock2);
		processid2 = get_current()->tgid;
		kfifo_in_spinlocked(&kfifo_buffer_2, module_buf, sizeof(module_buf), &lock2);
    }
	pcount++;
	return 0;
}

static int device_close(struct inode *inodep, struct file *filp)
{
	if(get_current()->tgid == processid0){
		kfifo_free(&kfifo_buffer_0);
		processid0 = 0;
	}
	else if(get_current()->tgid == processid1){
		kfifo_free(&kfifo_buffer_1);
		processid1 = 0;
	}
	else if(get_current()->tgid == processid2){
		kfifo_free(&kfifo_buffer_2);
		processid2 = 0;
	}
	return 0;
}

static const struct file_operations device_operations = {
    .owner	 = THIS_MODULE,
    .open	 = device_open,
    .read	 = device_read,
    .write	 = device_write,
    .release = device_close
};

struct miscdevice dev_chat = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "chatroom",
    .fops = &device_operations,
    .mode = S_IRUGO | S_IWUGO,
};

static int __init start_device(void)
{
	if (misc_register(&dev_chat) != 0) {
		return -1;
	}
	return 0;
}

static void __exit end_device(void)
{
	misc_deregister(&dev_chat);
}

module_init(start_device)
module_exit(end_device)
MODULE_DESCRIPTION("chatroom kernel module...\n");
MODULE_AUTHOR("Pradeep Ramesh");
MODULE_LICENSE("GPL");

