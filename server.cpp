
#include"server.h"
#include <cassert>

const char* Server::ip = "10.15.20.231";

Server::Server(int port):port(port) {
	try {
		pool = new Threadpool<HttpConn>;
	}
	catch (...) {
		perror("new Threadpool error");
		exit(1);
	}
	
	users = new HttpConn[MAXFDS];
	assert(users);
	user_count = 0;
	
}

Server::~Server() {
	printf("关闭监听描述符");
	close(lfd);
	delete[] users;
	delete pool;
}

void Server::server_run() {

	//得到负责监听的lfd并加入epoll
	init_listen_fd();

	while (1) {
		//printf("进入循环.....\n");
		int number = epoller.epoll_wat();
		if ((number < 0) && (errno != EINTR)) {
			break;
		}
		for (int i = 0; i < number; i++) {
			int fd = epoller.get_event_fd(i);
			if (fd == lfd) {
				//接收连接请求
				bool ret = do_accept();
				if (!ret) {
					continue;
				}
			}
			else if (epoller.get_events(i) & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				users[fd].disconnect();
			}
			else if(epoller.get_events(i) & EPOLLIN) {
				//读数据
				if (users[fd].read()) {
					pool->append(users + fd);
				}
				else {
					users[fd].disconnect();
				}
			}
			else if (epoller.get_events(i) & EPOLLOUT) {
				if (!users[fd].write()) {
					printf("不保持连接...");
					users[fd].disconnect();
				}
				
				
			}
			else{}
		}
	}
}

void Server::init_listen_fd() {

	lfd = socket(PF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}
	struct linger tmp = { 1, 1 };
	setsockopt(lfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);
	//serv.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET, ip, &serv.sin_addr);

	//设置端口复用
	int op = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));

	int ret = bind(lfd, (struct sockaddr*)&serv, sizeof(serv));
	if (ret == -1) {
		perror("bind error");
		exit(1);
	}

	ret = listen(lfd, 5);
	if (ret == -1) {
		perror("listen error");
		exit(1);
	}

	epoller.epoll_add(lfd,false);
	HttpConn::epoller = &epoller;
}

bool Server::do_accept() {
	struct sockaddr_in clie;
	socklen_t clie_len = sizeof(clie);
	int cfd;
	//循环调用accpet，因为ET模式下当多个连接到达时只会触发一次listenfd可读事件
	while (true)
	{
		cfd = accept(lfd, (sockaddr*)&clie, &clie_len);
		if (cfd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
			{
				perror("accept error");
				return false;
			}
		}
		if (HttpConn::user_count >= MAXFDS) {
			show_error(cfd, "Internal server busy");
			return false;
		}
		users[cfd].init(cfd, clie);

		char ip[64] = { 0 };
		printf("New Client IP: %s, Port: %d, cfd = %d\n",
			inet_ntop(AF_INET, &clie.sin_addr.s_addr, ip, sizeof(ip)),
			ntohs(clie.sin_port), cfd);
	}

	return true;
}

void Server::show_error(int cfd, const char* info) {
	printf("%s\n", info);
	send(cfd, info, strlen(info), 0);
	printf("关闭连接描述符: %d\n",cfd);
	close(cfd);
}