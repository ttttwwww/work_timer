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
cd build-Release
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

数据库路径和运行端口可以自定义，make 后会在 build-Release（或 build-Debug）文件夹中生成 config.json 文件。


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


## 计时修复与结束时间调整

- 计时显示以 `/api/state` 返回的服务器时间为基准，使用 `performance.now()` 推进，不依赖设备的 `Date.now()`。每 15 秒和页面重新可见时同步一次。
- 进行中的日历明细、时间条和总时长每秒刷新；跨午夜的记录分别统计到对应日期。
- 点击“结束”后先确认实际结束时间，可将时间提前，并预览保留与扣除的时长。取消则继续计时。
- 后端校验结束时间范围和会话 ID，拒绝重复开始计时、删除活跃记录，以及用旧页面结束另一条记录。
- 笔记使用参数绑定保存，支持单引号、换行和中文；保存失败时保留编辑内容。

## 任务面板

1. 打开“任务面板”，新建任务，填写标题、说明和状态（待办 / 进行中 / 已完成）。左侧支持搜索和状态筛选。
2. 在任务中“添加问题节点”，记录遇到的问题。节点状态为待解决 / 处理中 / 已解决。
3. 选中节点后，添加、编辑和勾选待办，查看清单完成比例。
4. 点击具体待办的“展开进度”，在待办内部追加本次尝试、结果和下一步。各待办分别保存日志，最新记录在前；新建待办会自动展开。勾选完成不会清除日志。
5. 任务、节点、待办和进度支持删除。删除任务、节点或待办会在确认后同时删除其下属数据。

所有内容保存在现有 SQLite 数据库中，刷新或重启后保留，其他设备连接同一服务也能查看。新表首次启动时自动创建，旧计时记录不变。与原项目一致，这是同一服务共享的面板，没有新增账号或用户隔离。编辑说明或状态后需点击保存；进度日志需点击“记录进度”。

任务完成状态与节点状态分别维护，不会因为勾完清单就自动解决问题或完成整个任务。

升级时，原先直接记录在节点下的日志会自动放入该节点新增的“历史进度（原节点记录）”待办，保留原文和时间，不猜测它们属于哪条现有待办。迁移只执行一次，旧计时和其他待办不变。继续使用原数据库路径即可。当前页面内切换节点、收起待办或保存失败都会保留未提交的草稿；刷新页面会丢弃未提交的草稿。

## 回归测试

前端计时逻辑测试（Node.js 18+）：

```bash
cd frontend
npm ci
npm test
npm run build
```

后端 HTTP 集成测试（Python 3；先构建后端）：

```bash
python3 tests/test_api.py ./build-Release/WorkTimer
```

测试会启动临时本地服务并使用临时 SQLite 数据库，验证旧数据库迁移、并发开始、结束时间校验、笔记特殊字符、任务 CRUD、重启持久化和级联删除，不会连接运行中的服务。

API 和数据结构说明见 [docs/task-panel.md](./docs/task-panel.md)。
