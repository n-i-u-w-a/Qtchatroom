# 部署指南

## 目录

1. [环境准备](#1-环境准备)
2. [获取源码](#2-获取源码)
3. [编译构建](#3-编译构建)
4. [部署运行库](#4-部署运行库)
5. [启动运行](#5-启动运行)
6. [打包分发](#6-打包分发)
7. [常见问题](#7-常见问题)

---

## 1. 环境准备

### 必需软件

| 软件 | 版本要求 | 下载地址 |
|------|---------|---------|
| Qt 6 | 6.10.2+ (MinGW 64-bit) | https://www.qt.io/download |
| CMake | 3.16+ (Qt 自带) | 已包含在 Qt 安装中 |
| Ninja | (Qt 自带) | 已包含在 Qt 安装中 |
| MinGW | 13.1.0 (Qt 自带) | 已包含在 Qt 安装中 |

### 安装 Qt

1. 下载 [Qt Online Installer](https://www.qt.io/download-qt-installer)
2. 安装时勾选以下组件：
   - **Qt 6.10.2** → `MinGW 64-bit`
   - **Qt 6.10.2** → `Qt SQL` (SQLite 驱动)
   - **Developer and Designer Tools** → `MinGW 13.1.0 64-bit`
   - **Developer and Designer Tools** → `CMake`
   - **Developer and Designer Tools** → `Ninja`

### 验证安装

打开终端，确认以下路径存在：

```
D:\Qt\6.10.2\mingw_64\bin          ← Qt DLL（Qt6Core.dll 等）
D:\Qt\6.10.2\mingw_64\bin\windeployqt.exe  ← Qt 部署工具
D:\Qt\Tools\mingw1310_64\bin\g++.exe       ← C++ 编译器
D:\Qt\Tools\CMake_64\bin\cmake.exe         ← CMake
D:\Qt\Tools\Ninja\ninja.exe                ← Ninja 构建工具
```

> 如果你的 Qt 装在别的盘或版本不同，记下实际路径，后续步骤需要替换。

---

## 2. 获取源码

```
d:\vscodeqt\
├── server\
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── chatserver.h / chatserver.cpp
│   └── clienthandler.h / clienthandler.cpp
├── client\
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── chatclient.h / chatclient.cpp
│   ├── chatwindow.h / chatwindow.cpp
│   └── privatechatdialog.h / privatechatdialog.cpp
├── run_server.bat
└── run_client.bat
```

---

## 3. 编译构建

以下命令中的 `D:/Qt/6.10.2/mingw_64` 和 `D:/Qt/Tools/mingw1310_64` 请替换为你的实际安装路径。

### 3.1 编译服务端

```bash
# 配置
D:/Qt/Tools/CMake_64/bin/cmake.exe -G Ninja ^
  -S d:/vscodeqt/server ^
  -B d:/vscodeqt/build-server ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe

# 编译
D:/Qt/Tools/CMake_64/bin/cmake.exe --build d:/vscodeqt/build-server
```

编译成功后在 `d:\vscodeqt\build-server\` 生成 `chatserver.exe`。

### 3.2 编译客户端

```bash
# 配置
D:/Qt/Tools/CMake_64/bin/cmake.exe -G Ninja ^
  -S d:/vscodeqt/client ^
  -B d:/vscodeqt/build-client ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe

# 编译
D:/Qt/Tools/CMake_64/bin/cmake.exe --build d:/vscodeqt/build-client
```

编译成功后在 `d:\vscodeqt\build-client\` 生成 `chatclient.exe`。

### 3.3 一键编译脚本

创建 `build.bat` 放在 `d:\vscodeqt\` 下：

```batch
@echo off
set CMAKE=D:\Qt\Tools\CMake_64\bin\cmake.exe
set PREFIX=D:\Qt\6.10.2\mingw_64
set GXX=D:\Qt\Tools\mingw1310_64\bin\g++.exe
set NINJA=D:\Qt\Tools\Ninja\ninja.exe

echo === Building Server ===
%CMAKE% -G Ninja -S server -B build-server -DCMAKE_PREFIX_PATH=%PREFIX% -DCMAKE_CXX_COMPILER=%GXX% -DCMAKE_MAKE_PROGRAM=%NINJA%
%CMAKE% --build build-server
if %ERRORLEVEL% NEQ 0 exit /b

echo === Building Client ===
%CMAKE% -G Ninja -S client -B build-client -DCMAKE_PREFIX_PATH=%PREFIX% -DCMAKE_CXX_COMPILER=%GXX% -DCMAKE_MAKE_PROGRAM=%NINJA%
%CMAKE% --build build-client
if %ERRORLEVEL% NEQ 0 exit /b

echo === Build Complete ===
pause
```

---

## 4. 部署运行库

编译出的 `chatserver.exe` 和 `chatclient.exe` 依赖 Qt DLL 和 MinGW 运行时库，不能直接拷贝到别的机器运行，需要部署。

### 4.1 自动部署（推荐）

使用 Qt 自带的 `windeployqt` 工具：

```bash
# 服务端
D:/Qt/6.10.2/mingw_64/bin/windeployqt.exe --no-translations ^
  d:/vscodeqt/build-server/chatserver.exe

# 客户端
D:/Qt/6.10.2/mingw_64/bin/windeployqt.exe --no-translations ^
  d:/vscodeqt/build-client/chatclient.exe
```

`windeployqt` 会自动检测依赖并拷贝所需文件。

#### 部署后文件结构

```
build-server/
├── chatserver.exe
├── Qt6Core.dll
├── Qt6Network.dll
├── Qt6Sql.dll
├── libgcc_s_seh-1.dll
├── libstdc++-6.dll
├── libwinpthread-1.dll
├── networkinformation/          ← Qt 网络信息插件
│   └── qnetworklistmanager.dll
├── sqldrivers/                  ← SQL 数据库驱动
│   ├── qsqlite.dll
│   └── ...
└── tls/                         ← Qt SSL/TLS 插件
    ├── qcertonlybackend.dll
    └── qschannelbackend.dll

build-client/
├── chatclient.exe
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Network.dll
├── Qt6Svg.dll
├── libgcc_s_seh-1.dll
├── libstdc++-6.dll
├── libwinpthread-1.dll
├── opengl32sw.dll               ← OpenGL 软件渲染
├── D3Dcompiler_47.dll           ← DirectX 着色器编译
├── generic/                     ← Qt 输入插件
├── iconengines/                 ← Qt 图标引擎
├── imageformats/                ← Qt 图片格式插件
├── networkinformation/          ← Qt 网络信息插件
├── platforms/                   ← Qt 平台插件
│   └── qwindows.dll             ← 必须有此文件才能启动 GUI
├── styles/                      ← Qt 窗口风格插件
└── tls/                         ← Qt SSL/TLS 插件
```

> `qwindows.dll` 必须放在 `platforms/` 子目录中，否则客户端无法启动。

### 4.2 手动补充

如果 `windeployqt` 遗漏了某些库，手动从以下路径拷贝缺失的 DLL 到 build 目录：

```
D:\Qt\6.10.2\mingw_64\bin\       ← Qt DLL
D:\Qt\Tools\mingw1310_64\bin\     ← MinGW 运行时 DLL
```

常用 MinGW 运行时库：

| 文件 | 说明 |
|------|------|
| `libgcc_s_seh-1.dll` | GCC 运行时 |
| `libstdc++-6.dll` | C++ 标准库 |
| `libwinpthread-1.dll` | POSIX 线程 |
| `libatomic-1.dll` | 原子操作 |
| `libgomp-1.dll` | OpenMP 并行库 |

---

## 5. 启动运行

### 5.1 启动脚本

项目自带两个启动脚本：

**run_server.bat**（服务端）：
```batch
@echo off
cd /d "%~dp0build-server"
echo Starting Chat Server...
chatserver.exe
pause
```

**run_client.bat**（客户端）：
```batch
@echo off
cd /d "%~dp0build-client"
echo Starting Chat Client...
chatclient.exe
pause
```

> `cd /d` 切换到 build 目录是**必须的**，因为 `chatserver.exe` 会在当前目录查找 `chat.db` 和 Qt DLL。

### 5.2 运行步骤

1. **启动服务端** — 双击 `run_server.bat`
   ```
   Database initialized (chat.db)
   Server listening on port 9527
   Press Ctrl+C to stop...
   ```
   如果 9527 被占用，自动尝试 9528、9529...

2. **启动客户端** — 双击 `run_client.bat`（可打开多个窗口）
   - 首次使用：点 `Don't have an account? Register` → 选密保问题 → 填答案 → 点 Register
   - 已有账号：直接填用户名密码 → 点 Login

### 5.3 服务端端口

默认从 `9527` 开始尝试，最多到 `9536`。如需修改，编辑 `server/main.cpp`：

```cpp
quint16 port = 9527;  // 改为你想要的端口
while (!server.start(port)) {
    if (++port > 9536) {  // 改为上限端口
        ...
    }
}
```

客户端登录页可以手动修改连接的服务器地址和端口。

---

## 6. 打包分发

### 6.1 给另一台电脑用

1. 编译并部署完成后，将整个 `build-server/` 和 `build-client/` 目录打包
2. 目标电脑上不需要安装 Qt，直接解压运行
3. 如果目标电脑也装了 MinGW，可能需要额外安装 VC++ 运行时

### 6.2 创建便携版

将以下文件打包成一个 ZIP：

```
ChatApp/
├── server/
│   ├── chatserver.exe + 所有 DLL + 插件目录
│   └── run_server.bat
├── client/
│   ├── chatclient.exe + 所有 DLL + 插件目录
│   └── run_client.bat
└── README.txt
```

### 6.3 杀毒软件误报

打包后的 exe 可能被杀毒软件误报，因为 Qt/MinGW 程序使用动态链接且未签名。建议在分发前：
1. 用 `upx` 压缩 exe（可选）
2. 提交到 VirusTotal 验证
3. 签名（需要代码签名证书）

---

## 7. 常见问题

### Q1：双击 exe 闪退或报 "找不到 xxx.dll"

- 确保 `windeployqt` 已正确执行
- 检查 DLL 和 exe 在同一目录下
- 检查 `platforms/qwindows.dll` 是否存在（客户端必须）

### Q2：客户端启动后无法连接服务端

- 确认服务端已启动
- 检查客户端填写的 IP 和端口是否正确（本机 `127.0.0.1`）
- 检查防火墙是否阻止了对应端口

### Q3：端口被占用

服务端会自动尝试下一个端口。如果 9527-9536 全被占用：
- 关闭占用端口的程序
- 或修改 `server/main.cpp` 中的端口范围和 `client/chatwindow.cpp` 中的默认端口

### Q4：数据库文件在哪里

`chat.db` 自动生成在 `chatserver.exe` 所在目录（即 `build-server/`）。可用 DB Browser for SQLite 打开查看/管理用户。

### Q5：如何重置数据库

直接删除 `build-server/chat.db`，下次启动服务端会自动创建新的空库。

### Q6：两台电脑如何互相聊天

1. 一台电脑启动服务端
2. 记下服务端电脑的 IP 地址（`ipconfig` 查看局域网 IP）
3. 客户端登录页将 Server 地址从 `127.0.0.1` 改为服务端电脑的 IP
4. 确保防火墙允许服务端端口的入站连接

### Q7：编译报错 "No CMakeLists.txt found"

确认你的目录结构正确，`server/CMakeLists.txt` 和 `client/CMakeLists.txt` 存在。根目录的旧 `CMakeLists.txt` 是废弃的单文件项目。

### Q8：`windeployqt` 报 "Cannot find any version of dxcompiler.dll"

这个警告可以忽略，不影响运行。如果需要用到硬件加速渲染才需要这些 DirectX 文件。
