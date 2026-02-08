# WorkTimer - 牛马钟
供牛马灵活记录自己的工作/摸鱼时间，评估自己的实际工作时长


## quick start

### 前置要求

- GCC 9+ 或 Clang 10+，支持 C++17
- CMake 3.20+
- Node.js 16+
- SQLite3
- Make

### 构建与运行

```bash
# 克隆项目
git clone https://github.com/ttttwwww/work_timer.git
cd work_timer

# 一键构建
make release

# 运行
cd release
./WorkTimer
```

在浏览器打开：http://localhost:8080

注意：该项目需要sqlite3和libasio-dev的依赖，否则编译可能报错，运行下列命令安装相应的库文件
对于Ubuntu/Debian：

```bash
sudo apt update
sudo apt install -y libasio-dev libsqlite3-dev
```

对于CentOS 7 / Rocky Linux / RHEL
```bash
# 安装开发工具包（如果未安装）
sudo yum groupinstall "Development Tools"

# 安装 SQLite3 和 Asio 的开发包
sudo yum install -y sqlite-devel asio-devel

# 如果找不到 asio-devel，可能需要先安装 EPEL 源
# sudo yum install -y epel-release
```
nodejs的安装命令如下
安装nodejs
```bash
# 1. 一键安装 fnm 脚本
curl -fsSL https://fnm.vercel.app/install | bash

# 2. 激活环境 (或重启终端)
source ~/.bashrc

# 3. 安装并使用最新的 LTS (长期支持版)
fnm install --lts
fnm use lts-latest

# 4. 验证
node -v
```


## 配置

数据库路径和运行端口可以自定义，make后会在会在release文件夹中生成config.json文件。


```json
{
  "port": 8080,
  "database_path": "../data/worktimer.db"
}
```
修改其中的port与database_path以自定义端口与数据存放路径

## 项目结构

```
worker-counter/
├── backend/              # C++ 后端
│   ├── include/          # 头文件
│   ├── src/              # 源代码
│   └── config.json       # 配置文件
├── frontend/             # Vue 前端
│   └── src/
│       ├── app.vue       # 主组件
│       └── components/   # 子组件
├── data/                 # 数据目录（不会被 clean 删除）
│   └── worktimer.db      # SQLite 数据库
├── docs/                 # 文档
│   ├── architecture.md   # 架构文档
│   ├── data-flow.md      # 数据流图
│   └── HOW-TO-DRAW.md    # 绘图教程
├── release/              # 发布目录
└── Makefile              # 构建脚本
```

详细架构见：[docs/architecture.md](./docs/architecture.md)



## 编译选项

```bash
make              # 增量构建（默认 Debug）
make debug        # Debug 模式（带调试输出）
make release      # Release 模式（优化版本）
make clean        # 清理构建（保留 data/）
```

