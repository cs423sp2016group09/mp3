#define LINUX
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h> 
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/cache.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_09");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1

/* FILE OPERATIONS */
#define FILENAME "status"
#define DIRECTORY "mp3"
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static int finished_writing;
#define READ_BUFFER_SIZE 400
/* END FILE OPERATIONS */

static void REGISTER(unsigned int pid) {
    printk(KERN_ALERT "REGISTER called for pid: %u\n", pid);

    // TODO: Everything
}

static void UNREGISTER(unsigned int pid) {
    printk(KERN_ALERT "UNREGISTER called for pid: %u\n", pid);

    // TODO: Everything
}

static ssize_t mp3_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
    int copied;
    char * buf;
    char * line;
    int line_length;
    char * buf_curr_pos;


    // must return 0 to signal that we're done writing to cat
    if (finished_writing == 1) {
        printk(KERN_ALERT "mp3_read finished writing\n");
        finished_writing = 0;
        return 0;
    } 

    printk(KERN_ALERT "mp3_read called\n");
    
    copied = 0;

    // allocate a buffer big enough to hold the contents
    buf = (char *) kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
    // memset(buf, 0, READ_BUFFER_SIZE);
    // buf_curr_pos = buf;

    // list_for_each_entry(i, &head_task, task_node) {

    //     // allocate line long enoguh to hold the string
    //     line = kmalloc(LINE_LENGTH, GFP_KERNEL);
    //     memset(line, 0, LINE_LENGTH);

    //     sprintf(line, "PID: %u, period: %lu\n", i->pid, i->period);
    //     line_length = strlen(line);
        
    //     snprintf(buf_curr_pos, line_length + 1, "%s", line); // + 1 to account for the null char
    //     buf_curr_pos += line_length; // advance the buffer 

    //     kfree(line);
    // }

    copy_to_user(buffer, buf, READ_BUFFER_SIZE);
    finished_writing = 1; // return 0 on the next run
    kfree(buf);
    return READ_BUFFER_SIZE;
}

static ssize_t mp3_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){ 
    int copied;
    char *buf;
    unsigned int pid;

    printk(KERN_ALERT "mp3_write called\n");

    // unsigned long period;
    // unsigned long cputime;
    
    // manually null terminate
    buf = (char *) kmalloc(count+1,GFP_KERNEL); 
    copied = copy_from_user(buf, buffer, count);
    buf[count]=0;
    //parses the string and checks
    if (count > 0) {
        char cmd = buf[0];
        // example: R 345
        // indices: 01234
        sscanf(buf + 2, "%u", &pid);
        switch (cmd){
            case 'R':
                REGISTER(pid);
                break;
            case 'U':
                UNREGISTER(pid);
                break;
        }

    }
    
    kfree(buf);
    return count;
}

static const struct file_operations mp3_file = {
    .owner = THIS_MODULE, 
    .read = mp3_read,
    .write = mp3_write,
};

// mp3_init - Called when module is loaded
int __init mp3_init(void)
{
    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE LOADING\n");
    #endif

    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp3_file); 

    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE LOADED\n");
    #endif
    
    return 0;   
}

// mp3_exit - Called when module is unloaded
void __exit mp3_exit(void)
{
    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE UNLOADING\n");
    #endif
    
    // remove the procfs entry first so that users can't access it while we're deleting the list
    proc_remove(proc_entry);
    proc_remove(proc_dir);
    
    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
