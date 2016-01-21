#include<linux/sched.h>
#include<linux/rtc.h>
#include<linux/seq_file.h>
#include<linux/proc_fs.h>
/*
	head file of PidMonitor
*/
//The max number of process the system may have,
//get from /proc/sys/kernel/pid_max
#define PROCESS_NUMBER 32770

//Contain informations of each process
typedef struct process_informaion{
	unsigned int pid;
	unsigned int ppid;
	char pname[TASK_COMM_LEN];
	char ppname[TASK_COMM_LEN];
	char tty[8];
	unsigned int state;
	char start[16];		//start time
	char time[16];		//running time
	char *dentry;		//where is the executable file
}pidinfo;

//Contain informations of all processes
pidinfo pid_table[PROCESS_NUMBER];

//Indicate whether the process is destroyed or not
char pid_new_table[PROCESS_NUMBER];

//RTC TIMEZONE has 8 hour's offset
#define TIME_OFFSET ((24*30+8)*3600)

//Get running time of process
static int get_run_time(char *,cputime_t);
//Get start time of process
static int get_start_time(char *,struct timespec);
//Get all task informations,print to pid_table
static int get_all_task_info(void);

//structs and functions below are for seq_file operations
static struct proc_dir_entry *entry;
static int pid_single_open(struct inode *,struct file *);//open handler
static int pid_seq_show(struct seq_file *,void *);//write data to proc file
static struct file_operations pid_proc_ops={
	.owner	=	THIS_MODULE,
	.open	=	pid_single_open,
	.read	=	seq_read,
	.llseek	=	seq_lseek,
	.release=	single_release,
};

