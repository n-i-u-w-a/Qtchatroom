# Qt 多人聊天室

基于 **Qt 6 + CMake** 的图形化多人聊天系统，支持用户注册/登录、广播/私聊、密保找回密码、账号注销。

## 项目结构

```
vscodeqt/
├── server/                         # 服务端（控制台程序）
│   ├── CMakeLists.txt              # 依赖 Qt6::Core + Network + Concurrent + Sql
│   ├── main.cpp                    # 入口，自动探测可用端口
│   ├── chatserver.h / .cpp         # QTcpServer 子类：连接管理、用户注册/登录、消息路由
│   └── clienthandler.h / .cpp      # 每客户端 I/O：异步收发、线程池消息解析
│
├── client/                         # 客户端（GUI 程序）
│   ├── CMakeLists.txt              # 依赖 Qt6::Core + Widgets + Network
│   ├── main.cpp                    # 入口，启动登录窗口
│   ├── chatclient.h / .cpp         # QTcpSocket 封装：协议序列化、信号转发
│   ├── chatwindow.h / .cpp         # 主窗口：登录/注册/找回密码卡片 + 聊天面板
│   └── privatechatdialog.h / .cpp  # 私聊弹窗：QQ 气泡风格，历史持久化
│
├── run_server.bat                  # 一键启动服务端
├── run_client.bat                  # 一键启动客户端
├── CMakeLists.txt                  # 旧单文件项目（已废弃，保留用作参考）
├── main.cpp                        # 旧单文件入口（已废弃）
└── README.md
```

## 快速开始

### 环境要求

- Windows 10/11
- Qt 6.10（MinGW 64-bit）
- CMake 3.16+
- Ninja（Qt 自带）

### 构建

```bash
# 服务端
cmake -G Ninja -S server -B build-server \
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe
cmake --build build-server

# 客户端
cmake -G Ninja -S client -B build-client \
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe
cmake --build build-client

# 部署 Qt DLL（首次构建后执行一次）
windeployqt --no-translations build-server/chatserver.exe
windeployqt --no-translations build-client/chatclient.exe
```

### 运行

1. 双击 `run_server.bat` 启动服务端（自动探测 9527-9536 端口）
2. 双击 `run_client.bat` 启动客户端（可多开模拟多用户）

## 功能列表

| 功能 | 说明 |
|------|------|
| **用户注册** | 用户名 + 密码 + 密保问题，SHA-256 加盐存储 |
| **用户登录** | 密码校验，同名互斥（同一账号不能重复登录） |
| **找回密码** | 输入用户名 → 显示密保问题 → 回答正确后重设密码 |
| **注销账号** | 删除数据库记录、踢下线、广播通知 |
| **广播消息** | 发给所有在线用户 |
| **私聊消息** | 选中在线用户发送，仅对方可见 |
| **私聊弹窗** | 双击用户列表弹出独立对话框，QQ 气泡风格 |
| **聊天记录** | 私聊历史内存持久化，关闭对话框后不丢失 |
| **在线列表** | 实时显示在线用户，用户上下线自动更新 |

## 通信协议

所有消息以 JSON 文本 + 换行符分隔，通过 TCP 发送。

### 登录 / 注册

```json
// → 登录
{"type":"login","username":"alice","password":"123456"}

// → 注册
{"type":"register","username":"alice","password":"123456","question":"你母亲的名字是什么？","answer":"mary"}

// ← 成功
{"type":"login_ok","username":"alice"}

// ← 失败
{"type":"login_error","content":"Invalid username or password"}
```

### 找回密码

```json
// → 查询密保问题
{"type":"forgot_password","username":"alice"}

// ← 返回问题
{"type":"security_question","username":"alice","question":"你母亲的名字是什么？"}

// ← 用户不存在
{"type":"forgot_error","content":"User not found or no security question set"}

// → 重置密码
{"type":"reset_password","username":"alice","answer":"mary","new_password":"7890"}

// ← 成功
{"type":"password_reset_ok","content":"Password reset successfully. You can now login."}

// ← 失败
{"type":"reset_error","content":"Wrong answer"}
```

### 消息

```json
// → 广播
{"type":"broadcast","content":"hello everyone"}

// → 私聊
{"type":"private","to":"bob","content":"hi bob"}

// → 群发
{"type":"group","to":["bob","eve"],"content":"hello"}

// → 注销
{"type":"delete_account"}

// ← 收到消息（scope: broadcast / private / group）
{"type":"message","sender":"alice","content":"hello","timestamp":"14:30:05","scope":"broadcast"}

// ← 系统通知
{"type":"system","content":"alice joined the chat"}

// ← 在线用户列表
{"type":"userlist","users":["alice","bob","eve"]}
```

## 数据库

服务端使用 **SQLite**，数据库文件 `chat.db` 自动创建在服务端运行目录。

### users 表

| 列 | 类型 | 说明 |
|----|------|------|
| id | INTEGER | 自增主键 |
| username | TEXT UNIQUE | 用户名，不可重复 |
| password | TEXT | `salt:sha256_hash` 格式存储 |
| question | TEXT | 密保问题文本 |
| answer | TEXT | 密保答案，`salt:sha256_hash` 格式存储 |
| created | TEXT | 创建时间 |

可用 [DB Browser for SQLite](https://sqlitebrowser.org/dl/) 打开 `chat.db` 直接管理用户。

## 架构设计

### 服务端并发模型

- **主线程**：事件循环驱动，所有 QTcpSocket I/O 异步非阻塞
- **QThreadPool**：消息解析（JSON parse + 格式化）通过 `QtConcurrent::run` 投递到线程池
- **QReadWriteLock**：保护客户端列表和用户名映射，读多写少场景

### 安全设计

- 密码和密保答案均使用 **SHA-256 + 随机盐**（16 字节）哈希存储，不与明文比较
- 密保答案比较时统一转小写、去空格
- 注册/登录时用户名长度限制 1-20 字符，禁止空格
- 密码最少 4 个字符

## 许可证

仅供学习和个人使用。
