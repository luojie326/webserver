#pragma once

#include<exception>
#include<pthread.h>
#include<semaphore.h>

//封装信号量的类
class Sem {
public:
	Sem() {
		if (sem_init(&sem, 0, 0) != 0) {
			throw std::exception();
		}
	}
	Sem(int n) {
		if (sem_init(&sem, 0, n) != 0) {
			throw std::exception();
		}
	}
	~Sem() {
		sem_destroy(&sem);
	}
	int wait() {
		return sem_wait(&sem);
	}
	int post() {
		return sem_post(&sem);
	}

private:
	sem_t sem;
};

//封装互斥锁的类
class Lock {
public:
	Lock() {
		if (pthread_mutex_init(&mutex, NULL) != 0) {
			throw std::exception();
		}
	}
	~Lock() {
		pthread_mutex_destroy(&mutex);
	}
	int lock() {
		return pthread_mutex_lock(&mutex);
	}
	int unlock() {
		return pthread_mutex_unlock(&mutex);
	}

private:
	pthread_mutex_t mutex;
};

//封装条件变量的类
class Cond {
public:
	Cond() {
		if (pthread_mutex_init(&mutex, NULL) != 0) {
			throw std::exception();
		}
		if (pthread_cond_init(&cond, NULL) != 0) {
			pthread_mutex_destroy(&mutex);
			throw std::exception();
		}
	}
	~Cond() {
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
	}
	int wait() {
		int ret = 0;
		pthread_mutex_lock(&mutex);
		ret = pthread_cond_wait(&cond, &mutex);
		pthread_mutex_unlock(&mutex);
		return ret == 0;
	}

	int signal() {
		return pthread_cond_signal(&cond);
	}

private:
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};