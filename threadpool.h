#pragma once

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>

#include"lock.h"

template<typename T>
class Threadpool{
public:
	Threadpool(int thread_num = 8, int max_request = 100000);
	~Threadpool();
	bool append(T* request);//往请求队列添加任务

private:
	static void* worker(void* arg);//工作线程运行的函数，它不断从请求队列中取出任务并执行
	void run();

private:
	int thread_num;//线程池中的线程数
	int max_request;//请求队列最大长度
	pthread_t* threads;//存放线程池中所有线程ID
	std::list<T*> requests;//请求队列
	Lock req_locker;//保护请求队列的互斥锁
	Sem req_sem;//请求队列中是否有任务需要处理
	bool stop; //否结束线程

};

template<typename T>
Threadpool<T>::Threadpool(int thread_num, int max_request) :
	thread_num(thread_num), max_request(max_request),stop(false),threads(NULL) {

	if ((thread_num <= 0) || (max_request <= 0)) {
		throw std::exception();
	}

	threads = new pthread_t[thread_num];
	if (!threads) {
		throw std::exception();
	}

	//创建thread_num个线程，并设置为线程分离
	for (int i = 0; i < thread_num; i++) {
		printf("create the %dth thread\n", i);
		if (pthread_create(threads + i, NULL, worker, this) != 0) {
			delete[] threads;
			throw std::exception();
		}

		if (pthread_detach(threads[i])){
			delete[] threads;
			throw std::exception();
		}
	}
}

template<typename T>
Threadpool<T>::~Threadpool() {
	delete[] threads;
	stop = true;
}

template<typename T>
bool Threadpool<T>::append(T* request) {
	req_locker.lock();
	if (requests.size() >= max_request) {
		req_locker.unlock();
		return false;
	}
	requests.push_back(request);
	req_locker.unlock();
	req_sem.post();
	return true;
}

template<typename T>
void* Threadpool<T>::worker(void* arg) {
	Threadpool* pool = (Threadpool*)arg;
	pool->run();
	return pool;
}


template<typename T>
void Threadpool<T>::run() {
	while (!stop) {
		req_sem.wait();
		req_locker.lock();
		if (requests.empty()) {
			req_locker.unlock();
			continue;
		}
		T* request = requests.front();
		requests.pop_front();
		req_locker.unlock();
		if (!request) {
			continue;
		}
		request->process();
	}
}