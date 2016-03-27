#define LINUX
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
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
#define LINE_LENGTH 40
/* END FILE OPERATIONS */

/* PCB */
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

/* vmalloc */
#define PAGE_SIZE 4096
static char *memory_buffer;
/* end vmalloc */

static void debug_print_list(void) {
    printk(KERN_ALERT "I EXIST\n");
    mp3_pcb *i;
    list_for_each_entry(i, &pcb_list_head, pcb_list_node) {
        printk(KERN_ALERT "Have pid: %u\n", i->pid);
    }
}

static void REGISTER(unsigned int pid) {
    mp3_pcb *pcb;
    struct task_struct *task;

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
    list_add(&(pcb->pcb_list_node), &pcb_list_head);
    mutex_unlock(&pcb_list_mutex);

    // debug_print_list();

    // TODO: create workqueue job if the requesting process is the first
    // one in the PCB list? Piazza question

    printk(KERN_ALERT "REGISTER successful!\n");
}

static void UNREGISTER(unsigned int pid) {
    mp3_pcb *i;
    mp3_pcb *next;
    
    printk(KERN_ALERT "UNREGISTER called for pid: %u\n", pid);

    mutex_lock(&pcb_list_mutex);
    list_for_each_entry_safe(i, next, &pcb_list_head, pcb_list_node) {
        if (i->pid == pid) {
            list_del(&(i->pcb_list_node));
            kfree(i);
        }
    }
    mutex_unlock(&pcb_list_mutex);
    
    // debug_print_list();

    // TODO: if the PCB list is empty after the delete operation, the workqueue
    // is deleted as well
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

    // set up procfs
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp3_file); 

    memory_buffer = vmalloc(128 * PAGE_SIZE);
    // TODO: set PG_reserved

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
    
    vfree(memory_buffer);

    #ifdef DEBUG
        printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
    #endif
}

// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
