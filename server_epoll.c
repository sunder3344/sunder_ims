#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <gdbm.h>
#include "cJSON.h"

#define MAXLINE 4096
#define OPEN_MAX 100
#define LISTENQ 2048
#define SOCKET_NUM 4096
#define SERV_PORT 8888
#define INFTIM 1000
#define IP_ADDR "127.0.0.1"
#define PTHREAD_NUM 10
#define DB_NAME "sunder_db"
#define LOGIN_MSG "##sunder##ims##"
#define OFFLINE "offline"

struct Threadparam {
	int epfd_param;
	int listenfd_param;
	struct epoll_event ev_param;
};

//接收消息
struct RecvInfo {
	char *nickname;
	char *destip;
	char *content;
};

//转发消息
struct SendInfo {
	char *nickname;
	char *content;
};

/**
 * 解析接收到的信息
 */
int parseRecvInfo(struct RecvInfo *rinfo, char *jsonstr) {
	cJSON *parse = cJSON_Parse(jsonstr);
	const cJSON *destip = NULL;
	const cJSON *nickname = NULL;
	const cJSON *content = NULL;
	destip = cJSON_GetObjectItemCaseSensitive(parse, "dest_ip");
	nickname = cJSON_GetObjectItemCaseSensitive(parse, "nickname");
	content = cJSON_GetObjectItemCaseSensitive(parse, "content");
	if (cJSON_IsString(destip) && (destip->valuestring != NULL)) {
		rinfo->destip = destip->valuestring;
	} else {
		return -1;
	}
	if (cJSON_IsString(nickname) && (nickname->valuestring != NULL)) {
		rinfo->nickname = nickname->valuestring;
	} else {
		return -1;
	}
	if (cJSON_IsString(content) && (content->valuestring != NULL)) {
		rinfo->content = content->valuestring;
	} else {
		return -1;
	}
}

/**
 * 生成json
 */
char *getParamString(char *nickname, char *content) {
	cJSON *root = NULL;
	root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "content", cJSON_CreateString(content));
    cJSON_AddItemToObject(root, "nickname", cJSON_CreateString(nickname));

	char *out = NULL;
    char *buf = NULL;
    size_t len = 0;
    out = cJSON_Print(root);
    len = strlen(out) + 5;
    buf = (char *)malloc(len);

    if (!cJSON_PrintPreallocated(root, buf, (int)len, 1)) {
        printf("failed\n");
    }
    return buf;
}

/**
 * 插入gdbm，返回值：0：成功；1：有重复值；-1：插入失败(type值：1：插入；2：替换)
 */
int insert(char *keyStr, char *valueStr, int type) {
	int result;
	GDBM_FILE db;
	datum key = {keyStr, strlen(keyStr) + 1};
	datum value = {valueStr, strlen(valueStr) + 1};
	if (type == 1) {
		db = gdbm_open(DB_NAME, 0, GDBM_WRCREAT, 0666, 0);
	} else {
		db = gdbm_open(DB_NAME, 0, GDBM_REPLACE, 0666, 0);
	}
	result = gdbm_store(db, key, value, GDBM_INSERT);
	gdbm_close(db);
	return result;
}

/**
 * 查询gdbm
 */
char *find(char *keyStr) {
	GDBM_FILE db;
	char *val = (char *)malloc(sizeof(char *) * 200);
	db = gdbm_open(DB_NAME, 0, GDBM_READER, 0666, 0);
	if (db == NULL) {
		return NULL;
	}
	datum key = {keyStr, strlen(keyStr) + 1};
	key.dsize = strlen(keyStr) + 1;
	datum value = gdbm_fetch(db, key);
	if (value.dptr != NULL) {
		val = value.dptr;
	} else {
		val = NULL;
	}
	gdbm_close(db);
	return val;
}

/**
 * 删除记录，返回值：0：成功；-1：失败
 */
int del(char *keyStr) {
	GDBM_FILE db;
	int result;
	db = gdbm_open(DB_NAME, 0, GDBM_WRITER, 0666, 0);
	datum key;
	key.dsize = strlen(keyStr) + 1;
	result = gdbm_delete(db, key);
	gdbm_close(db);
	return result;
}

void *loop(struct Threadparam *threadp) {
	struct epoll_event events[SOCKET_NUM];
	char buf[MAXLINE];
	int sock_fd, conn_fd;
	int nfds;
	int n, i;
	socklen_t clilen;
	struct sockaddr_in clientaddr;
	
	while(1) {
		nfds = epoll_wait(threadp->epfd_param, events, SOCKET_NUM, 1);	//等待事件发生
		for (i = 0; i < nfds; i++) {		//处理所发生的全部事件
			if (events[i].data.fd == threadp->listenfd_param) {	//有新的连接
				clilen = sizeof(struct sockaddr_in);
				conn_fd = accept(threadp->listenfd_param, (struct sockaddr *)&clientaddr, &clilen);
				printf("accept a new client: %s in thread:%ld\n", inet_ntoa(clientaddr.sin_addr), pthread_self());
				//连接后记录ip以及在线状态

				threadp->ev_param.data.fd = conn_fd;
				threadp->ev_param.events = EPOLLIN;		//设置监听事件为可写
				epoll_ctl(threadp->epfd_param, EPOLL_CTL_ADD, conn_fd, &(threadp->ev_param));	//新增套接字
			} else if (events[i].events & EPOLLIN) {	//可读事件
				if ((sock_fd = events[i].data.fd) < 0) {
					continue;
				}
				memset(&buf, 0, sizeof(buf));
				if ((n = recv(sock_fd, buf, MAXLINE, 0)) < 0) {
					if (errno == ECONNRESET) {
						close(sock_fd);	
						events[i].data.fd = -1;
					} else {
						printf("readline error\n");
					}
				} else if (n == 0) {
					close(sock_fd);
					printf("关闭\n");
					int del_res = del(inet_ntoa(clientaddr.sin_addr));
					printf("del ip result: %d\n", del_res);
					events[i].data.fd = -1;
				}
				//将sock_fd保存
				if (strcmp(LOGIN_MSG, buf) == 0) {
					char *sockstr = (char *)malloc(sizeof(char *) * 10);
					sprintf(sockstr, "%d", sock_fd);
					insert(inet_ntoa(clientaddr.sin_addr), sockstr, 1);
					free(sockstr);
				}
				//printf("thread:%ld  %d -- > %s\n", pthread_self(), sock_fd, buf);
				threadp->ev_param.data.fd = sock_fd;
				threadp->ev_param.events = EPOLLOUT;
				epoll_ctl(threadp->epfd_param, EPOLL_CTL_MOD, sock_fd, &(threadp->ev_param));	//改动监听事件为可读
			} else if (events[i].events & EPOLLOUT) {	//可写事件
				sock_fd = events[i].data.fd;
				printf("thread:%ld   OUT   buf:=%s  len = %d   isNull = %d\n", pthread_self(), buf, strlen(buf), buf == NULL);
				/*if (buf != NULL) {
					if (strcmp(LOGIN_MSG, buf) != 0) {
						send(sock_fd, buf, MAXLINE, 0);				//如果开启，会发回给连接的客户端
					}
				}*/
				
				if (strlen(buf) > 0 && strcmp(LOGIN_MSG, buf) != 0) {			//结束的时候buf可能会读入NULL
					printf("********buf*********: %s\n", buf);
					//解析发送的聊天json
					struct RecvInfo *rinfo = (struct RecvInfo *)malloc(sizeof (struct RecvInfo));
					parseRecvInfo(rinfo, buf);
					//printf("%s, %s, %s\n", rinfo->destip, rinfo->nickname, rinfo->content);
					//发送消息给需要联系的客户端
					char *sockstr = (char *)malloc(sizeof(char *) * 10);
					//查找对端是否上线了
					sockstr = find(rinfo->destip);
					if (sockstr == NULL) {			//如果不存在，通知原客户端
						char *json_str2 = getParamString(rinfo->nickname, OFFLINE);
						send(sock_fd, json_str2, strlen(json_str2), 0);		//直接转发
					} else {		//如果存在，直接转发
						//printf("find sockfd = %d\n", atoi(sockstr));
						//拼接内容json
						char *json_str = getParamString(rinfo->nickname, rinfo->content);
						time_t now = time(NULL);
						char tmp[64];
						strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&now));
						printf("[%s] server: from %s to %s, content = %s\n", tmp, inet_ntoa(clientaddr.sin_addr), rinfo->destip, json_str);
						send(atoi(sockstr), json_str, strlen(json_str), 0);		//直接转发
					}
					free(sockstr);
					free(rinfo);
				}
				memset(&buf, 0, sizeof(buf));

				threadp->ev_param.data.fd = sock_fd;
				threadp->ev_param.events = EPOLLIN;
				epoll_ctl(threadp->epfd_param, EPOLL_CTL_MOD, sock_fd, &(threadp->ev_param));
			}
		}
	}
}

int main(int argc, char * argv[]) {
	struct epoll_event ev;
	struct sockaddr_in serveraddr;
	int epfd;
	int listenfd;		//监听fd
	int maxi;
	int i;
	pthread_t tid;

	epfd = epoll_create(256);		//生成epoll句柄
	listenfd = socket(AF_INET, SOCK_STREAM, 0);	//创建套接字
	ev.data.fd = listenfd;		//设置与要处理事件相关的文件描写叙述符
	//ev.events = EPOLLIN|EPOLLET;		//设置要处理的事件类型(打开ET模式，可选;当设置ET时，需要用fcntl将socket设置为非阻塞模式)
	ev.events = EPOLLIN;
	
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);	//注冊epoll事件

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERV_PORT);
	bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));	//绑定套接口
	listen(listenfd, LISTENQ);	//转为监听套接字
	int n;

	struct Threadparam *threadp = (struct Threadparam *)malloc(sizeof(struct Threadparam));
	threadp->epfd_param = epfd;
	threadp->listenfd_param = listenfd;
	threadp->ev_param = ev;
	for (i = 0; i < PTHREAD_NUM; i++) {
		pthread_create(&tid, NULL, (void *)loop, threadp);
	}

	pthread_join(tid, NULL);
	while (1) {
		pause();
	}
	return 0;
}
