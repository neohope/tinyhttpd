/* 简单 TCP 客户端示例：连接本地指定端口并与服务器交换一个字符 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* 程序入口：创建套接字并与服务器通信 */
int main(int argc, char *argv[]) {
  int sockfd;                 /* 用于保存套接字描述符 */
  int len;                    /* 结构体地址长度 */
  struct sockaddr_in address; /* 保存服务器地址信息的结构体 */
  int result;                 /* connect 调用的返回值 */
  char ch = 'A';              /* 将要发送给服务器的字符 */
  int port;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  port = atoi(argv[1]);
  if (port <= 0 || port > 65535) {
    fprintf(stderr, "Invalid port: %s\n", argv[1]);
    exit(1);
  }

  /* 创建 TCP 套接字：IPv4 + 流式套接字 + 默认协议 */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* 填充服务器地址结构体 */
  address.sin_family = AF_INET;                     /* 使用 IPv4 地址族 */
  address.sin_addr.s_addr = inet_addr("127.0.0.1"); /* 目标 IP 地址（本机） */
  address.sin_port = htons((u_short)port); /* 目标端口，需要转换为网络字节序 */

  len = sizeof(address);

  /* 主动向服务器发起连接请求 */
  result = connect(sockfd, (struct sockaddr *)&address, len);

  /* 如果连接失败，打印错误并退出 */
  if (result == -1) {
    perror("oops: client1");
    exit(1);
  }

  /* 向服务器发送一个字节 */
  write(sockfd, &ch, 1);

  /* 从服务器读取一个字节（可能被修改） */
  read(sockfd, &ch, 1);

  /* 打印从服务器返回的字符 */
  printf("char from server = %c\n", ch);

  /* 关闭套接字并正常退出 */
  close(sockfd);
  exit(0);
}
