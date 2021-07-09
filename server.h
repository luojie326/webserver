#pragma once

#include"epoll.h"
#include"http_conn.h"
#include"threadpool.h"

class Server {
public:
	Server(int port);
	~Server();
	void server_run();                                  //����server

private:
	void init_listen_fd();                              //��ʼ������socket������
	bool do_accept();                                   //������������
	void show_error(int cfd, const char* info);

private:
	static const int MAXFDS = 65536;                   //����ļ�������
	int lfd;                                            //����������
	int port;                                           //server�Ķ˿�
	Epoll epoller;					        //epoll����
	Threadpool<HttpConn>* pool;//�̳߳�
	HttpConn* users;
	int user_count;
	static const char* ip;
	
};