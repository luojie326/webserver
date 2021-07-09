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
	Epoll();                              //����epoll��epoll_create
	~Epoll();
	void epoll_add(int fd, bool one_shot);               //ע���µ���������epoll
	void epoll_del(int fd);               //��epoll��ɾ��������
	void epoll_mod(int fd,int ev);               //�޸��Ѿ�ע��ļ����¼�
	int epoll_wat(int timeout = -1);      //�ȴ������¼�����
	int get_event_fd(int i) const;        //��ȡ��������������
	uint32_t get_events(int i) const;     //��ȡ���������¼�
	
private:
	static const int MAXEVENT = 100000;     //epoll�����ɼ������ļ�����������
	int epfd;                             //epoll���
	struct epoll_event ep_events[MAXEVENT]; //epoll�������¼�
};