/*
 * 这是一个用 C 语言编写的简单 Web 服务器示例程序，
 * 用来演示套接字编程和基本的 HTTP 请求处理流程。
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void *accept_request_thread(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* 当 accept() 返回后，解析并处理客户端发来的 HTTP 请求。
 * 参数：client 为已连接客户端的套接字描述符。*/
/**********************************************************************/
void accept_request(int client) {
  char buf[1024];   /* 用于保存请求行和请求头 */
  int numchars;     /* 实际读取的字节数 */
  char method[255]; /* HTTP 方法，例如 GET/POST */
  char url[255];    /* 客户端请求的 URL */
  char path[512];   /* 映射到本地文件系统的路径 */
  size_t i, j;
  struct stat st;            /* 目标文件的状态信息 */
  int cgi = 0;               /* 非 0 表示需要以 CGI 方式执行 */
  char *query_string = NULL; /* 指向 URL 中的查询字符串部分 */

  numchars = get_line(client, buf, sizeof(buf)); /* 读取请求行 */
  i = 0;
  j = 0;
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';

  if (strcasecmp(method, "GET") &&
      strcasecmp(method, "POST")) /* 只支持 GET/POST */
  {
    unimplemented(client);
    return;
  }

  if (strcasecmp(method, "POST") == 0) /* POST 一律视为 CGI */
    cgi = 1;

  i = 0;
  while (ISspace(buf[j]) && (j < sizeof(buf)))
    j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  if (strcasecmp(method, "GET") == 0) /* GET 请求中可能包含查询字符串 */
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?') {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  sprintf(path, "htdocs%s", url); /* 将 URL 映射到 htdocs 目录下的文件 */
  if (path[strlen(path) - 1] == '/')
    strcat(path, "index.html");
  if (stat(path, &st) == -1) { /* 文件不存在 */
    while ((numchars > 0) &&
           strcmp("\n", buf)) /* read & discard headers 丢弃剩余头部 */
      numchars = get_line(client, buf, sizeof(buf));
    not_found(client);
  } else {
    if ((st.st_mode & S_IFMT) == S_IFDIR) /* 如果是目录，默认追加 index.html */
      strcat(path, "/index.html");
    if ((st.st_mode & S_IXUSR) || /* 检查执行位，判断是否当作 CGI 程序 */
        (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);
  }

  close(client);
}

void *accept_request_thread(void *arg) {
  int client = (int)(intptr_t)arg;
  accept_request(client);
  return NULL;
}

/**********************************************************************/
/* 告知客户端其发送的 HTTP 请求有错误，例如缺少必要头部字段等。
 * 参数：client 为客户端套接字。*/
/**********************************************************************/
void bad_request(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* 将指定文件的全部内容通过套接字发送给客户端。
 * 参数：client 为客户端套接字，resource 为已打开的文件指针。*/
/**********************************************************************/
void cat(int client, FILE *resource) {
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while (!feof(resource)) {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* 通知客户端：服务器端 CGI 脚本无法正常执行。
 * 参数：client 为客户端套接字。*/
/**********************************************************************/
void cannot_execute(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 使用 perror() 打印系统调用相关错误信息，并以错误状态退出程序。*/
/**********************************************************************/
void error_die(const char *sc) {
  perror(sc);
  exit(1);
}

/**********************************************************************/
/* 执行指定的 CGI 脚本，为其设置必要的环境变量并转发输入/输出。
 * 参数：client 为客户端套接字，path 为 CGI 路径，
 *       method 为 HTTP 方法，query_string 为查询字符串（GET 情况）。*/
/**********************************************************************/
void execute_cgi(int client, const char *path, const char *method,
                 const char *query_string) {
  char buf[1024];    /* 用于读取请求头和临时存放数据 */
  int cgi_output[2]; /* 子进程 CGI 输出管道（服务器从中读） */
  int cgi_input[2];  /* 子进程 CGI 输入管道（服务器向其写） */
  pid_t pid;
  int status;        /* 子进程退出状态 */
  int i;
  char c;                  /* 逐字节转发 POST 数据和 CGI 输出 */
  int numchars = 1;        /* 最近一次读取到的字节数 */
  int content_length = -1; /* POST 请求体长度，默认为未知 */

  buf[0] = 'A';
  buf[1] = '\0';
  if (strcasecmp(method, "GET") == 0)
    while ((numchars > 0) &&
           strcmp("\n", buf)) /* read & discard headers 丢弃剩余头部 */
      numchars = get_line(client, buf, sizeof(buf));
  else /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) &&
           strcmp("\n", buf)) /* 遍历头部以获得 Content-Length */
    {
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1) { /* POST 缺少 Content-Length 视为错误请求 */
      bad_request(client);
      return;
    }
  }

  sprintf(buf, "HTTP/1.0 200 OK\r\n"); /* 执行 CGI 成功时返回 200 状态行 */
  send(client, buf, strlen(buf), 0);

  if (pipe(cgi_output) < 0) { /* 创建 CGI 输出管道失败 */
    cannot_execute(client);
    return;
  }
  if (pipe(cgi_input) < 0) { /* 创建 CGI 输入管道失败 */
    cannot_execute(client);
    return;
  }

  if ((pid = fork()) < 0) { /* 派生子进程执行 CGI */
    cannot_execute(client);
    return;
  }
  if (pid == 0) /* child: CGI script 子进程执行 CGI 程序 */
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    dup2(cgi_output[1], 1); /* 标准输出重定向到管道写端 */
    dup2(cgi_input[0], 0);  /* 标准输入重定向到管道读端 */
    close(cgi_output[0]);
    close(cgi_input[1]);
    sprintf(meth_env, "REQUEST_METHOD=%s", method); /* 设置请求方法环境变量 */
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string); /* GET 查询字符串 */
      putenv(query_env);
    } else { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d",
              content_length); /* POST 请求体长度 */
      putenv(length_env);
    }
    execl(path, path, NULL); /* 用指定脚本替换当前子进程镜像 */
    exit(0);
  } else { /* parent 父进程负责转发数据并收集 CGI 输出 */
    close(cgi_output[1]);
    close(cgi_input[0]);
    if (strcasecmp(method, "POST") == 0) /* 将请求体转发给 CGI 标准输入 */
      for (i = 0; i < content_length; i++) {
        recv(client, &c, 1, 0);
        write(cgi_input[1], &c, 1);
      }
    while (read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    close(cgi_output[0]);
    close(cgi_input[1]);
    waitpid(pid, &status, 0);
  }
}

/**********************************************************************/
/* 从套接字中读取一行数据，兼容 '\n'、'\r'、"\r\n" 等多种行结束符。
 * 读取到的字符串以 '\0' 结尾。
 * 参数：sock 为套接字描述符，buf 为接收缓冲区，size 为缓冲区大小。
 * 返回：实际存入缓冲区的字节数（不含 '\0'）。*/
/**********************************************************************/
int get_line(int sock, char *buf, int size) {
  int i = 0;     /* 已经读入的字符数量 */
  char c = '\0'; /* 当前读取的字符 */
  int n;         /* recv 返回的字节数 */

  while ((i < size - 1) && (c != '\n')) /* 逐字节读取直到换行或缓冲区满 */
  {
    n = recv(sock, &c, 1, 0);
    /* DEBUG printf("%02X\n", c); */
    if (n > 0) {
      if (c == '\r') {
        n = recv(sock, &c, 1, MSG_PEEK); /* 预读一个字符，判断是否为 '\n' */
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n')) /* 若为 "\r\n" 组合，则真正读掉 '\n' */
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }
      buf[i] = c;
      i++;
    } else
      c = '\n';
  }
  buf[i] = '\0';

  return (i);
}

/**********************************************************************/
/* 返回一个普通文件对应的 HTTP 响应头信息。
 * 参数：client 为客户端套接字，filename 为文件名（此处未用来区分类型）。*/
/**********************************************************************/
void headers(int client, const char *filename) {
  char buf[1024];
  (void)filename; /* could use filename to determine file type */

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 向客户端返回 404 Not Found 状态以及简单的 HTML 错误页面。*/
/**********************************************************************/
void not_found(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 向客户端发送一个普通静态文件（如 HTML），并附带合适的响应头。
 * 参数：client 为客户端套接字，filename 为要发送的文件名。*/
/**********************************************************************/
void serve_file(int client, const char *filename) {
  FILE *resource = NULL; /* 要发送的文件指针 */
  int numchars = 1;      /* 读取请求头时使用的计数器 */
  char buf[1024];        /* 读取并丢弃请求头的缓冲区 */

  buf[0] = 'A';
  buf[1] = '\0';
  while ((numchars > 0) &&
         strcmp("\n", buf)) /* read & discard headers 先丢弃请求头 */
    numchars = get_line(client, buf, sizeof(buf));

  resource = fopen(filename, "r"); /* 以文本方式打开要发送的文件 */
  if (resource == NULL)
    not_found(client);
  else {
    headers(client, filename);
    cat(client, resource);
  }
  fclose(resource);
}

/**********************************************************************/
/* 在指定端口上创建监听套接字，如果端口为 0 则由系统自动分配端口。
 * 参数：port 为端口号指针，返回值为监听套接字描述符。*/
/**********************************************************************/
int startup(u_short *port) {
  int httpd = 0;           /* 监听套接字描述符 */
  struct sockaddr_in name; /* 服务器自身的地址结构 */

  httpd = socket(PF_INET, SOCK_STREAM, 0); /* 创建 TCP 套接字 */
  if (httpd == -1)
    error_die("socket");
  memset(&name, 0, sizeof(name));           /* 先将结构体清零 */
  name.sin_family = AF_INET;                /* IPv4 */
  name.sin_port = htons(*port);             /* 监听端口（网络字节序） */
  name.sin_addr.s_addr = htonl(INADDR_ANY); /* 绑定到所有本地地址 */
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    error_die("bind");
  if (*port == 0) /* 若没有指定端口，则需要获取系统分配的端口 */
  {
    socklen_t namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("getsockname");
    *port = ntohs(name.sin_port);
  }
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return (httpd);
}

/**********************************************************************/
/* 通知客户端：请求的 HTTP 方法尚未在服务器中实现。*/
/**********************************************************************/
void unimplemented(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 服务器主函数：创建监听套接字并循环接受客户端连接。
 * 每个新连接由一个线程来处理 HTTP 请求。*/
/**********************************************************************/
int main(void) {
  int server_sock = -1;           /* 服务器监听套接字 */
  u_short port = 0;               /* 监听端口，0 表示交给系统分配 */
  int client_sock = -1;           /* 已连接的客户端套接字 */
  struct sockaddr_in client_name; /* 客户端地址信息 */
  socklen_t client_name_len = sizeof(client_name);
  pthread_t newthread; /* 处理客户端请求的线程 ID */

  server_sock = startup(&port);               /* 创建监听套接字并开始监听 */
  printf("httpd running on port %d\n", port); /* 打印实际使用的端口 */

  while (1) /* 主循环：不断接受新的客户端连接 */
  {
    client_sock = accept(server_sock, (struct sockaddr *)&client_name,
                         &client_name_len); /* 阻塞等待客户端连接 */
    if (client_sock == -1)
      error_die("accept");
    /* accept_request(client_sock); */ /* 单线程版本直接调用处理函数 */
    if (pthread_create(&newthread, NULL, accept_request_thread,
                       (void *)(intptr_t)client_sock) != 0)
      perror("pthread_create"); /* 使用线程并发处理多个客户端 */
  }

  close(server_sock);

  return (0);
}
