#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#define PROCESS_NUMBER 65535
int proc_pid[PROCESS_NUMBER];
int ps_pid[PROCESS_NUMBER];
int proc_number;
int ps_number;

#define NETLINK_MYTEST 28
#define MAX_PAYLOAD 4096
int sock_fd;

int hidepid[100];  //The pid of the hiden process
int pidcount[100];//how many times the hiden process appear,+1 every period,==2 indicates the process is a hiden process
int pidtag[100];  //indicate whether is hiden process is still running(1)
int pidnumber=0;//how many hiden process are found currently

void signal_exit(int signal_no)
{
	if(sock_fd)
		close(sock_fd);
	printf("program exit.\n");
	exit(1);
}

int report_hide_process()
{//print the hiden process
	int i=0,j=0;
	int k=0;
	char command[200];
	for(k=0;k<pidnumber;k++)
	{
		pidtag[k]=0;	
	}
	while((i<proc_number)&&(j<ps_number))
	{
		if(proc_pid[i]==ps_pid[j])
		{
			i++;j++;
		}
		else
		{
			while(proc_pid[i]>ps_pid[j]) j++;
			if(proc_pid[i]!=ps_pid[j])
			{
				//printf("Find hiden process:%d\n",proc_pid[i]);
				if(pidnumber==0)
				{
					pidnumber++;
					hidepid[0]=proc_pid[i];
					pidcount[0]=1;
					pidtag[0]=1;
				}
				else
				{
					//is the process appeared before
					for(k=0;k<pidnumber;k++)
					{
						if(hidepid[k]==proc_pid[i]) break;
					}
					if((k<pidnumber)&&(pidcount[k]==1))  //the hiden process has appeared once
					{
						pidtag[k]=1;
						//printf("Find hiden process:%d\n",proc_pid[i]);
						memset(command,'\0',200);
						sprintf(command,"cat /proc/pidmonitor |awk '$1==%d {print \"process:\" $1 \" name:\" $2 \" father:\" $3 \" fname:\" $4}'",proc_pid[i]);
						system(command);
						pidcount[k]++;
					}
					else if((k<pidnumber)&&(pidcount[k]!=1))
					{
						pidtag[k]=1;
					}
					else// a new hiden process found
					{
						hidepid[pidnumber]=proc_pid[i];
						pidcount[pidnumber]=1;
						pidtag[pidnumber]=1;
						pidnumber++;
					}
				}
				i++;
			}
			while(proc_pid[i]>ps_pid[j]) j++;
		}
	}
	//Is there any process exit?
	for(k=0;k<pidnumber;k++)
	{
		if(pidtag[k]==0)
		{
			printf("Hiden process %d exit.\n",hidepid[k]);
			pidnumber--;
			for(;k<pidnumber;k++)
			{
				hidepid[k]=hidepid[k+1];
			}	
			break;
		}
	}
	return 0;
}

void *read_proc(void *args)
{
	int fd;
	char c,buffer[10];
	int i,j;
	fd=open("/tmp/proc_pid",O_RDONLY);
	if(fd<=0)	return NULL;

	i=j=0;
	memset(buffer,'\0',10);
	while(read(fd,&c,1)==1)
	{
		if(c==EOF) break;
		if(c=='\n')
		{
			i=0;
			proc_pid[j]=atoi(buffer);
			//printf("proc_pid[%d]=%d\n",j,proc_pid[j]);
			memset(buffer,'\0',10);
			j++;
			continue;
		}
		buffer[i]=c;
		i++;
	}
	close(fd);
	*(int *)args=j;
	return NULL;
}
void *read_ps(void *args)
{
	int fd;
	char c,buffer[10];
	int i,j;
	fd=open("/tmp/ps_pid",O_RDONLY);
	if(fd<=0)	return NULL;

	i=j=0;
	memset(buffer,'\0',10);
	while(read(fd,&c,1)==1)
	{
		if(c==EOF) break;
		if(c=='\n')
		{
			i=0;
			ps_pid[j]=atoi(buffer);
			//printf("ps_pid[%d]=%d\n",j,ps_pid[j]);
			memset(buffer,'\0',10);
			j++;
			continue;
		}
		buffer[i]=c;
		i++;
	}
	close(fd);
	*(int *)args=j;
	return NULL;
}

int main()
{
	pthread_t thread1,thread2;
	int rs;
	int retval,state_smg=0;
	struct sockaddr_nl src_addr,dest_addr;
	struct nlmsghdr *nlh=NULL;
	struct iovec iov;
	struct msghdr msg;

	signal(SIGINT,signal_exit);
	signal(SIGKILL,signal_exit);
	signal(SIGTERM,signal_exit);
	signal(SIGSTOP,signal_exit);
	signal(SIGTSTP,signal_exit);

	//create socket,and binding src address
	sock_fd=socket(AF_NETLINK,SOCK_RAW,NETLINK_MYTEST);
	if(sock_fd==-1)
	{
		printf("Create socket error.\n");
		return -1;
	}
	memset(&src_addr,0,sizeof(src_addr));
	src_addr.nl_family=AF_NETLINK;
	src_addr.nl_pid=getpid();
	src_addr.nl_groups=0;
	retval=bind(sock_fd,(struct sockaddr *)&src_addr,sizeof(src_addr));
	if(retval<0)
	{
		printf("Bind address error.\n");
		close(sock_fd);
		return -1;
	}

	//set dest_addr
	memset(&dest_addr,0,sizeof(dest_addr));
	dest_addr.nl_family=AF_NETLINK;
	dest_addr.nl_pid=0; //transfer to kernel
	dest_addr.nl_groups=0;

	//NLMSG_SPACE
	nlh=(struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	if(!nlh)
	{
		printf("Create nlh error.\n");
		close(sock_fd);
		return -1;

	}
	nlh->nlmsg_len=NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid=getpid();
	nlh->nlmsg_flags=0;
	strcpy(NLMSG_DATA(nlh),"update");

	//iov
	iov.iov_base=(void *)nlh;
	iov.iov_len=NLMSG_SPACE(MAX_PAYLOAD);
	//msg
	memset(&msg,0,sizeof(msg));
	msg.msg_name=(void *)&dest_addr;
	msg.msg_namelen=sizeof(dest_addr);
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	while(1){
	//send message
	state_smg=sendmsg(sock_fd,&msg,0);
	if(state_smg==-1)
	{
		printf("Send message error.\n");
	}

	//create two process information files
	system("ps aux|sed '1d'|sed '/ps aux/d'|awk '{print $2}' > /tmp/ps_pid");
	system("cat /proc/pidmonitor | awk '$7==1 {print $1}' > /tmp/proc_pid");
	
	//create two thread to read files
	rs=pthread_create(&thread1,NULL,read_proc,(void *)(&proc_number));
	if(rs)
	{
		printf("Error create thread1.\n");
	}
	rs=pthread_create(&thread2,NULL,read_ps,(void *)(&ps_number));
	if(rs)
	{
		printf("Error create thread2.\n");
	}

	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);

	//printf("finished reading files.\n");
	if(proc_number!=ps_number)	report_hide_process();
	sleep(10);	
	}//end of while

	return 0;
}
