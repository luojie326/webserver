
#include"server.h"
#include<signal.h> 
#include<cassert>

void addsig(int sig, void(handler)(int), bool restart = true) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}
int main() {
	int argc = 3;
	const char* argv[] = { "0","8999","/home/luojie/Documents" };
	if (argc < 3) {
		printf("eg: ./a.out port path\n");
		exit(1);
	}
	int port = atoi(argv[1]);
	int ret = chdir(argv[2]);
	if (ret == -1) {
		perror("chdir error");
		exit(1);
	}
	addsig(SIGPIPE, SIG_IGN);
	
	//启动server
	Server server(port);
	server.server_run();
	return 0;
}