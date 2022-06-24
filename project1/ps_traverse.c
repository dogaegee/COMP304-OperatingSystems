#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

static char *flag;
int pid = 0;
module_param(pid, int, 0); //module parameter pid
module_param(flag, charp, 0); //module parameter flag


void depth_first_search(struct task_struct * parent_task){   
    struct list_head *list;
    struct task_struct *task;
    printk("Task Command: %s\n", parent_task->comm);
    printk("PID: %d\n", parent_task->pid);
    list_for_each(list, &parent_task->children) {
    	task = list_entry(list, struct task_struct, sibling);
    	depth_first_search(task);
    }
}

void breadth_first_search(struct task_struct *parent_task){   
    struct list_head *list;
    struct task_struct *task;
    printk("Task Command: %s\n", parent_task->comm);
    printk("PID: %d\n", parent_task->pid);
    list_for_each(list, &parent_task->children) {
    	breadth_first_search(task);
    	task = list_entry(list, struct task_struct, sibling);
    }
}

int my_driver_init(void){
	printk(KERN_INFO "Ps Traverse Module started!\n");
	struct task_struct *task = pid_task(find_get_pid(pid), PIDTYPE_PID);
	//We first check whether the given task is valid or not. If it is not a valid pid, it shows an error.
	//If it is valid, we call the corresponding search function according to the given flag.
    	if(task == NULL) {
    		printk(KERN_ERR "No such PID found!\n");
	} else if(!strcmp(flag,"-d")){
            	depth_first_search(task);
        } else if (!strcmp(flag,"-b")){
        	breadth_first_search(task);
        }
    return 0;
}


void my_driver_exit(void){
	printk(KERN_INFO "Ps Traverse Module exited!\n");
}


module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BETUL DEMIRTAS - DOGA EGE INHANLI");
MODULE_DESCRIPTION("The driver");
