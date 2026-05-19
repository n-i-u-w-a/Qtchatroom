# 部署指南

## 环境要求

| 软件 | 版本 | 来源 |
|------|------|------|
| Qt 6 | 6.10.2+ (MinGW 64-bit) | https://www.qt.io/download |
| CMake | 3.16+ (Qt 自带) | Qt 安装包内含 |
| Ninja | (Qt 自带) | Qt 安装包内含 |
| MinGW | 13.1.0 (Qt 自带) | Qt 安装包内含 |

### Qt 安装勾选组件

- Qt 6.10.2 → MinGW 64-bit
- Qt 6.10.2 → Qt SQL
- Developer Tools → MinGW 13.1.0 64-bit / CMake / Ninja

## 构建

```bash
# 服务端
D:/Qt/Tools/CMake_64/bin/cmake.exe -G Ninja ^
  -S server -B build-server ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe

D:/Qt/Tools/CMake_64/bin/cmake.exe --build build-server

# 客户端
D:/Qt/Tools/CMake_64/bin/cmake.exe -G Ninja ^
  -S client -B build-client ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.10.2/mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe

D:/Qt/Tools/CMake_64/bin/cmake.exe --build build-client
```

## 部署运行库

```bash
windeployqt --no-translations build-server/chatserver.exe
windeployqt --no-translations build-client/chatclient.exe
```

部署后 build 目录结构：

```
build-server/
├── chatserver.exe
├── Qt6Core.dll, Qt6Network.dll, Qt6Sql.dll
├── libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll
├── sqldrivers/   ← qsqlite.dll 等
├── tls/          ← SSL 插件
└── networkinformation/

build-client/
├── chatclient.exe
├── Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, Qt6Network.dll, Qt6Svg.dll
├── libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll
├── platforms/    ← qwindows.dll（必须有）
├── imageformats/, iconengines/, styles/, tls/, generic/, networkinformation/
├── opengl32sw.dll, D3Dcompiler_47.dll
```

## 运行

双击 `run_server.bat` → `run_client.bat`（可开多个窗口）

客户端登录页 `⚙` 设置按钮可改服务器 IP 和端口。

## 打包分发

将 `build-server/` 和 `build-client/` 分别打包即可，目标机器无需安装 Qt。

## 常见问题

| 问题 | 解决 |
|------|------|
| 双击闪退/找不到 DLL | 确保 windeployqt 已执行，所有 DLL 与 exe 在同一目录 |
| 客户端连不上 | 确认服务端已启动，检查 IP 和端口，关闭防火墙阻拦 |
| 端口被占用 | 服务端自动尝试 9527-9536，修改 `server/main.cpp` 调整范围 |
| 数据库在哪 | `build-server/chat.db`，可用 DB Browser for SQLite 打开 |
| 重置数据库 | 删除 `chat.db`，重启服务端自动创建 |
| 局域网联机 | 客户端 Server 地址填服务端电脑的局域网 IP |
| 浅色主题 | 聊天面板底部 `☀️` 按钮切换 |
