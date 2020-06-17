/*
gcc -o epoll_et_noblock epoll_et_noblock.c

et模式，同一个fd，如果这次的读事件没有读完，不会继续触发epoll_wait
而是等待下一次事件触发继续读
所以使用et模式必须保证读完（读到EAGAIN为止）
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>

int tcp_socket(int port);

static void sp_nonblocking(int fd);

int main(int argc,char *argv[]){
	if(argc != 2)
	{
		printf("Usage : %s port\n",argv[0]);
		return 1;
	}
	int lfd = tcp_socket(atoi(argv[1]));
	assert(lfd > 0);

	sp_nonblocking(lfd);

	int efd = epoll_create(1);
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN | EPOLLET;
	epoll_ctl(efd,EPOLL_CTL_ADD,lfd,&ev);

	struct epoll_event event[1024];

	struct sockaddr_in cli_addr;
	bzero(&cli_addr,sizeof(cli_addr));
	socklen_t len = sizeof(cli_addr);

	int ret;
	char buf[1024];
	char*ptr;

	for(;;)
	{
		int r = epoll_wait(efd,event,1024,-1);
		for(int i = 0; i<r; i++)
		{
			int fd = event[i].data.fd;
			if(fd == lfd && event[i].events & EPOLLIN)
			{
				//printf("fd == lfd && event[i].event & EPOLLIN\n");
				int cfd = accept(lfd,(struct sockaddr*)&cli_addr,&len);
				assert(cfd > 0);

				sp_nonblocking(cfd);

				ev.data.fd = cfd;
				ev.events = EPOLLIN | EPOLLET;
				epoll_ctl(efd,EPOLL_CTL_ADD,cfd,&ev); 

				printf("new client fd = %d from %s:%d\n",cfd,inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
			}
			else if(event[i].events & EPOLLIN)
			{
				memset(buf,0,sizeof(buf));    
				printf("event[i].event & EPOLLIN\n");

				ptr = buf;

				for(;;)
				{
					ret = recv(fd, ptr, 5, 0);//test
					if(ret > 0)
					{
						ptr += ret;
					}
					else if(ret == 0)
					{
						epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL); 
						close(fd);
						printf("closed client fd:%d\n",fd);
					}
					else if(ret < 0)
					{
						if(errno == EAGAIN)
						{
							break;
						}
						else
						{
							fprintf(stderr,"recv error");
							//exit(1);
							epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL); 
							close(fd);
							printf("closed client fd:%d\n",fd);
							break;
						}
					}
				}
				ev.data.fd = fd;
				ev.events &= (~EPOLLIN);
				ev.events |= EPOLLOUT;

				epoll_ctl(efd,EPOLL_CTL_MOD,fd,&ev); 

				printf("count %d\n",ptr - buf);
			}
			else if(event[i].events & EPOLLOUT)
			{
				printf("event[i].event & EPOLLOUT\n");
				//
				int count = ptr - buf;
				//printf("count %d\n",count);
				ptr = buf;
				while(count > 0)
				{
					ret = send(fd,ptr,5,0);
					ptr += ret;
					count -= ret;
				}
				ev.data.fd = fd;
				ev.events &= (~EPOLLOUT);
				ev.events |= EPOLLIN;
				//
				epoll_ctl(efd,EPOLL_CTL_MOD,fd,&ev); 
			}
		}
	}

	close(lfd);
	close(efd);
	return 0;
}
int tcp_socket(int port){
	int sock;
	struct sockaddr_in addr;
	memset(&addr,0,sizeof addr);
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	addr.sin_port=htons(port);

	sock=socket(AF_INET,SOCK_STREAM,0);
	const int on=1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof on))
	{
		printf("setsockopt\n");
		return -1;
	}
	int r = bind(sock,(struct sockaddr*)&addr,sizeof(addr));
	if(r == -1)
		return -1;
	r=listen(sock,10);
	if(r == -1)
		return -1;
	return sock;
}
static void
	sp_nonblocking(int fd) {
		int flag = fcntl(fd, F_GETFL, 0);
		if ( -1 == flag ) {
			return;
		}

		fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

