# WorkTimer 构建问题修复记录

> 记录日期：2026-02-09  
> 系统环境：Rocky Linux 8.10  
> 初始状态：编译失败，缺少多个依赖

## 📋 问题清单与解决方案

### 问题 1: npm 命令未找到

**错误信息：**
```
/bin/sh: line 2: npm: command not found
make[1]: *** [Makefile:58: frontend/.build_done] Error 127
```

**原因分析：**
- 系统未安装 Node.js 和 npm
- Vite 5.x 前端构建工具需要 Node.js 环境

**解决方案：**
```bash
# 检查可用的 Node.js 版本
sudo dnf module list nodejs

# 安装 Node.js 20 LTS
sudo dnf module reset nodejs -y
sudo dnf module enable nodejs:20 -y
sudo dnf install nodejs -y

# 验证安装
node --version  # v20.19.5
npm --version   # 10.8.2
```

---

### 问题 2: Node.js 版本过旧

**错误信息：**
```
npm ERR! my-work-timer@0.0.0 build: `vite build`
npm ERR! Exit status 1
```

**原因分析：**
- 初始 Node.js 版本为 v10.24.0
- Vite 5.x 要求 Node.js >= 18.0.0

**解决方案：**
已通过问题 1 的解决方案升级到 v20.19.5

---

### 问题 3: SQLite3 库未找到

**错误信息：**
```
CMake Error: Could NOT find SQLite3 (missing: SQLite3_INCLUDE_DIR SQLite3_LIBRARY)
```

**原因分析：**
- 系统仅安装了 SQLite3 运行时库
- 缺少编译所需的开发头文件和库

**解决方案：**
```bash
sudo dnf install sqlite-devel -y
```

**安装内容：**
- SQLite3 头文件：`/usr/include/sqlite3.h`
- SQLite3 库文件：`/usr/lib64/libsqlite3.so`

---

### 问题 4: ASIO 头文件缺失

**错误信息：**
```
/home/leolee/Work/cpp/work_timer/backend/include/crow_all.h:1029:10: 
fatal error: asio.hpp: No such file or directory
 #include <asio.hpp>
          ^~~~~~~~~~
```

**原因分析：**
- Crow Web 框架依赖 ASIO 异步 I/O 库
- Rocky Linux 8 的 EPEL 仓库不包含 asio-devel

**解决方案：**
```bash
# 从源码安装 ASIO（header-only 库）
cd /tmp
wget https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
tar -xzf asio-1-30-2.tar.gz
sudo cp -r asio-asio-1-30-2/asio/include/asio* /usr/include/
rm -rf asio-*
```

---

### 问题 5: DBL_DECIMAL_DIG 未定义

**错误信息：**
```
/home/leolee/Work/cpp/work_timer/backend/include/crow_all.h:4379:74: 
error: 'DBL_DECIMAL_DIG' was not declared in this scope
```

**原因分析：**
- GCC 8.5.0 在某些情况下不会自动定义 `DBL_DECIMAL_DIG` 宏
- Crow 库依赖此宏进行浮点数序列化

**解决方案：**

修改文件：`backend/include/crow_all.h`

```cpp
// SPDX-License-Identifier: BSD-3-Clause AND ISC AND MIT

// 修复 DBL_DECIMAL_DIG 未定义的问题
#include <cfloat>
#ifndef DBL_DECIMAL_DIG
#define DBL_DECIMAL_DIG 17
#endif

/*BSD 3-Clause License
...
```

**修改位置：** 文件开头，第 1-2 行之后

---

### 问题 6: pthread 链接错误

**错误信息：**
```
/usr/bin/ld: CMakeFiles/WorkTimer.dir/main.cpp.o: 
undefined reference to symbol 'pthread_join@@GLIBC_2.2.5'
```

**原因分析：**
- ASIO 库使用多线程，需要 pthread 库
- CMakeLists.txt 未链接 pthread

**解决方案：**

修改文件：`backend/CMakeLists.txt`

```cmake
# 4. 链接库
# 查找 pthread
find_package(Threads REQUIRED)

if(TARGET SQLite3::SQLite3)
    target_link_libraries(WorkTimer SQLite3::SQLite3 Threads::Threads)
else ()
    target_include_directories(WorkTimer PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(WorkTimer ${SQLite3_LIBRARIES} Threads::Threads)
endif ()
```

**修改内容：**
- 添加 `find_package(Threads REQUIRED)`
- 在 `target_link_libraries` 中添加 `Threads::Threads`

---

### 问题 7: std::filesystem 链接错误

**错误信息：**
```
undefined reference to `std::filesystem::__cxx11::path::_M_split_cmpts()'
undefined reference to `std::filesystem::status(std::filesystem::__cxx11::path const&)'
```

**原因分析：**
- GCC 8.x 的 `std::filesystem` 需要单独链接库文件
- 项目使用了文件系统功能但未链接对应库

**解决方案：**

修改文件：`backend/CMakeLists.txt`（在上一步的基础上）

```cmake
if(TARGET SQLite3::SQLite3)
    target_link_libraries(WorkTimer SQLite3::SQLite3 Threads::Threads stdc++fs)
else ()
    target_include_directories(WorkTimer PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(WorkTimer ${SQLite3_LIBRARIES} Threads::Threads stdc++fs)
endif ()
```

**修改内容：**
- 在 `target_link_libraries` 中添加 `stdc++fs`

---

## 🔧 修改文件汇总

| 文件路径 | 修改内容 | 解决问题 |
|---------|---------|---------|
| `backend/CMakeLists.txt` | 添加 `_GLIBCXX_USE_C99_MATH_TR1` 定义 | C++ 标准库兼容性 |
| `backend/CMakeLists.txt` | 添加 `find_package(Threads)` | pthread 链接 |
| `backend/CMakeLists.txt` | 链接 `Threads::Threads` | pthread 未定义引用 |
| `backend/CMakeLists.txt` | 链接 `stdc++fs` | filesystem 未定义引用 |
| `backend/include/crow_all.h` | 添加 `DBL_DECIMAL_DIG` 宏定义 | 浮点数宏未定义 |

## 📦 系统依赖安装清单

### 必需依赖

| 软件包 | 版本 | 安装命令 | 用途 |
|--------|------|---------|------|
| Node.js | 20.19.5 | `sudo dnf install nodejs` | 前端构建 |
| npm | 10.8.2 | 随 Node.js 安装 | 包管理 |
| sqlite-devel | 3.26.0 | `sudo dnf install sqlite-devel` | SQLite3 开发库 |
| ASIO | 1.30.2 | 手动安装（见下方） | 异步 I/O 库 |

### ASIO 安装脚本

```bash
#!/bin/bash
# 安装 ASIO 头文件库

cd /tmp
wget https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
tar -xzf asio-1-30-2.tar.gz
sudo cp -r asio-asio-1-30-2/asio/include/asio* /usr/include/
rm -rf asio-*

echo "ASIO 安装完成"
```

## 🎯 构建流程

### 完整构建命令

```bash
# 1. 进入项目目录
cd /home/leolee/Work/cpp/work_timer

# 2. 清理旧构建（可选）
make clean

# 3. Release 构建
make release

# 4. 运行程序
cd build-Release
./WorkTimer
```

### 构建输出结构

```
build-Release/
├── WorkTimer          # C++ 可执行文件
├── config.json        # 配置文件
└── dist/              # 前端静态资源
    ├── index.html
    ├── assets/
    │   ├── index-*.css
    │   └── index-*.js
    └── ...

data/                  # 数据库目录（自动创建）
└── worktimer.db       # SQLite 数据库
```

## 📊 问题解决时间线

```
初始状态: make release 失败
    ↓ (1分钟)
安装 Node.js 20
    ↓ (2分钟)
安装 sqlite-devel
    ↓ (3分钟)
安装 ASIO 库
    ↓ (5分钟)
修改 crow_all.h
    ↓ (2分钟)
修改 CMakeLists.txt (pthread)
    ↓ (1分钟)
修改 CMakeLists.txt (stdc++fs)
    ↓
✅ 构建成功！
```

总计用时：约 15 分钟

## 🔍 验证构建成功

```bash
# 检查可执行文件
$ file build-Release/WorkTimer
build-Release/WorkTimer: ELF 64-bit LSB executable, x86-64

# 检查依赖库
$ ldd build-Release/WorkTimer
    linux-vdso.so.1
    libsqlite3.so.0 => /lib64/libsqlite3.so.0
    libstdc++.so.6 => /lib64/libstdc++.so.6
    libpthread.so.0 => /lib64/libpthread.so.0
    ...

# 启动服务
$ cd build-Release && ./WorkTimer
[INFO] Server starting on http://0.0.0.0:8080
```

## 💡 经验总结

### 1. Node.js 版本管理
- Rocky Linux 8 默认 Node.js 版本为 v10，已过时
- 建议使用 dnf module 安装较新版本（18/20/22）
- 或使用 nvm 进行版本管理

### 2. C++ 依赖管理
- Rocky Linux 8 使用 GCC 8.5.0，部分 C++17 特性需要额外链接库
- header-only 库（如 ASIO）需要手动安装
- 建议使用 CMake 的 `FetchContent` 或 `find_package` 管理依赖

### 3. 构建优化建议
- 添加依赖检查脚本 `check_dependencies.sh`
- 使用 CMake presets 管理不同构建配置
- 考虑使用容器化部署避免环境差异

## 📚 参考资料

- [ASIO 官方文档](https://think-async.com/Asio/)
- [Crow Web 框架](https://crowcpp.org/)
- [Node.js Rocky Linux 安装指南](https://nodejs.org/en/download/package-manager/)
- [CMake find_package 文档](https://cmake.org/cmake/help/latest/command/find_package.html)

## ✅ 验证清单

- [x] Node.js >= 18.0.0
- [x] npm 可用
- [x] SQLite3 开发库已安装
- [x] ASIO 头文件已安装
- [x] pthread 正确链接
- [x] filesystem 库正确链接
- [x] 前端成功构建
- [x] 后端成功编译
- [x] 程序可正常启动

---

**文档版本：** v1.0  
**最后更新：** 2026-02-09  
**维护者：** leolee
