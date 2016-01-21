#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/netlink.h>
#include<net/netlink.h>
#include<net/sock.h>
#include"PidMonitor.h"

#define NETLINK_MYTEST 28
struct sock *nl_sk=NULL;

//recieve message from user space
static void recv_message(struct sk_buff *__skb)
{
	//printk(KERN_INFO "Recieve a message.\n");
	get_all_task_info();
}

//Travel the task_struct list,and print it into pid_table
static int get_all_task_info()
{
	struct task_struct *taskp;
	int i;
	memset(pid_new_table,0,PROCESS_NUMBER);
	
	for_each_process(taskp)
	{
		i=taskp->pid;
		pid_table[i].pid=taskp->pid;
		pid_table[i].ppid=taskp->real_parent->pid;
		strcpy(pid_table[i].pname,taskp->comm);
		strcpy(pid_table[i].ppname,taskp->real_parent->comm);
		pid_table[i].state=taskp->state;
		//tty
		//dentry
		//start_time is not used in current
		get_start_time(pid_table[i].start,taskp->start_time);
		memset(pid_table[i].time,'\0',16);
		get_run_time(pid_table[i].time,taskp->utime+taskp->stime);
		pid_new_table[i]=1;
	//	printk(KERN_INFO "Pid:%d Comm:%s ppid:%d ppcomm:%s Run time:%s\n",
	//						pid_table[i].pid,pid_table[i].pname,pid_table[i].ppid,pid_table[i].ppname,pid_table[i].time);
	}
	return 0;
}

//get start time of process
static int get_start_time(char *dest_time,struct timespec start_time)
{
	struct rtc_time tm;

	if(dest_time==NULL)	return -1;
	rtc_time_to_tm(start_time.tv_sec+TIME_OFFSET,&tm);
	sprintf(dest_time,"%04d/%02d/%02d %02d:%02d\n",tm.tm_year+1900,tm.tm_mon
						,tm.tm_mday,tm.tm_hour,tm.tm_min);
	//printk(KERN_INFO "dest_time:%s,start_time->tv_sec=%ld\n",dest_time,start_time.tv_sec);
	return 0;
}

//get running time of process,convert time type to hour:min
static int get_run_time(char *dest_time,cputime_t run_time)
{
	int sec=run_time/HZ;
	int min=sec/60;

	sec=sec%60;
	sprintf(dest_time,"%d:%d",min,sec);
	//printk(KERN_INFO "cputime_t run_time=%ld\n ",run_time);
	return 0;
}

static int pid_single_open(struct inode *inode,struct file *file)
{
	return single_open(file,pid_seq_show,NULL);
}
static int pid_seq_show(struct seq_file *p,void *v)
{
	int i;
	for(i=1;i<PROCESS_NUMBER;i++)
	{
		if(pid_table[i].pid==0)	continue;//no such process
		//PID PNAME PPID PPNAME STAT TIME NEW
		seq_printf(p,"%-5d %-16s %-5d %-16s %-2d %-16s %1d\n",
					pid_table[i].pid,pid_table[i].pname,
					pid_table[i].ppid,pid_table[i].ppname,
					pid_table[i].state,pid_table[i].time,
					(unsigned int)pid_new_table[i]);
	}
	return 0;
}

static int __init pid_module_start(void)
{
	int result;

	memset(pid_table,0,PROCESS_NUMBER*sizeof(pidinfo));
	printk(KERN_INFO "Pid Monitor start.\n");
	result = get_all_task_info();
	//create file /proc/pidmonitor
	entry=create_proc_entry("pidmonitor",0,NULL);
	if(entry)
		entry->proc_fops=&pid_proc_ops;
	//create sock
	nl_sk=netlink_kernel_create(&init_net,NETLINK_MYTEST,0,recv_message,NULL,THIS_MODULE);
	if(!nl_sk)
	{
		printk(KERN_EMERG "Failed to create netlink socket!\n");
	}
	return 0;
}

static void __exit pid_module_exit(void)
{
	printk(KERN_INFO "Pid Monitor exit.\n");
	if(entry)
		remove_proc_entry("pidmonitor",NULL);
	if(nl_sk)
		sock_release(nl_sk->sk_socket);
}

module_init(pid_module_start);
module_exit(pid_module_exit);

MODULE_AUTHOR("Peng Zhenyi");
MODULE_DESCRIPTION(" A process monitor.");
MODULE_LICENSE("Dual BSD/GPL");
