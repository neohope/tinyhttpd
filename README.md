## 项目介绍

这是一个 500 行实现的超轻量级 HTTP 服务器示例，用于学习和理解：
- 基于 TCP 的网络编程
- HTTP 请求/响应的基本处理流程
- 简单 CGI 执行机制

本项目原作者为 J. David Blackstone（1999 年），原始项目地址：  
https://sourceforge.net/projects/tinyhttpd/

本仓库在保留原始结构的基础上，对源码做了适配和中文注释，便于在现代 Linux / WSL 环境中编译、运行和阅读。

## 目录结构

- `httpd.c`：核心 HTTP 服务器，实现 socket 监听、解析请求、返回静态文件和执行 CGI
- `simpleclient.c`：简单的 TCP 客户端示例，用于连接本地服务器测试
- `htdocs/`：站点根目录
  - `index.html`：默认首页
  - `check.cgi` / `color.cgi`：示例 CGI 脚本
- `Makefile`：在类 Unix / WSL 环境下的编译脚本

## 在原生 Linux 下编译

假设你已经将仓库克隆到某个目录，例如：

```bash
cd ~/workspace/tinyhttpd
```

1. 确认已经安装 gcc、make：

   ```bash
   sudo apt update
   sudo apt install build-essential
   ```

2. 使用 Makefile：

   ```bash
   make
   ```

   会在当前目录生成 `httpd` 和 `simpleclient` 可执行文件。

3. 清理并重新编译：

   ```bash
   make clean
   make
   ```

## 在 Windows + WSL 环境下编译

确保在 WSL/Linux 中已经安装 gcc、make：

```bash
sudo apt update
sudo apt install build-essential
```

然后在 Windows 侧（例如 PowerShell）中，使用 WSL 进入项目目录并编译（假设仓库在 `D:\workspace\tinyhttpd`）：

```powershell
wsl bash -lc "cd /mnt/d/workspace/tinyhttpd && make"
```

这会生成一个名为 `httpd` 和 `simpleclient` 的可执行文件。如果需要重新编译，可以先清理：

```powershell
wsl bash -lc "cd /mnt/d/workspace/tinyhttpd && make clean && make"
```

## 运行服务器与测试（Linux / WSL 通用）

1. 启动 HTTP 服务器

   在 Linux 终端中：

   ```bash
   cd /项目路径/tinyhttpd
   ./httpd
   ```

   或在 Windows + WSL 的 PowerShell 中：

   ```powershell
   wsl bash -lc "cd /mnt/d/workspace/tinyhttpd && ./httpd"
   ```
   终端会输出类似：

   ```text
   httpd running on port 55799
   ```

   实际端口号由系统分配，**以程序输出为准**。

2. 使用浏览器访问

   在浏览器中访问（端口号请替换为实际输出）：

   `http://127.0.0.1:55799/`

   将看到 `htdocs/index.html` 的内容。

3. 使用 simpleclient 测试（可选）

   在另一个终端运行：

   - 原生 Linux：

     ```bash
     cd /你的/项目路径/tinyhttpd
     ./simpleclient
     ```

   - Windows + WSL：

     ```powershell
     wsl bash -lc "cd /mnt/d/workspace/tinyhttpd && ./simpleclient"
     ```

   该程序会向服务器发送一个字符，并打印返回的字符。

## CGI 相关注意事项（Linux / WSL 通用）

- CGI 脚本位于 `htdocs/` 目录下，例如 `check.cgi`、`color.cgi`。
- 确保脚本具有可执行权限：

  ```bash
  cd /你的/项目路径/tinyhttpd/htdocs
  chmod +x check.cgi color.cgi
  ```

- 对于 Perl CGI（例如 `color.cgi`），需要安装 `CGI.pm` 模块（Debian/Ubuntu/WSL）：

  ```bash
  sudo apt install libcgi-pm-perl
  ```

## 学习建议

- 从 `httpd.c` 开始阅读，关注以下函数：
  - `main`：服务器入口，创建监听 socket 并循环 `accept`
  - `accept_request`：解析 HTTP 请求行和头部，决定返回文件还是执行 CGI
  - `serve_file` / `headers` / `not_found`：返回静态资源和错误页
  - `execute_cgi`：示例 CGI 执行流程（环境变量、管道、fork/exec）
- 再结合 `simpleclient.c`，理解客户端如何通过 `socket` / `connect` 与服务器交互。
