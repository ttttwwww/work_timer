# HTTP API 参考

[返回 README](../README.md) · [使用指南](usage.md) · [系统架构](architecture.md) · [数据流](data-flow.md) · [数据库](database.md)

接口定义在 [WebServer.cpp](../backend/src/WebServer.cpp)，数据库方法在 [db.hpp](../backend/include/db.hpp)。本文描述当前实现，示例 ID 和时间均为示意值，实际调用时使用服务器返回的数据。

## 1. 公共约定

- 默认服务地址为 `http://localhost:8080`。浏览器使用同源的 `/api/...` 路径；Vite 开发时由代理转发。
- 有请求体时发送 JSON 对象及 `Content-Type: application/json`。GET 和 DELETE 不需要请求体。
- 当前接口无登录鉴权和用户字段，作用于这个服务的共享数据库。
- 时间戳为 Unix **秒**，不是 JavaScript 毫秒；创建时间由服务器填写。
- JSON 中的 ID 必须是正整数，不能用字符串、小数或布尔值代替；当前 ID 范围为 1 到 2147483647。路径中的 `:id` 表示实际整数，不包含冒号。各路径对非正数的处理不完全统一：PUT 显式返回 400，创建子资源/删除的查询通常返回 404，不应依赖非法路径值进行业务判断。
- 显式传入的 `end_time` 必须为 1 到 9007199254740991 的 JSON 整数，之后还要通过会话时间范围校验。
- PUT 是提交指定可编辑字段的完整集合，不是 PATCH；未提供必需字段会失败；新增的可选 `note_location` 省略时保留原值。不能通过更新接口改变对象的父级归属。
- 业务响应为 JSON，并设置 `Cache-Control: no-store`。未匹配路由或 HTTP 框架产生的错误不保证使用下述 JSON 格式。

### 文本和状态

| 字段 | 后端限制 | 其他要求 |
| --- | --- | --- |
| 任务/节点/待办 `title` | 最多 800 个 UTF-8 字节 | 必须是字符串，不能全为空格、Tab 或换行 |
| 任务/节点 `description` | 最多 20000 个 UTF-8 字节 | 必须提供字符串，允许 `""` |
| 任务/节点/待办 `note_location` | 最多 4000 个 UTF-8 字节 | 可省略；提供时必须为字符串，不能为 `null`；`""` 清空 |
| 日志 `content` | 最多 20000 个 UTF-8 字节 | 必须提供非空白字符串 |
| 计时 `note` | 最多 20000 个 UTF-8 字节 | 必须提供字符串，允许 `""` 清空 |
| 计时 `type` | `formal` / `informal` | 正式工作 / 摸鱼学习 |
| 任务 `status` | `todo` / `doing` / `done` | 待办 / 进行中 / 已完成 |
| 节点 `status` | `open` / `doing` / `resolved` | 待解决 / 处理中 / 已解决 |
| 待办 `done` | JSON `true` / `false` | 不接受数字 0/1 或字符串 `"false"` |

前端标题输入上限为 200，任务/节点说明、日志和计时笔记输入上限为 5000，笔记位置为 1000；它们是输入控件限制，不等于后端 UTF-8 字节限制。API 不自动去除文本首尾空白，界面会对标题和新日志先执行 `trim()`。数据库的默认值也不代表 API 可以省略字段。

### 响应与错误

| 状态码 | 含义 |
| --- | --- |
| 200 | 查询、更新或删除成功 |
| 201 | 创建成功，返回 `{"id": 新ID}` |
| 400 | JSON 格式、字段类型、文本、状态或结束时间范围不合法 |
| 404 | 对象/父对象不存在，或删除资源类型不支持 |
| 409 | 计时冲突：重复开始、停止非活跃记录、删除活跃记录 |
| 410 | 旧节点日志写入接口已停用 |
| 500 | 数据库等服务端处理失败 |

业务错误示例：

```json
{"error":"已有计时在进行中，请先结束"}
```

## 2. 接口与数据库操作总览

这里把路径、C++ 方法和数据表连起来。状态码与示例见后续各节；表字段和外键细节只在[数据库文档](database.md)维护。

| 方法与路径 | Database 方法 | 查询或变更 |
| --- | --- | --- |
| `GET /api/state` | `getAllSessions()` | 查询 `sessions`，路由附加 `server_time` |
| `GET /api/history` | `getAllSessions()` | 查询 `sessions`，返回原有数组格式 |
| `POST /api/start` | `startSession()` | 确认无活跃记录后插入 `sessions` |
| `POST /api/stop` | `stopSession()` | 校验并更新指定 `sessions.end_time` |
| `POST /api/note` | `addNoteToSession()` | 更新指定 `sessions.note` |
| `POST /api/delete` | `deleteSession()` | 删除已结束的 `sessions` 记录 |
| `GET /api/tasks` | `getBoard()` | 查询 `tasks`、`problem_nodes`、`node_todos`、`todo_progress`，日志连接待办表 |
| `POST /api/tasks` | `saveTask(0, ...)` | 插入 `tasks` |
| `PUT /api/tasks/:id` | `saveTask(id, ...)` | 更新 `tasks` |
| `DELETE /api/tasks/:id` | `deleteBoardItem("tasks", id)` | 删除任务，外键级联删除下属内容 |
| `POST /api/tasks/:id/nodes` | `saveNode(0, taskId, ...)` | 检查任务存在，插入 `problem_nodes` |
| `PUT /api/nodes/:id` | `saveNode(id, 0, ...)` | 更新 `problem_nodes`，保留 `task_id` |
| `DELETE /api/nodes/:id` | `deleteBoardItem("nodes", id)` | 删除节点及其待办、日志 |
| `POST /api/nodes/:id/todos` | `saveTodo(0, nodeId, ...)` | 检查节点存在，插入未完成的 `node_todos` |
| `PUT /api/todos/:id` | `saveTodo(id, 0, ...)` | 更新 `node_todos` 标题、完成标记及可选笔记位置 |
| `DELETE /api/todos/:id` | `deleteBoardItem("todos", id)` | 删除待办及其 `todo_progress` |
| `POST /api/todos/:id/progress` | `addProgress()` | 检查待办存在，插入 `todo_progress` |
| `DELETE /api/progress/:id` | `deleteBoardItem("progress", id)` | 删除指定 `todo_progress` |
| `POST /api/nodes/:id/progress` | 不调用数据库 | 兼容提示，返回 410 |

DELETE 路由由 `/api/<string>/<int>` 统一接收，但只允许 `tasks`、`nodes`、`todos`、`progress` 四种资源。计时删除仍用 `POST /api/delete`。

## 3. 计时接口

### GET /api/state 与 GET /api/history

无参数。`GET /api/state` 返回 200：

```json
{
  "server_time": 1700003600,
  "sessions": [
    {"id": 8, "start_time": 1700003000, "end_time": 0, "type": "formal", "note": ""},
    {"id": 7, "start_time": 1700000000, "end_time": 1700001200, "type": "informal", "note": "整理实验记录"}
  ]
}
```

`server_time` 是响应生成时的服务器时间。`end_time=0` 表示活跃计时；旧库中的 NULL 也规范化为 0。会话按 `start_time DESC, id DESC` 排序，没有分页或日期过滤。没有记录时 `sessions` 为 `[]`。

`GET /api/history` 返回完全相同的会话数组，但没有外层对象与 `server_time`；保留该接口用于兼容。当前页面校时使用 `/api/state`。`note` 对外为字符串，旧库中的 NULL 读为 `""`。

### POST /api/start

请求：

```json
{"type":"formal"}
```

成功返回 **201**：

```json
{"id":8}
```

只接收计时类型，开始时间取服务器时间。已有活跃计时返回 409，类型错误返回 400。没有暂停、恢复或覆盖当前记录的隐含行为。

### POST /api/stop

请求，`id` 必需，`end_time` 可选：

```json
{"id":8,"end_time":1700003500}
```

省略 `end_time` 时，使用服务器收到请求时的当前时间。指定时间必须满足 `start_time ≤ end_time ≤ now`，允许零秒会话；示例时间只有在对应会话和当前时间满足条件时才有效。

成功返回 **200**：

```json
{"ok":true}
```

记录不存在或已经结束均返回 **409**；时间范围或字段无效返回 400。接口要求指定 ID，避免旧页面误结束另一设备刚创建的会话。

### POST /api/note

请求：

```json
{"id":8,"note":"已完成第一轮验证\n下一步检查误差"}
```

成功返回 `200 {"ok":true}`。空字符串清空笔记；可更新活跃或已结束的记录。记录不存在返回 404，字段无效返回 400。

### POST /api/delete

请求：

```json
{"id":7}
```

成功返回 `200 {"ok":true}`。删除活跃记录返回 409，不存在返回 404。对跨日计时删除的是整条记录，而不是某一天的显示片段。

## 4. 读取任务面板

### GET /api/tasks

无参数，返回 **200** 与四个数组：

```json
{
  "tasks": [
    {"id":1,"title":"复现实验","description":"复现基线结果","status":"doing","created_at":1700000000,"updated_at":1700000300,"note_location":"个人电脑：D:\\Notes\\实验记录.md"}
  ],
  "nodes": [
    {"id":2,"task_id":1,"title":"准确率偏低","description":"核对数据预处理","status":"open","created_at":1700000100,"updated_at":1700000100,"note_location":"实验记录.md 第3节"}
  ],
  "todos": [
    {"id":3,"node_id":2,"title":"检查输入归一化","done":false,"note_location":""}
  ],
  "progress": [
    {"id":4,"node_id":2,"todo_id":3,"content":"已核对训练集，下一步检查测试集","created_at":1700000200}
  ]
}
```

这是全服务的面板数据集合，不只包含任务列表；没有分页、服务端搜索参数或单对象 GET 接口。空库仍返回四个空数组。前端在本地筛选选中的任务、节点与待办。

| 数组 | 排序 | 与数据库的差别 |
| --- | --- | --- |
| `tasks` | `id DESC` | 对应任务字段 |
| `nodes` | `id ASC` | JSON 名称是 nodes，表名是 `problem_nodes` |
| `todos` | `id ASC` | SQLite 的 `done` 0/1 转为 JSON 布尔值 |
| `progress` | `id DESC` | `node_id` 由 JOIN 待办表推导，日志表只存 `todo_id` |

列表称为“快照”表示一次 API 返回的面板集合，不承诺隔离其他进程直接写库的跨表事务，详见[数据库一致性](database.md#5-索引删除与一致性)。

## 5. 创建和更新任务、节点、待办

以下所有创建/更新接口均支持可选字符串 `note_location`。POST 省略时保存 `""`；PUT 省略时保留现值，显式传 `""` 清空。`null`、非字符串或超过 4000 个 UTF-8 字节返回 400。该字段只保存原文，不读取服务器或个人设备上的文件，也不提供上传、下载或打开文件的接口。

例如在下列任务请求的三个必填字段之外，可增加 `"note_location":"个人电脑：D:\\Notes\\实验记录.md"`；JSON 中的反斜杠需转义。GET 返回的任务、节点和待办均包含此字段，即使为空。

### POST /api/tasks 与 PUT /api/tasks/:id

两个接口都必须提供以下三个字段，`description` 可为空字符串：

```json
{"title":"复现实验","description":"复现基线结果","status":"doing"}
```

POST 成功返回 `201 {"id":1}`；PUT 成功返回 `200 {"id":1}`。服务器负责时间字段，PUT 保留创建时间。更新对象不存在返回 404，字段/状态无效返回 400。

### POST /api/tasks/:id/nodes 与 PUT /api/nodes/:id

创建路径中的 ID 为任务 ID，更新路径中的 ID 为节点 ID。两者都必须提供：

```json
{"title":"准确率偏低","description":"核对预处理","status":"open"}
```

POST 返回 `201 {"id":2}`；PUT 返回 `200 {"id":2}`。创建时任务不存在或更新时节点不存在返回 404。更新不能移动节点到另一个任务，也不会修改任务状态。

### POST /api/nodes/:id/todos

路径 ID 为节点 ID，请求：

```json
{"title":"检查输入归一化"}
```

成功返回 `201 {"id":3}`，新待办始终为未完成。父节点不存在返回 404。创建请求没有可设置的 `done` 字段。

### PUT /api/todos/:id

必须同时提供标题和 JSON 布尔值：

```json
{"title":"检查训练集与测试集的归一化","done":true}
```

成功返回 `200 {"id":3}`。不存在返回 404，缺字段或错误类型返回 400。即使只切换完成状态，也要传当前标题；更新不删除日志，不改变所属节点或上级状态。

## 6. 追加与删除进度

### POST /api/todos/:id/progress

路径 ID 为待办 ID，请求：

```json
{"content":"已核对训练集\n下一步检查测试集预处理"}
```

成功返回 `201 {"id":4}`，ID 是新日志 ID，`created_at` 由服务器生成。待办不存在返回 404，正文为空白/超长或类型错误返回 400。已完成待办仍可追加日志。

每次成功 POST 新增一条，不会覆盖旧日志；没有编辑接口或幂等键。网络响应丢失后直接重试可能重复追加，应先读取面板确认。

### DELETE /api/progress/:id

路径 ID 为日志 ID，无请求体。成功返回 `200 {"ok":true}`；日志不存在返回 404。只删除该条日志。

### POST /api/nodes/:id/progress（已停用）

不写入数据库，返回 **410**：

```json
{"error":"进度记录已移入待办，请刷新页面后在具体待办内记录"}
```

旧客户端需要改用待办接口，不能把原节点 ID 直接当作待办 ID。旧数据如何保留见[迁移说明](database.md#6-初始化与迁移)。

## 7. 删除任务、节点和待办

以下接口都不需要请求体，成功返回 `200 {"ok":true}`，对象不存在返回 404：

| 路径 | 删除范围 |
| --- | --- |
| `DELETE /api/tasks/:id` | 指定任务及其全部节点、待办、日志 |
| `DELETE /api/nodes/:id` | 指定节点及其待办、日志 |
| `DELETE /api/todos/:id` | 指定待办及其日志 |

前端会弹出确认框，API 本身不会再要求确认，也没有回收站。级联删除来自数据库外键，服务端并非逐条调用其他 HTTP 接口。保留的旧节点日志表也会随所属节点删除。

## 8. 最小调用示例

以下示例针对本机运行的服务，使用 Bash/WSL。先创建任务：

```bash
curl -i http://localhost:8080/api/tasks \
  -H 'Content-Type: application/json' \
  -d '{"title":"复现实验","description":"核对基线","status":"todo"}'
```

将返回的任务 ID 填入下一个路径；假设是 1：

```bash
curl -i http://localhost:8080/api/tasks/1/nodes \
  -H 'Content-Type: application/json' \
  -d '{"title":"准确率偏低","description":"核对预处理","status":"open"}'
```

再用返回的节点 ID 调用 `POST /api/nodes/:id/todos`，用返回的待办 ID 调用 `POST /api/todos/:id/progress`。各表独立分配 ID，不能推断它们相同。最后用下面的查询确认归属：

```bash
curl http://localhost:8080/api/tasks
```

## 9. 静态资源与维护入口

`GET /` 提供 `dist/index.html`，`GET /assets/<path>` 提供构建资源，它们不是 JSON API。生产环境由同一个 Crow 服务处理，不需要额外的前端服务器。

修改路由、字段或状态码时同步更新本文及 [tests/test_api.py](../tests/test_api.py)；修改表结构时同步更新 [database.md](database.md)。涉及多端同步或组件状态时，再核对 [data-flow.md](data-flow.md)。
