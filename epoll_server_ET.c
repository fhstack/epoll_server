#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>

void setNonBlock(int fd)
{
    int fl = fcntl(fd,F_GETFL);

    if(fl < 0)
    {
        perror("fcntl");
        return;
    }

    fcntl(fd,F_SETFL,fl | O_NONBLOCK);
}

void my_accept(int listen_sock,int epfd)
{
    for(;;)
    {
        struct epoll_event ev;
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        //accpet立即返回
        int new_sock = accept(listen_sock,(struct sockaddr*)&client,\
                                  &len);
            
        if(new_sock < 0)
        {
            break;
        }
        //连接进来的描述符设为非阻塞
        setNonBlock(new_sock);
        printf("get a new link\n");

        //注意：先读后写
        ev.events = EPOLLIN;
        ev.data.fd = new_sock;
        epoll_ctl(epfd,EPOLL_CTL_ADD,new_sock,&ev);
    }
}

ssize_t my_read(int sock,char outBuf[], int size)
{
    ssize_t total = 0;
    ssize_t s = 0;
    while( (s = read(sock, outBuf+total , 128)) > 0 && s == 128 && total < size)
    {
        total += s;
    }

    outBuf[total] = 0;
    return total;
}
void handlerReadyEvents(int epfd,struct epoll_event revs[],int num,int listen_sock)
{ 
    int i = 0;
    for(;i < num;i++)
    {
        struct epoll_event ev;
        int sock = revs[i].data.fd;
        //时间类型
        uint32_t events = revs[i].events;
        
        if(sock == listen_sock && (events & EPOLLIN))
        {
            my_accept(listen_sock,epfd);
        }
        else if(events & EPOLLIN)
        {
            //事实上这样可能读不完
            //目前我们先这样处理
            //read event ready
            char buf[10240];
            ssize_t s = my_read(sock,buf,sizeof(buf)-1);
            //我们假定一次就读完了对方的报文
            if(s > 0)
            {
                buf[s] = 0;
                printf("%s",buf);//read OK
                ev.events = EPOLLOUT|EPOLLET;
                ev.data.fd = sock;
                epoll_ctl(epfd,EPOLL_CTL_MOD,sock,&ev);
            }
            else if(s == 0)
            {
                //client close link
                close(sock);
                printf("client quit...!\n");
            
                epoll_ctl(epfd, EPOLL_CTL_DEL,sock,NULL);
            }
            else
            {
                perror("read");
            }
        }
        else if(events & EPOLLOUT)
        {
            //write event ready
            //printf("write event ready\n");
         
            const char* echo_http = "HTTP/1.0 200 OK\r\n\r\n<html><h1>hello Epoll Server!</h1></html>\r\n";
 
            //my_write(sock,echo_http,strlen(echo_http));
            write(sock,echo_http,strlen(echo_http));
            close(sock);
            epoll_ctl(epfd, EPOLL_CTL_DEL,sock, NULL);
        }
        else
        {
            perror("bug\n");
        }
    }
}

int startup(int port)
{
    int listen_sock = socket(AF_INET,SOCK_STREAM,0);
    if(listen_sock < 0)
    {
        perror("socket");
        exit(2);
    }

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);

    int opt = 1;
    setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    setNonBlock(listen_sock);

    if(bind(listen_sock,(struct sockaddr*)&local,sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
        exit(3);
    }

    if(listen(listen_sock,5))
    {
        perror("listen");
        exit(4);
    }

    return listen_sock;
}

int main(int argc,char* argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s [port]",argv[0]);
        return 1;
    }

    int listen_sock = startup(atoi(argv[1]));

    int epfd = epoll_create(256);

    if(epfd < 0)
    {
        perror("epoll_create");
        return 5;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN|EPOLLET;
    //首先关注监听套接字
    ev.data.fd = listen_sock;
    epoll_ctl(epfd,EPOLL_CTL_ADD,listen_sock,&ev);
    struct epoll_event revs[64];
    for(;;)
    {
        int timeout = 1000;
        int num = epoll_wait(epfd,revs,sizeof(revs)/sizeof(revs[0]),timeout);
        
        switch(num)
        {
            case -1:
                printf("epoll_wait error\n");
                break;
            case 0:
                printf("timeout...\n");
                break;
            default:
                handlerReadyEvents(epfd,revs,num,listen_sock);
                break;
        }
    }
}
