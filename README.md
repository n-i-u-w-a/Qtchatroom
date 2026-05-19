# Qt 多人聊天室

基于 **Qt 6 + CMake** 的 QQ 风格图形化多人聊天系统。

## 功能

| 模块 | 功能 |
|------|------|
| **用户系统** | 注册/登录、SHA-256+盐密码加密、自定义密保问题、找回密码、注销账号 |
| **消息** | 广播（公共频道）、私聊（一对一）、群聊（创建/加入/退出群组） |
| **好友系统** | 搜索用户、发送/接受/忽略/拒绝好友请求、好友分组管理、重命名/删除分组 |
| **QQ 风格 UI** | 可折叠侧栏、气泡消息、蓝色暗色主题、浅色主题一键切换、左右拖拽伸缩 |
| **辅助功能** | 系统托盘通知、emoji 表情面板、@提醒高亮、打字状态提示、聊天记录导出 txt/html |

## 项目结构

```
vscodeqt/
├── server/                         # 服务端（控制台）
│   ├── CMakeLists.txt              # Qt6::Core + Network + Concurrent + Sql
│   ├── main.cpp                    # 入口，自动探测可用端口 (9527-9536)
│   ├── chatserver.h / .cpp         # QTcpServer：连接管理、注册/登录、消息路由
│   └── clienthandler.h / .cpp      # 每客户端：异步 I/O + QThreadPool 消息解析
│
├── client/                         # 客户端（GUI）
│   ├── CMakeLists.txt              # Qt6::Core + Widgets + Network
│   ├── main.cpp                    # 入口
│   ├── chatclient.h / .cpp         # QTcpSocket 封装：协议序列化
│   ├── chatwindow.h / .cpp         # 主窗口：登录卡片 + QQ 风格聊天面板
│   └── privatechatdialog.h / .cpp  # 私聊弹窗：气泡样式
│
├── run_server.bat / run_client.bat # 一键启动脚本
├── README.md / DEPLOY.md           # 文档
└── .gitignore
```

## 快速开始

### 环境

- Windows 10/11 + Qt 6.10 (MinGW 64-bit) + CMake 3.16+

### 构建

```bash
# 服务端
cmake -G Ninja -S server -B build-server -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64
cmake --build build-server

# 客户端
cmake -G Ninja -S client -B build-client -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64
cmake --build build-client

# 部署 Qt DLL
windeployqt --no-translations build-server/chatserver.exe
windeployqt --no-translations build-client/chatclient.exe
```

### 运行

1. 双击 `run_server.bat` 启动服务端
2. 双击 `run_client.bat` 启动客户端（可多开）

## 通信协议

JSON 文本 + 换行符分隔，TCP 传输。

### 认证

```json
// 注册
{"type":"register","username":"alice","password":"123456","question":"你的宠物叫什么","answer":"cat"}
// 登录
{"type":"login","username":"alice","password":"123456"}
// 找回密码
{"type":"forgot_password","username":"alice"}
{"type":"reset_password","username":"alice","answer":"cat","new_password":"7890"}
```

### 消息

```json
// 广播
{"type":"broadcast","content":"hello everyone"}
// 私聊
{"type":"private","to":"bob","content":"hi"}
// 群消息
{"type":"group_msg","group_id":1,"content":"hello group"}
// 打字状态
{"type":"typing","to":"bob"}
```

### 好友

```json
{"type":"search_users","query":"al"}
{"type":"friend_request","to":"bob"}
{"type":"friend_response","from":"alice","action":"accept"}  // accept/ignore/reject
{"type":"set_friend_group","friend":"bob","group_name":"Work"}
{"type":"create_friend_group","group_name":"Family"}
{"type":"rename_friend_group","old":"Work","new":"Office"}
{"type":"delete_friend_group","group":"OldGroup"}
```

### 群组

```json
{"type":"create_group","name":"Team Alpha"}
{"type":"join_group","group_id":1}
{"type":"leave_group","group_id":1}
```

### 服务端响应

```json
{"type":"login_ok","username":"alice"}
{"type":"login_error","content":"Invalid password"}
{"type":"message","sender":"bob","content":"hi","timestamp":"14:30:05","scope":"private","to":"alice"}
{"type":"system","content":"alice joined the chat"}
{"type":"userlist","users":["alice","bob"]}
{"type":"friend_list","friends":[{"name":"bob","group":"Work"}]}
{"type":"friend_request","from":"eve"}
{"type":"group_list","groups":[{"id":1,"name":"Team","owner":"alice","members":3}]}
```

## 数据库

SQLite (`chat.db`)，表结构：

| 表 | 说明 |
|----|------|
| `users` | 用户：username, password(salt:hash), question, answer |
| `chat_groups` | 群组：id, name, owner |
| `group_members` | 群成员：group_id, username |
| `friends` | 好友：username, friend, group_name |
| `friend_groups` | 空分组持久化：username, group_name |
| `friend_requests` | 好友申请：from_user, to_user, status |

## 架构

- **服务端**：QTcpServer 异步事件驱动 + QThreadPool 并行消息解析 + QReadWriteLock 保护状态
- **客户端**：QTcpSocket 异步收发 + QTreeWidget 可折叠侧栏 + QSplitter 可拖拽布局
- **安全**：SHA-256 + 16 字节随机盐，密保答案统一小写比较

## 许可证

仅供学习和个人使用。
