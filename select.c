/*
    gcc -o select select.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int tcp_socket(int port);

int main(int argc,char *argv[])
{
	if(argc != 2)
	{
		printf("Usage : %s port\n",argv[0]);
		return 1;
	}
	int sock = tcp_socket( atoi(argv[1]) );
	assert(sock > 0);

	int r;
	fd_set reads, temp;//reads为文件描述符集合，temp为临时集合
	
	int fd_max,i;
	struct timeval timeout;//时间结构体
	char buf[1024];

	FD_ZERO(&reads);//清空reads集合
	FD_SET(sock,&reads); //监听描述符sock加入集合reads
	fd_max = sock; //文件描述符数量为sock,sock数值为3，因为012为stdin,stdout,stderr
	
    struct sockaddr_in cli_addr;
    bzero(&cli_addr,sizeof(cli_addr));
    socklen_t len = sizeof(cli_addr);
	while(1)
	{
        // 原来为1的位置都变为0，但发生变化的文件描述符不变，所以要临时保存集合
		temp=reads;//temp临时存储集合，因为如果reads某个位置有变化，

		timeout.tv_sec=5; //设置超时时间5s
		timeout.tv_usec=0;
		//printf("fd_max = %d\n", fd_max);
		r = select(fd_max+1, &temp, NULL, NULL, &timeout);  //调用select函数，对临时集合temp监听
		if(r == -1)
			break;
		else if(r == 0)	//返回0，超时
		{
			printf("time out! 5s\n");
		    continue;
		}
		else
		{
            //0 1 2 已被系统使用
			for(i = 3; i < fd_max+1; i++) //从下标为3的集合位置开始
			{
				if(FD_ISSET(i,&temp))//如果这个位置有变化
				{
					if(i == sock)//有新客户端加入
					{
						len=sizeof(cli_addr);
						int cli_sock=accept(sock,(struct sockaddr*)&cli_addr,&len);
						if(cli_sock == -1)
							continue;
						FD_SET(cli_sock,&reads);//在reads集合加入新文件描述符
						if(fd_max < cli_sock)
							fd_max = cli_sock;  //如果最大数量有变
						printf("new client fd = %d from %s:%d\n",cli_sock,inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
					}
					else{//有客户消息发送
					
						memset(buf,0,1024);
						r = read(i,buf,1024-1);
						if(r == 0) //客户端断开链接
						{
							FD_CLR(i,&reads);////在reads集合删除文件描述符
							close(i);
							printf("closed client fd:%d\n",i);
						}
						else if(r > 0){
							//printf("client %d:",i);
							write(i,buf,r);//写回客户端
							buf[r]='\0';
							printf("recv from client %d: %s\n",i,buf);
						}
						else {
							printf("read error\n");
							exit(1);
						}
					}
				}
			}
		}
	}
	close(sock);
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
	//在bind前，设置端口复用
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

