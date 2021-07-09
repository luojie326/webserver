
#include"epoll.h"
#include<sys/errno.h>

Epoll::Epoll() {
	epfd = epoll_create(5);
	if (epfd == -1) {
		perror("epoll_create error");
		exit(1);
	}
}

Epoll::~Epoll(){
	printf("关闭epoll描述符\n");
	close(epfd);
}

void Epoll::epoll_add(int fd, bool one_shot) {
	// 设置fd为非阻塞
	int flag = fcntl(fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flag);

	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;        //读事件|边缘触发模式|对端是否关闭
	if (one_shot) {
		ev.events |= EPOLLONESHOT; //保证一个socket连接任一时刻只能被一个线程处理
	}
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	if (ret == -1) {
		perror("epoll_ctl add fd error");
	}
}

void Epoll::epoll_del(int fd) {
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	if (ret == -1) {
		perror("epoll delete error");
	}
}

void Epoll::epoll_mod(int fd,int event){
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = event | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	if (ret == -1) {
		perror("epoll_ctl mod fd error");
	}

}

int Epoll::epoll_wat(int timeout) {
	//printf("epoll_wait\n");
	int ret = epoll_wait(epfd, ep_events, MAXEVENT, timeout=-1);
	//printf("epoll_wait return %d\n",ret);
	if (ret == -1 && errno!=EINTR) {
		perror("epoll wait error");
	}
	return ret;
}

int Epoll::get_event_fd(int i) const {
	return ep_events[i].data.fd;
}

uint32_t Epoll::get_events(int i) const {
	return ep_events[i].events;
}
