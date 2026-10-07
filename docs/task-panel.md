# 任务面板与计时接口

## 数据结构

- `sessions` 保留原表结构；旧数据库缺少 `note` 时自动补列。
- `tasks`：标题、说明、状态、创建与更新时间。
- `problem_nodes`：所属任务、问题标题、说明、状态、创建与更新时间。
- `node_todos`：所属节点、待办标题、完成标记。
- `todo_progress`：所属待办、进度文本、记录时间。
- `node_progress`：旧版节点日志表，仅保留迁移前的备份，当前接口不再读写。

数据库首次升级在事务中将 `PRAGMA user_version` 从 0 更新至 1。每个有旧日志的节点新增一条“历史进度（原节点记录）”待办，旧日志复制到该待办，保留 ID、正文及时间。没有日志的节点不新增待办；原有待办不变。迁移失败会回滚，重启不会重复迁移，删除已迁移日志后也不会再次恢复。历史待办初始未完成，可自行勾选。

外键使用 `ON DELETE CASCADE`，删除父对象时同时删除子对象。数据库连接启用外键约束；同一服务内的数据库操作通过互斥锁串行访问。所有文本使用 SQLite 参数绑定。数据库错误会返回失败，不再以成功响应掩盖保存失败。

## 计时

| 方法 | 路径 | 内容 |
| --- | --- | --- |
| GET | `/api/state` | `{server_time, sessions}`，时间戳为服务器 Unix 秒 |
| GET | `/api/history` | 保持原会话数组格式 |
| POST | `/api/start` | `{type: "formal" 或 "informal"}`，返回 201 和新 ID；已有活跃会话返回 409 |
| POST | `/api/stop` | `{id, end_time?}`；不传结束时间使用服务器当前时间 |
| POST | `/api/note` | `{id, note}` |
| POST | `/api/delete` | `{id}`；拒绝删除活跃记录 |

`/api/stop` 现在要求明确的会话 ID，以防旧页面误结束另一设备刚创建的记录；自定义 API 客户端需要同时升级。结束时间必须位于开始时间与服务器当前时间之间，支持零秒记录。

前端以服务器 epoch 加上 `performance.now()` 的差值计算当前时间。请求往返耗时的一半用于近似延迟补偿；服务器时钟仍是持久化依据。每秒刷新 UI，每 15 秒及页面重新可见时同步。断网时显示错误并禁止新的计时操作，已知计时继续显示；恢复后以服务器状态校准。日期仍按浏览器时区显示，日历按本地午夜拆分统计。

## 任务接口

| 方法 | 路径 | 内容 |
| --- | --- | --- |
| GET | `/api/tasks` | `{tasks:[], nodes:[], todos:[], progress:[]}` 快照；每条 progress 包含 `todo_id` 和由其待办推导的 `node_id` |
| POST | `/api/tasks` | `{title, description, status}` |
| PUT | `/api/tasks/:id` | `{title, description, status}` |
| DELETE | `/api/tasks/:id` | 删除任务及下属内容 |
| POST | `/api/tasks/:id/nodes` | `{title, description, status}` |
| PUT | `/api/nodes/:id` | `{title, description, status}` |
| DELETE | `/api/nodes/:id` | 删除节点及下属内容 |
| POST | `/api/nodes/:id/todos` | `{title}` |
| PUT | `/api/todos/:id` | `{title, done: boolean}` |
| DELETE | `/api/todos/:id` | 删除待办及其全部日志 |
| POST | `/api/todos/:id/progress` | `{content}`，向指定待办追加日志 |
| POST | `/api/nodes/:id/progress` | 旧写入接口返回 410，提示刷新页面后在具体待办内记录 |
| DELETE | `/api/progress/:id` | 删除日志 |

任务状态：`todo`, `doing`, `done`。节点状态：`open`, `doing`, `resolved`。创建成功返回 201 与 `{id}`；更新返回 200。校验失败返回 400，资源不存在返回 404，计时状态冲突返回 409。失败响应是 `{error: "说明"}`。空白标题或日志不可保存。

面板显示时每 15 秒刷新一次；打开编辑框时暂停自动刷新，避免干扰编辑。保存会立即刷新数据。多个设备编辑同一字段时，后保存者覆盖先保存者；进度日志逐条追加，不会整体覆盖其他设备的日志。此版本节点为任务下的平级问题列表，不包含自由连线画布或递归子节点。
