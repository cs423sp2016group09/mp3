#define LINUX
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_09");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1

#define QUEUE_SIZE 12000

//#define PAGE_SIZE 4096
/* FILE OPERATIONS */
#define FILENAME "status"
#define DIRECTORY "mp3"
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static int finished_writing;
#define READ_BUFFER_SIZE 400
#define LINE_LENGTH 40
/* END FILE OPERATIONS */

/* Workqueue struct */
static struct workqueue_struct *mp3_wq;
static void create_wq_job(void);

/* Character device */
static int chrdev_open(struct inode *i, struct file *f)
{
    return 0;
}
static int chrdev_release(struct inode *i, struct file *f)
{
    return 0;
}
static int chrdev_mmap(struct file *f, struct vm_area_struct *vas)
{
    return 0;
}
static struct file_operations chrdev_fops = {
    .open = chrdev_open,
    .release = chrdev_release,
    .mmap = chrdev_mmap
};
static int chrdev_major;
/* end character device */

/* PCB */
static int pcb_num_elements; /*Flag for checking pcb*/
static LIST_HEAD(pcb_list_head);
static DEFINE_MUTEX(pcb_list_mutex);
typedef struct mp3_pcb_struct {
    struct list_head pcb_list_node;
    struct task_struct *task;
    unsigned int pid; // TODO: maybe

    unsigned long process_utilization;
    unsigned long major_fault_count;
    unsigned long minor_fault_count;
} mp3_pcb;
/* END PCB */


typedef struct mp3_sample_struct {

    // struct list_head mem_list_node;
    unsigned long jffs;
    unsigned long major_faults;
    unsigned long minor_faults;
    unsigned long cpu_util;

} mp3_sample;


/* vmalloc */
static unsigned int quantity_unread;
static unsigned int read_idx;
static unsigned int write_idx;
static mp3_sample *memory_buffer;
/* end vmalloc */

/*
static void debug_print_list(void) {

    mp3_pcb *i; /
    printk(KERN_ALERT "I EXIST\n");
    list_for_each_entry(i, &pcb_list_head, pcb_list_node) {
        printk(KERN_ALERT "Have pid: %u\n", i->pid);
    }
}
*/
static long counter;
static void wq_fun(struct work_struct *mp3_work) {
    mp3_pcb *i;

    unsigned long jffs = jiffies;
    unsigned long major_faults = 0;
    unsigned long minor_faults = 0;
    unsigned long cpu_util = 0; 
    mp3_sample *sample = &memory_buffer[write_idx];
    if (pcb_num_elements > 0) {
        list_for_each_entry(i, &pcb_list_head, pcb_list_node) {

            unsigned long maj_flt = 0;
            unsigned long min_flt = 0;
            unsigned long utime = 0; 
            unsigned long stime = 0;
            unsigned long j_utime;
            unsigned long j_stime;

            get_cpu_use(i->pid, &(maj_flt), &(min_flt), &(utime), &(stime));

            major_faults += maj_flt;
            minor_faults += min_flt;

            // ask whether to divide by jiffies or jffs?
            // TODO: this is wrong! it always does 0
            

            j_utime = cputime_to_jiffies(utime); 
            j_stime = cputime_to_jiffies(stime);

            printk(KERN_ALERT "utime:%lu\nstime:%lu\nj_utime:%lu\nj_stime:%lu\n", utime, stime, j_utime, j_stime);
            cpu_util += ((j_stime + j_utime) * 1000 / jiffies);
        }

        sample->jffs = jffs;
        sample->major_faults = major_faults;
        sample->minor_faults = minor_faults;
        sample->cpu_util = cpu_util;
        // printk(KERN_ALERT "Logging sample: %lu\nmaj: %lu\nmin: %lu\ncpu: %lu\nwrite_idx:%d\nread_idx:%d\n", jffs, major_faults, minor_faults, cpu_util, write_idx, read_idx);

        // they point to the same spot because the queue is full
        // advance both pointers and don't update the quantity
        if (write_idx == read_idx && quantity_unread == QUEUE_SIZE) {
                read_idx = (read_idx + 1) % QUEUE_SIZE;
                write_idx = (write_idx + 1) % QUEUE_SIZE;
        } else {
            write_idx = (write_idx + 1) % QUEUE_SIZE;
            quantity_unread++;
        }

        create_wq_job();
    }
}

static struct delayed_work mp3_work;

static void create_wq_job(void) {
    INIT_DELAYED_WORK(&mp3_work, wq_fun);
    queue_delayed_work(mp3_wq, &mp3_work, msecs_to_jiffies(50));
}

// // helpers for timer code
// static struct timer_list myTimer;
// void timerFun (unsigned long arg) {
//     myTimer.expires = jiffies + 5 * HZ;
//     add_timer (&myTimer); /* setup the timer again */
    


//     // schedule_work(&mp3_wq); // trigger the bottom half
//     // 
    
    
//     // create_wq_job();
// }



    

static void REGISTER(unsigned int pid) {
    mp3_pcb *pcb;
    struct task_struct *task;
    //unsigned long currentTime;
    //unsigned long expiryTime;

    printk(KERN_ALERT "REGISTER called for pid: %u\n", pid);
    task = find_task_by_pid(pid);

    // pid not valid
    if (task == NULL) { 
        printk(KERN_ALERT "find_task_by_pid failed\n");
        return;
    }
    
    pcb = kmalloc(sizeof(mp3_pcb), GFP_KERNEL);
    pcb->task = task;
    pcb->pid = pid;

    mutex_lock(&pcb_list_mutex);
    if (pcb_num_elements == 0) {
        // currentTime = jiffies; // pre-defined kernel variable jiffies gives current value of ticks
        // expiryTime = currentTime + 5 * HZ; 
        // init_timer (&myTimer);
        // myTimer.function = timerFun;
        // myTimer.expires = expiryTime;
        // myTimer.data = 0;
        // add_timer (&myTimer);
        create_wq_job();
    }
    list_add(&(pcb->pcb_list_node), &pcb_list_head);
    pcb_num_elements++;
    mutex_unlock(&pcb_list_mutex);

    // debug_print_list(); //Function is commented out above because it throws up a warning. Uncomment to use.


    printk(KERN_ALERT "REGISTER successful!\n");
}

static void UNREGISTER(unsigned int pid) {
    mp3_pcb *i;
    mp3_pcb *next;
    
    printk(KERN_ALERT "UNREGISTER called for pid: %u\n", pid);
    // printk(KERN_ALERT "line 136\n");
    mutex_lock(&pcb_list_mutex);
    // printk(KERN_ALERT "line 138\n");
    list_for_each_entry_safe(i, next, &pcb_list_head, pcb_list_node) {
        // printk(KERN_ALERT "line 140\n");
        if (i->pid == pid) {
            // printk(KERN_ALERT "line 142\n");
            list_del(&(i->pcb_list_node));
            // printk(KERN_ALERT "line 144\n");
            kfree(i);
            // printk(KERN_ALERT "line 146\n");
            pcb_num_elements--;
            // printk(KERN_ALERT "line 148\n");
        }
    }

    // printk(KERN_ALERT "line 152\n");
    if (pcb_num_elements == 0) {
        // printk(KERN_ALERT "line 154\n");
        // del_timer(&myTimer);
        // printk(KERN_ALERT "line 156\n");
        
        // printk(KERN_ALERT "line 158\n");
        // mp3_wq = NULL;
    }   
    // printk(KERN_ALERT "line 161\n");
    mutex_unlock(&pcb_list_mutex);
    // printk(KERN_ALERT "line 163\n");
    // debug_print_list(); //Function is commented out above because it throws up a warning. Uncomment to use.

}


static ssize_t mp3_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
    int copied;
    char * buf;
    char * line;
    int line_length;
    char * buf_curr_pos;
    mp3_pcb *i;

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
    memset(buf, 0, READ_BUFFER_SIZE);
    buf_curr_pos = buf;

    mutex_lock(&pcb_list_mutex);

    list_for_each_entry(i, &pcb_list_head, pcb_list_node) {
        // allocate line long enoguh to hold the string
        line = kmalloc(LINE_LENGTH, GFP_KERNEL);
        memset(line, 0, LINE_LENGTH);

        sprintf(line, "PID: %u\n", i->pid);
        line_length = strlen(line);
        
        snprintf(buf_curr_pos, line_length + 1, "%s", line); // + 1 to account for the null char
        buf_curr_pos += line_length; // advance the buffer 

        kfree(line);
    }
    mutex_unlock(&pcb_list_mutex);

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
    
    pcb_num_elements = 0;
    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp3_file); 

    mp3_wq = create_workqueue("wq");

    memory_buffer = vmalloc(128 * PAGE_SIZE);
    // set_bit(PG_reserved, &virt_to_page(memory_buffer)->flags);
    chrdev_major = register_chrdev(0, "node", &chrdev_fops); 

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
    
    destroy_workqueue(mp3_wq);

    unregister_chrdev(chrdev_major, "node");

    vfree(memory_buffer);

    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
