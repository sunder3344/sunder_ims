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
#include <time.h>
#include "cJSON.h"
#define MAXLINE 4096
#define LOGIN_MSG "##sunder##ims##"

struct MsgInfo {
	char *nickname;
	char *destip;
	int sockfd_param;
};

//转发消息
struct RecvInfo {
	char *nickname;
	char *content;
};

/**
 * 解析接收到的信息
 */
int parseRecvInfo(struct RecvInfo *rinfo, char *jsonstr) {
	cJSON *parse = cJSON_Parse(jsonstr);
	const cJSON *nickname = NULL;
	const cJSON *content = NULL;
	nickname = cJSON_GetObjectItemCaseSensitive(parse, "nickname");
	content = cJSON_GetObjectItemCaseSensitive(parse, "content");
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
char *getParamString(char *ip, char *nickname, char *content) {
	cJSON *root = NULL;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "dest_ip", cJSON_CreateString(ip));
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
 * 接受消息线程
 */
void readThread(int sockfd) {
	char buf[4096];
	int n;
	while (1) {
		if ((n = recv(sockfd, buf, MAXLINE, 0)) < 0) {
			if (errno == ECONNRESET) {
				close(sockfd);	
			} else {
				printf("readline error\n");
			}
			sleep(1);
		} else if ((n > 0)) {
			//printf("\nfeedback: ---%s--- n:= %d\n", buf, n);
			time_t now = time(NULL);
			char tmp[64];
			strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&now));
			struct RecvInfo *rinfo = (struct RecvInfo *)malloc(sizeof (struct RecvInfo));
			parseRecvInfo(rinfo, buf);
			printf("%s (%s)\n", rinfo->nickname, tmp);
			printf("%s\n\n", rinfo->content);
			free(rinfo);
		}
	}
}

/**
 * 发送消息线程
 */
void writeThread(struct MsgInfo *info) {
	char buf[4096];
	while (1) {
		//printf("%s: ", info->nickname);
		scanf("%s", buf);
		char *json_str = getParamString(info->destip, info->nickname, buf);
		//printf("send json: %s\n", json_str);
		send(info->sockfd_param, json_str, strlen(json_str), 0);
		//printf("received: %s\n", buf);
		//printf("numbytes: %d\n", numbytes);
	}
}

int main(int argc, char * argv[]) {
	int sockfd, numbytes;
	char buf[4096];
	char ip[15];		//对端ip地址
	struct sockaddr_in their_addr;
	pthread_t rid, wid;
	
	if (argc != 3) {
		perror("wrong parameters, please input destination ip address and nickname.");
		exit(1);
	}

	//printf("start!\n");
	while ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1);
	printf("Hi %s, welcome to this chat room\n", argv[2]);
	
	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(8888);
	their_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&(their_addr.sin_zero), 8);

	while(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1);
	printf("Get the Server peer!\n");
	//发送一个消息标识一下
	send(sockfd, LOGIN_MSG, strlen(LOGIN_MSG), 0);

	struct MsgInfo *msgInfo = (struct MsgInfo *)malloc(sizeof(struct MsgInfo));
	//装入MsgInfo
	msgInfo->nickname = argv[2];
	msgInfo->destip = argv[1];
	msgInfo->sockfd_param = sockfd;

	pthread_create(&rid, NULL, readThread, sockfd);
	pthread_create(&wid, NULL, writeThread, msgInfo);
	pthread_join(rid, NULL);
	pthread_join(wid, NULL);
	free(msgInfo);
	close(sockfd);
	return 0;
}
