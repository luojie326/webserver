#pragma once

#include"epoll.h"
#include<memory>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/stat.h>
#include<dirent.h>
#include<ctype.h>
#include <sys/mman.h> 
#include <sys/uio.h>
#include<cstdarg>
using namespace std;


class HttpConn {

public:
	static const int FILENAME_LEN = 200;//文件完整路径最大长度
	static const int READBUF_SIZE = 2048;//读缓冲区大小
	static const int WRITEBUF_SIZE = 1024;//写缓冲区大小
	enum METHOD {
		GET = 0, POST, HEAD, PUT, DELETE,  //http请求方法
		TRACE, OPTIONS, CONNECT, PATCH
	};
	//解析客户请求时，主状态机所处状态
	enum CHECK_STATE {
		CHECK_STATE_REQUESTLINE = 0,  //正在分析请求行
		CHECK_STATE_HEADER,         //正在分析头部字段
		CHECK_STATE_CONTENT
	};
	//从状态机：行的读取状态
	enum LINE_STATUS {
		LINE_OK = 0, //读取到一个完整的行
		LINE_BAD,    //行出错
		LINE_OPEN
	};  //行数据尚不完整

//服务器处理http请求的可能结果
	enum HTTP_CODE {
		NO_REQUEST,//请求不完整，需要继续读取客户数据
		GET_REQUEST,//获得了一个完整的客户请求
		BAD_REQUEST,//客户请求有语法错误
		NO_RESOURCE,//文件或目录不存在
		FILE_REQUEST,//请求文件
		DIR_REQUEST,//请求目录
		FORBIDDEN_REQUEST,//客户对资源没有足够的访问权限
		INTERNAL_ERROR,//服务器内部错误
		CLOSED_CONNECTION,//客户端已经关闭连接了

	};

	
	static Epoll* epoller;
	static int user_count;//用户数量

public:

	HttpConn() {};
	~HttpConn() {};
	
	void init(int fd,const sockaddr_in& addr);      //初始化新接受的连接
	void process();//处理客户请求
	bool read();                              //接受HTTP请求
	bool write();//发送http响应
	void disconnect();                           //断开连接

private:
	void init();//初始化连接

	HTTP_CODE process_read();//主状态机,解析HTTP请求
	LINE_STATUS parse_line();                           //从状态机,读行的状态
	char* get_line() { return read_buf + start_line; }
	HTTP_CODE parse_request_line(char* text);    //解析请求行
	HTTP_CODE parse_headers(char* text);//解析头部信息
	HTTP_CODE parse_content(char* text);//消息体

	HTTP_CODE do_request();//解析后处理请求

	bool process_write(HTTP_CODE ret);//写http响应
	void unmap();
	bool add_response(const char* format, ...);//向缓冲区写入
	bool add_status_line(int status, const char* title);//写状态行
	bool add_headers(int content_len);//写消息报头
	bool add_content(const char* content);//写响应正文
	void send_dir();        //发送目录
	void mydelete();
	void encode_str(char* to, int tosize, const char* from);
	void decode_str(char* to, char* from);
	const char* get_file_type(const char* name);
	int hexit(char c);


private:

	//该HTTP连接的socket描述符和地址
	int cfd;
	sockaddr_in address;

	char read_buf[READBUF_SIZE];//读缓冲区
	int read_idx;//标识读缓冲区已经读入的客户数据的最后一个字节的下一个位置
	int checked_idx;//当前正在分析的字符在读缓冲区中的位置
	int start_line;//当前正在解析的行的起始位置
	char write_buf[WRITEBUF_SIZE];//写缓冲区
	int write_idx;//写缓冲区中待发送的字节数

	CHECK_STATE check_state;//主状态机当前状态
	METHOD method;//http请求方法

	char real_file[FILENAME_LEN];//客户请求的目标文件完整路径
	char* url;//客户请求的目标文件名
	char* version;//HTTP协议版本号，我们仅支持HTTP/1.1
	char* host;//主机名
	int content_length;//http请求的消息体长度
	bool linger;//http请求是否要求保持连接

	char* file_address;//客户请求的目标文件被mmap到内存中的起始位置
	char* dir_address;
	int isdir;
	struct stat file_stat;//目标文件的状态
	struct iovec iv[2];
	int iv_count;
	
};
