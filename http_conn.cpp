
#include"http_conn.h"
#include <cassert>
#include"epoll.h"

//定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char* root = "/home/luojie/Documents";//根目录


//要定义，否则链接不到
int HttpConn::user_count = 0;
Epoll* HttpConn::epoller = NULL;

void HttpConn::init(int fd, const sockaddr_in& addr) {
	cfd = fd;
	address = addr;
	// 以下两行为了避免TIME_WAIT状态
	int reuse = 1;
	setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	epoller->epoll_add(cfd,true);
	user_count++;
	init();
}

void HttpConn::init() {
	check_state = CHECK_STATE_REQUESTLINE;
	linger = false;
	method = GET;
	url = 0;
	version = 0;
	content_length = 0;
	host = 0;
	start_line = 0;
	checked_idx = 0;
	read_idx = 0;
	write_idx = 0;
	isdir = 0;
	memset(read_buf, '\0', READBUF_SIZE);
	memset(write_buf, '\0', WRITEBUF_SIZE);
	memset(real_file, '\0', FILENAME_LEN);
}



void HttpConn::disconnect() {
	if (cfd != -1) {
		HttpConn::epoller->epoll_del(cfd);
		printf("关闭连接描述符: %d\n",cfd);
		shutdown(cfd, 2);
		close(cfd);
		user_count--;
	}
	
}

//从状态机 = > 得到行的读取状态，分别表示读取一个完整的行，行出错，行的数据尚且不完整
HttpConn::LINE_STATUS HttpConn::parse_line() {
	char temp;
	for (; checked_idx < read_idx; checked_idx++) {
		temp = read_buf[checked_idx];
		if (temp == '\r') {
			if ((checked_idx + 1) == read_idx) {//'\r'为读缓冲区最后一个字符，说明没有读到完整的行
				return LINE_OPEN;
			}
			else if (read_buf[checked_idx + 1] == '\n') {//下一个字符是\n，说明读取到完整的行
				read_buf[checked_idx++] = '\0';
				read_buf[checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;//否则HTTP请求存在语法错误
		}
		else if (temp == '\n') {
			if ((checked_idx > 1) && (read_buf[checked_idx - 1] == '\r')) {
				read_buf[checked_idx - 1] = '\0';
				read_buf[checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

// 解析HTTP请求行，获得请求方法, 目标URL，以及HTTP版本号
HttpConn::HTTP_CODE HttpConn::parse_request_line(char* text) {

	url = strpbrk(text," \t");
	if (!url) {
		return BAD_REQUEST;
	}
	*url++ = '\0';

	char* temp = text;
	if (strcasecmp(temp, "GET") == 0) {
		method = GET;
	}
	else {
		return BAD_REQUEST;
	}
	url += strspn(url, " \t");
	version = strpbrk(url, " \t");
	if (!version) {
		return BAD_REQUEST;
	}
	*version++ = '\0';
	version += strspn(version, " \t");
	if (strcasecmp(version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}
	if (strncasecmp(url, "http://", 7) == 0) {
		url += 7;
		url = strchr(url, '/');
	}
	if (url[0] != '/') {
		return BAD_REQUEST;
	}
	 //解码 %23 %34 %5f
	decode_str(url, url);


	check_state = CHECK_STATE_HEADER;//状态变为解析头部
	return NO_REQUEST;

}


//分析头部字段
//HTTP请求的组成是一个请求行，后面跟随0个或者多个请求头，最后跟随一个空的文本行来终止报头列表
HttpConn::HTTP_CODE HttpConn::parse_headers(char* text) {
	//遇到空行，表示头部字段解析完毕
	if (text[0] == '\0') {
		if (content_length != 0) {//有消息体，需要读取content_length字节的消息体
			check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	//处理Connection头部字段
	else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0) {
			linger = true;
		}
	}
	//处理Content-Length头部字段
	else if (strncasecmp(text, "Content-Length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		content_length = atol(text);
	}
	else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 15;
		text += strspn(text, " \t");
		host = text;
	}
	else {
		//printf("oop! unknow header %s\n", text);
	}
	return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parse_content(char* text) {
	//判断消息体是否被完整读入
	if (read_idx >= (content_length + checked_idx)) {
		text[content_length] = '\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

//循环读取客户数据，直到无数据可读或者对方关闭连接
bool HttpConn::read() {

	if (read_idx >= READBUF_SIZE) {
		return false;
	}
	int bytes_read = 0;
	while (true) {
		bytes_read = recv(cfd, read_buf + read_idx, READBUF_SIZE - read_idx, 0);
		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {//没有数据可读
				break;
			}
			
			return false;
		}
		else if (bytes_read == 0) {
			printf("客户端断开了连接....");
			return false;
		}
		read_idx += bytes_read;
	}
	return true;
}


HttpConn::HTTP_CODE HttpConn::process_read() {
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;
	while (((check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
		|| ((line_status = parse_line()) == LINE_OK)) {

		text = get_line();
		start_line = checked_idx;
		printf("got 1 http lines: %s\n", text);
		switch (check_state) {
			case CHECK_STATE_REQUESTLINE: {
				ret = parse_request_line(text);
				if (ret == BAD_REQUEST) {
					return BAD_REQUEST;
				}
				break;
			}
			case CHECK_STATE_HEADER: {
				ret = parse_headers(text);
				if (ret == BAD_REQUEST) {
					return BAD_REQUEST;
				}
				else if (ret == GET_REQUEST) {
					return do_request();
				}
				break;
			}
			case CHECK_STATE_CONTENT: {
				ret = parse_content(text);
				if (ret == GET_REQUEST) {
					return do_request();
				}
				line_status = LINE_OPEN;
				break;
			}
			default: {
				return INTERNAL_ERROR;
			}
		}

	}
	return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::do_request() {
	strcpy(real_file, root);
	int len = strlen(root);
	strncpy(real_file + len, url, FILENAME_LEN - len - 1);
	printf("请求资源：%s\n",real_file);
	if (stat(real_file, &file_stat) < 0) {
		return NO_RESOURCE;
	}
	if (!(file_stat.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}
	if (S_ISDIR(file_stat.st_mode)) {
		dir_address = new char[2048];
		assert(dir_address);
		isdir = 1;
		return DIR_REQUEST;
	}
	//把文件映射到file_address处
	int fd = open(real_file, O_RDONLY);
	file_address = (char*)mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

void HttpConn::mydelete()
{
	if (dir_address&&isdir)
	{
		//printf("delete diraddress\n");
		delete[] dir_address;
		dir_address = 0;
		isdir = 0;
	}
}

void HttpConn::unmap()
{
	if (file_address)
	{
		//printf("unmap file_address\n");
		munmap(file_address, file_stat.st_size);
		file_address = 0;
	}
}

bool HttpConn::write()
{
	int temp = 0;
	int bytes_have_send = 0;
	int bytes_to_send = write_idx;
	if (bytes_to_send == 0)
	{
		epoller->epoll_mod(cfd,EPOLLIN);
		init();
		return true;
	}

	while (1)
	{
		temp = writev(cfd, iv, iv_count);
		//printf("发送了%d字节数据\n", temp);
		if (temp <= -1)
		{
			if (errno == EAGAIN)//写缓冲没有空间，等待下一轮EPOLLOUT事件
			{
				epoller->epoll_mod(cfd, EPOLLOUT);
				return true;
			}

			unmap();
			mydelete();
			
			return false;
		}

		bytes_have_send += temp;
		bytes_to_send -= temp;
	/*	if (bytes_have_send >= iv[0].iov_len)
		{
			iv[0].iov_len = 0;
			iv[1].iov_base = file_address + (bytes_have_send - write_idx);
			iv[1].iov_len = bytes_to_send;
		}
		else
		{
			iv[0].iov_base = write_buf + bytes_have_send;
			iv[0].iov_len = iv[0].iov_len - bytes_have_send;
		}*/

		if (bytes_to_send <= 0)//发送http响应成功
		{
			unmap();
			mydelete();
			
			epoller->epoll_mod(cfd, EPOLLIN);
			printf("发送http响应成功\n");
			if (linger)//是否保持连接
			{
				init();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}
bool HttpConn::add_response(const char* format, ...)
{
	if (write_idx >= WRITEBUF_SIZE)
		return false;
	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(write_buf + write_idx, WRITEBUF_SIZE - 1 - write_idx, format, arg_list);
	if (len >= (WRITEBUF_SIZE - 1 - write_idx))
	{
		va_end(arg_list);
		return false;
	}
	write_idx += len;
	va_end(arg_list);
	
	return true;
}
bool HttpConn::add_status_line(int status, const char* title)
{
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool HttpConn::add_headers(int content_len)
{
	return add_response("Content-Type:%s\r\n", get_file_type(real_file)) &&
	add_response("Content-Length:%d\r\n", content_len) &&
	add_response("Connection:%s\r\n", (linger == true) ? "keep-alive" : "close") &&
	add_response("%s", "\r\n");
}

bool HttpConn::add_content(const char* content)
{
	return add_response("%s", content);
}
bool HttpConn::process_write(HTTP_CODE ret)
{
	switch (ret)
	{
	case INTERNAL_ERROR:
	{
		add_status_line(500, error_500_title);
		add_headers(strlen(error_500_form));
		if (!add_content(error_500_form))
			return false;
		break;
	}
	case BAD_REQUEST:
	{
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	}
	case FORBIDDEN_REQUEST:
	{
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form))
			return false;
		break;
	}
	case FILE_REQUEST:
	{
		add_status_line(200, ok_200_title);
		if (file_stat.st_size != 0)
		{
			add_headers(file_stat.st_size);
			iv[0].iov_base = write_buf;
			iv[0].iov_len = write_idx;
			iv[1].iov_base = file_address;
			iv[1].iov_len = file_stat.st_size;
			iv_count = 2;
			return true;
		}
		else
		{
			const char* ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
				return false;
		}
	}
	case DIR_REQUEST: {
		add_status_line(200, ok_200_title);
		if (file_stat.st_size != 0)
		{
			send_dir();
			add_headers(strlen(dir_address));
			//printf("header长度%d, content长度%d\n", write_idx, strlen(dir_address));
			iv[0].iov_base = write_buf;
			iv[0].iov_len = write_idx;
			iv[1].iov_base = dir_address;
			iv[1].iov_len = strlen(dir_address);
			iv_count = 2;
			return true;
		}
		else
		{
			const char* ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
				return false;
		}
	}
	default:
		return false;
	}
	iv[0].iov_base = write_buf;
	iv[0].iov_len = write_idx;
	iv_count = 1;
	
	return true;
}
void HttpConn::process()
{
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST)
	{
		epoller->epoll_mod(cfd, EPOLLIN);
		return;
	}
	bool write_ret = process_write(read_ret);
	if (!write_ret)
	{
		disconnect();
	}
	epoller->epoll_mod(cfd, EPOLLOUT);
}

void HttpConn::send_dir() {

	// 拼一个html页面<table></table>

	sprintf(dir_address, "<html><head><title>目录名: %s</title></head>", real_file);
	sprintf(dir_address + strlen(dir_address), "<body><h1>当前目录: %s</h1><table>", real_file);

	char enstr[1024] = { 0 };
	char path[1024] = { 0 };
	// 目录项二级指针
	struct dirent** ptr;
	int num = scandir(real_file, &ptr, NULL, alphasort);
	// 遍历
	for (int i = 0; i < num; ++i)
	{
		char* name = ptr[i]->d_name;

		// 拼接文件的完整路径
		sprintf(path, "%s/%s", real_file, name);
		printf("path = %s ===================\n", path);
		struct stat st;
		stat(path, &st);

		encode_str(enstr, sizeof(enstr), name);
		// 如果是文件
		if (S_ISREG(st.st_mode))
		{
			sprintf(dir_address + strlen(dir_address),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}
		// 如果是目录
		else if (S_ISDIR(st.st_mode))
		{
			sprintf(dir_address + strlen(dir_address),
				"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}
	
	}

	sprintf(dir_address + strlen(dir_address), "</table></body></html>");

	printf("目录响应html页面拼接完成\n");

}



// 16进制数转化为10进制
int HttpConn::hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void HttpConn::encode_str(char* to, int tosize, const char* from)
{
	int tolen;

	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
	{
		if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
		{
			*to = *from;
			++to;
			++tolen;
		}
		else
		{
			sprintf(to, "%%%02x", (int)*from & 0xff);
			to += 3;
			tolen += 3;
		}

	}
	*to = '\0';
}


void HttpConn::decode_str(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{

			*to = hexit(from[1]) * 16 + hexit(from[2]);

			from += 2;
		}
		else
		{
			*to = *from;

		}

	}
	*to = '\0';

}

// 通过文件名获取文件的类型
const char* HttpConn::get_file_type(const char* name)
{
	const char* dot;

	// 自右向左查找‘.’字符, 如不存在返回NULL
	dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcmp(dot, ".pdf") == 0)
		return "application/pdf";
	if (strcmp(dot, ".doc") == 0)
		return "application/msword";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";
}

