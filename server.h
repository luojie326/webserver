#pragma once

#include"epoll.h"
#include"http_conn.h"
#include"threadpool.h"

class Server {
public:
	Server(int port);
	~Server();
	void server_run();                                  //运行server

private:
	void init_listen_fd();                              //初始化监听socket描述符
	bool do_accept();                                   //处理连接请求
	void show_error(int cfd, const char* info);

private:
	static const int MAXFDS = 65536;                   //最大文件描述符
	int lfd;                                            //监听描述符
	int port;                                           //server的端口
	Epoll epoller;					        //epoll对象
	Threadpool<HttpConn>* pool;//线程池
	HttpConn* users;
	int user_count;
	static const char* ip;
	
};