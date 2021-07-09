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
	bool append(T* request);//����������������

private:
	static void* worker(void* arg);//�����߳����еĺ����������ϴ����������ȡ������ִ��
	void run();

private:
	int thread_num;//�̳߳��е��߳���
	int max_request;//���������󳤶�
	pthread_t* threads;//����̳߳��������߳�ID
	std::list<T*> requests;//�������
	Lock req_locker;//����������еĻ�����
	Sem req_sem;//����������Ƿ���������Ҫ����
	bool stop; //������߳�

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

	//����thread_num���̣߳�������Ϊ�̷߳���
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