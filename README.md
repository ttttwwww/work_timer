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
git clone https://github.com/yourusername/worker-counter.git
cd worker-counter

# 一键构建
make release

# 运行
cd release
./WorkTimer
```

在浏览器打开：http://localhost:8080


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

