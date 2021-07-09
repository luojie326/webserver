#pragma once

#include<vector>
#include<sys/epoll.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
using namespace std;


class Epoll {
public:
	Epoll();                              //创建epoll，epoll_create
	~Epoll();
	void epoll_add(int fd, bool one_shot);               //注册新的描述符到epoll
	void epoll_del(int fd);               //从epoll中删除描述符
	void epoll_mod(int fd,int ev);               //修改已经注册的监听事件
	int epoll_wat(int timeout = -1);      //等待监听事件发生
	int get_event_fd(int i) const;        //获取被监听的描述符
	uint32_t get_events(int i) const;     //获取被监听的事件
	
private:
	static const int MAXEVENT = 100000;     //epoll的最多可监听的文件描述符个数
	int epfd;                             //epoll句柄
	struct epoll_event ep_events[MAXEVENT]; //epoll监听的事件
};