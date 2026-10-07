<script setup>
import { ref, reactive, computed, onMounted, onUnmounted, watch } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { api } from '../utils/api'
import NodeDetail from './NodeDetail.vue'
import ExpandableText from './ExpandableText.vue'
import PlainTextInput from './PlainTextInput.vue'
import NoteLocation from './NoteLocation.vue'

const props = defineProps({ active: Boolean })
const board = ref({ tasks: [], nodes: [], todos: [], progress: [] })
const selectedTaskId = ref(null)
const selectedNodeId = ref(null)
const query = ref('')
const filter = ref('all')
const loading = ref(false)
const busy = ref(false)
const error = ref('')
const taskLabels = { todo: '待办', doing: '进行中', done: '已完成' }
const nodeLabels = { open: '待解决', doing: '处理中', resolved: '已解决' }
const tasks = computed(() =>
  board.value.tasks.filter(
    (task) =>
      (filter.value === 'all' || task.status === filter.value) &&
      `${task.title} ${task.description}`
        .toLowerCase()
        .includes(query.value.trim().toLowerCase()),
  ),
)
const task = computed(() =>
  board.value.tasks.find((item) => item.id === selectedTaskId.value),
)
const nodes = computed(() =>
  board.value.nodes.filter((item) => item.task_id === selectedTaskId.value),
)
const node = computed(() =>
  nodes.value.find((item) => item.id === selectedNodeId.value),
)
const todos = computed(() =>
  board.value.todos.filter((item) => item.node_id === selectedNodeId.value),
)
const progress = computed(() =>
  board.value.progress.filter((item) => item.node_id === selectedNodeId.value),
)
// Keep unsaved text when moving between nodes during this visit.
const drafts = reactive({})
const nodeDraft = computed(() => {
  const id = selectedNodeId.value
  if (!drafts[id]) drafts[id] = { todo: '', progress: {}, expanded: {} }
  // Read through the reactive map so the first node's nested drafts update UI.
  return drafts[id]
})
function nodeCount(taskId) {
  return board.value.nodes.filter((item) => item.task_id === taskId).length
}
function completedCount(taskId) {
  return board.value.nodes.filter(
    (item) => item.task_id === taskId && item.status === 'resolved',
  ).length
}
function selectTask(id) {
  selectedTaskId.value = id
  selectedNodeId.value =
    board.value.nodes.find((item) => item.task_id === id)?.id ?? null
}
let version = 0,
  poll
async function load(showLoading = false) {
  const current = ++version
  if (showLoading) loading.value = true
  try {
    const result = await api('/api/tasks')
    if (current !== version) return
    board.value = result
    if (!result.tasks.some((item) => item.id === selectedTaskId.value))
      selectTask(tasks.value[0]?.id ?? null)
    if (!nodes.value.some((item) => item.id === selectedNodeId.value))
      selectedNodeId.value = nodes.value[0]?.id ?? null
    error.value = ''
  } catch (err) {
    if (current === version) error.value = `任务加载失败：${err.message}`
  } finally {
    if (current === version) loading.value = false
  }
}
async function mutate(path, method, body, onSaved) {
  if (busy.value) return false
  busy.value = true
  version++ // Ignore a read that began before this write.
  try {
    const result = await api(path, method, body)
    if (onSaved) onSaved(result)
    await load()
    ElMessage.success('已保存')
    return true
  } catch (err) {
    ElMessage.error(err.message)
    return false
  } finally {
    busy.value = false
  }
}
const editorVisible = ref(false)
const editorKind = ref('task')
const editorId = ref(null)
const editorParent = ref(null)
const form = ref({
  title: '',
  description: '',
  status: 'todo',
  note_location: '',
})
function edit(kind, item) {
  editorKind.value = kind
  editorId.value = item?.id ?? null
  editorParent.value =
    kind === 'node' ? selectedTaskId.value : selectedNodeId.value
  form.value = {
    title: item?.title ?? '',
    description: item?.description ?? '',
    status: item?.status ?? (kind === 'task' ? 'todo' : 'open'),
    done: item?.done ?? false,
    note_location: item?.note_location ?? '',
  }
  editorVisible.value = true
}
const editorTitle = computed(
  () =>
    `${editorId.value ? '编辑' : '新建'}${{ task: '任务', node: '问题节点', todo: '待办' }[editorKind.value]}`,
)
async function saveEditor() {
  const title = form.value.title.trim()
  if (!title) return
  const kind = editorKind.value,
    id = editorId.value
  let path, body
  if (kind === 'task') {
    path = id ? `/api/tasks/${id}` : '/api/tasks'
    body = {
      title,
      description: form.value.description,
      status: form.value.status,
    }
  } else if (kind === 'node') {
    path = id ? `/api/nodes/${id}` : `/api/tasks/${editorParent.value}/nodes`
    body = {
      title,
      description: form.value.description,
      status: form.value.status,
    }
  } else {
    path = `/api/todos/${id}`
    body = { title, done: form.value.done }
  }
  body.note_location = form.value.note_location
  const ok = await mutate(path, id ? 'PUT' : 'POST', body, (result) => {
    if (kind === 'task') {
      selectedTaskId.value = result.id
      query.value = ''
      filter.value = 'all'
    }
    if (kind === 'node') selectedNodeId.value = result.id
  })
  if (ok) editorVisible.value = false
}
async function remove(kind, id) {
  const messages = {
    tasks: '删除任务及其全部问题节点、待办和进度记录？此操作无法撤销。',
    nodes: '删除问题节点及其待办和进度记录？此操作无法撤销。',
    todos: '删除这条待办及其全部进度记录？此操作无法撤销。',
    progress: '删除这条进度记录？',
  }
  try {
    await ElMessageBox.confirm(messages[kind], '确认删除', {
      type: 'warning',
      confirmButtonText: '删除',
      cancelButtonText: '取消',
    })
  } catch {
    return
  }
  await mutate(`/api/${kind}/${id}`, 'DELETE')
}
function refreshVisible() {
  if (props.active && !document.hidden && !busy.value && !editorVisible.value)
    load()
}
watch(
  () => props.active,
  (active) => {
    if (active) refreshVisible()
  },
)
onMounted(() => {
  load(true)
  poll = setInterval(refreshVisible, 15000)
  document.addEventListener('visibilitychange', refreshVisible)
})
onUnmounted(() => {
  version++
  clearInterval(poll)
  document.removeEventListener('visibilitychange', refreshVisible)
})
</script>

<template>
  <section class="task-panel" v-loading="loading">
    <div class="panel-heading">
      <div>
        <h2>任务面板</h2>
        <p>把目标拆成问题，记录进展和下一步。</p>
      </div>
      <div>
        <el-button :disabled="busy" @click="load(true)">刷新</el-button
        ><el-button type="primary" :disabled="busy" @click="edit('task')"
          >新建任务</el-button
        >
      </div>
    </div>
    <el-alert
      v-if="error"
      :title="error"
      type="error"
      :closable="false"
      show-icon
    />
    <div class="board-layout">
      <aside class="task-sidebar">
        <el-input
          v-model="query"
          placeholder="搜索任务"
          aria-label="搜索任务"
          clearable
        />
        <el-select
          v-model="filter"
          aria-label="筛选任务状态"
          class="status-filter"
        >
          <el-option label="全部任务" value="all" /><el-option
            v-for="(label, value) in taskLabels"
            :key="value"
            :label="label"
            :value="value"
          />
        </el-select>
        <p class="muted">{{ tasks.length }} 个任务</p>
        <div class="task-list">
          <button
            v-for="item in tasks"
            :key="item.id"
            class="task-card"
            :disabled="busy"
            :class="{ selected: item.id === selectedTaskId }"
            @click="selectTask(item.id)"
          >
            <span class="task-title">{{ item.title }}</span>
            <span class="task-meta"
              ><span>{{ taskLabels[item.status] }}</span
              ><span
                >{{ completedCount(item.id) }}/{{
                  nodeCount(item.id)
                }}
                问题已解决</span
              ></span
            >
          </button>
        </div>
        <el-empty
          v-if="!tasks.length"
          :image-size="64"
          :description="
            board.tasks.length ? '没有匹配的任务' : '先创建一个任务吧'
          "
        />
      </aside>
      <main class="task-content" v-if="task">
        <div class="task-heading">
          <div>
            <el-tag :type="task.status === 'done' ? 'success' : 'primary'">{{
              taskLabels[task.status]
            }}</el-tag>
            <h2>{{ task.title }}</h2>
          </div>
          <div class="actions">
            <el-button :disabled="busy" @click="edit('task', task)"
              >编辑任务</el-button
            ><el-button
              type="danger"
              plain
              :disabled="busy"
              @click="remove('tasks', task.id)"
              >删除任务</el-button
            >
          </div>
        </div>
        <ExpandableText
          v-if="task.description"
          :key="task.id"
          :text="task.description"
          label="任务说明"
          class="description"
        />
        <p v-else class="muted">可在编辑任务中补充目标和说明。</p>
        <NoteLocation
          :key="`task-location-${task.id}`"
          :location="task.note_location"
        />
        <div class="section-heading">
          <h3>
            问题节点
            <span class="muted"
              >{{ completedCount(task.id) }}/{{ nodes.length }}</span
            >
          </h3>
          <el-button type="primary" plain :disabled="busy" @click="edit('node')"
            >添加问题节点</el-button
          >
        </div>
        <div v-if="nodes.length" class="node-grid">
          <button
            v-for="item in nodes"
            :key="item.id"
            class="node-card"
            :disabled="busy"
            :class="{
              selected: item.id === selectedNodeId,
              resolved: item.status === 'resolved',
            }"
            @click="selectedNodeId = item.id"
          >
            <span class="node-dot"></span
            ><span class="node-title">{{ item.title }}</span
            ><span class="muted">{{ nodeLabels[item.status] }}</span>
          </button>
        </div>
        <el-empty
          v-else
          :image-size="72"
          description="遇到了什么问题？添加一个节点，写下下一步。"
        />
        <NodeDetail
          v-if="node"
          :key="node.id"
          :node="node"
          :draft="nodeDraft"
          :todos="todos"
          :progress="progress"
          :busy="busy"
          :mutate="mutate"
          @edit-node="edit('node', node)"
          @delete-node="remove('nodes', node.id)"
          @edit-todo="(item) => edit('todo', item)"
          @delete-todo="(id) => remove('todos', id)"
          @delete-progress="(id) => remove('progress', id)"
        />
      </main>
      <main v-else class="task-content">
        <el-empty description="新建或选择任务，开始记录进展" />
      </main>
    </div>
    <el-dialog
      v-model="editorVisible"
      :title="editorTitle"
      width="min(860px,94vw)"
      top="5vh"
      :close-on-click-modal="false"
      :show-close="!busy"
      :close-on-press-escape="!busy"
    >
      <el-form label-position="top" @submit.prevent="saveEditor">
        <el-form-item label="标题" required
          ><el-input
            v-model="form.title"
            :disabled="busy"
            maxlength="200"
            show-word-limit
            aria-label="标题"
        /></el-form-item>
        <template v-if="editorKind !== 'todo'">
          <el-form-item label="说明"
            ><PlainTextInput
              v-model="form.description"
              :disabled="busy"
              :min-rows="7"
              label="说明"
              placeholder="描述目标、问题现象或需要补充的背景"
          /></el-form-item>
          <el-form-item label="状态"
            ><el-select v-model="form.status" :disabled="busy" aria-label="状态"
              ><el-option
                v-for="(label, value) in editorKind === 'task'
                  ? taskLabels
                  : nodeLabels"
                :key="value"
                :label="label"
                :value="value" /></el-select
          ></el-form-item>
        </template>
        <el-form-item label="笔记位置（可选）">
          <el-input
            v-model="form.note_location"
            type="textarea"
            :autosize="{ minRows: 2, maxRows: 5 }"
            :disabled="busy"
            maxlength="1000"
            show-word-limit
            aria-label="笔记位置"
            placeholder="例如：个人电脑 D:\Notes\实验记录.md，或共享盘目录、笔记本页码"
          />
          <p class="location-hint">
            填写便于自己或他人定位的文字说明。仅记录位置，不读取或上传笔记。
          </p>
        </el-form-item>
      </el-form>
      <template #footer
        ><el-button :disabled="busy" @click="editorVisible = false"
          >取消</el-button
        ><el-button
          type="primary"
          :loading="busy"
          :disabled="!form.title.trim()"
          @click="saveEditor"
          >保存</el-button
        ></template
      >
    </el-dialog>
  </section>
</template>

<style scoped>
.location-hint {
  margin: 6px 0 0;
  color: #909399;
  font-size: 12px;
  line-height: 1.6;
}
h2,
h3,
p {
  margin-top: 0;
}
h2 {
  font-size: 20px;
}
h3 {
  font-size: 16px;
}
.panel-heading,
.task-heading,
.section-heading {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  margin-bottom: 18px;
}
.panel-heading p,
.muted {
  color: #909399;
  font-size: 13px;
}
.panel-heading p {
  margin-bottom: 0;
}
.board-layout {
  display: grid;
  grid-template-columns: 260px minmax(0, 1fr);
  border: 1px solid #e4e7ed;
  border-radius: 12px;
  overflow: hidden;
  background: white;
}
.task-sidebar {
  padding: 18px;
  background: #fafbfc;
  border-right: 1px solid #e4e7ed;
}
.status-filter {
  width: 100%;
  margin: 12px 0 16px;
}
.task-list {
  display: grid;
  gap: 10px;
}
.task-card {
  width: 100%;
  text-align: left;
  border: 1px solid #e4e7ed;
  background: white;
  border-radius: 8px;
  padding: 14px;
  cursor: pointer;
  color: inherit;
}
.task-card.selected,
.node-card.selected {
  border-color: #409eff;
  background: #ecf5ff;
}
.task-title {
  display: block;
  font-weight: 600;
  overflow-wrap: anywhere;
  margin-bottom: 12px;
}
.task-meta {
  display: flex;
  justify-content: space-between;
  gap: 8px;
  font-size: 12px;
  color: #909399;
}
.task-content {
  min-width: 0;
  padding: 24px;
}
.task-heading {
  align-items: flex-start;
}
.task-heading h2 {
  margin: 10px 0 0;
  overflow-wrap: anywhere;
}
.actions {
  display: flex;
  flex-shrink: 0;
}
.description {
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  line-height: 1.7;
  font-size: 14px;
}
.section-heading {
  margin-top: 28px;
}
.section-heading h3 {
  margin: 0;
}
.node-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px;
  margin-bottom: 24px;
}
.node-card {
  display: flex;
  text-align: left;
  align-items: center;
  gap: 8px;
  padding: 14px;
  border: 1px solid #dcdfe6;
  border-radius: 8px;
  background: white;
  color: inherit;
  cursor: pointer;
}
.node-title {
  flex: 1;
  min-width: 0;
  overflow-wrap: anywhere;
  font-size: 14px;
}
.node-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #e6a23c;
  flex-shrink: 0;
}
.resolved .node-dot {
  background: #67c23a;
}
.node-card .muted {
  flex-shrink: 0;
  font-size: 12px;
}
@media (max-width: 800px) {
  .board-layout {
    grid-template-columns: 1fr;
  }
  .task-sidebar {
    border-right: none;
    border-bottom: 1px solid #e4e7ed;
  }
  .task-list {
    max-height: 260px;
    overflow: auto;
  }
  .task-content {
    padding: 18px;
  }
}
@media (max-width: 560px) {
  .panel-heading,
  .task-heading {
    align-items: flex-start;
    flex-direction: column;
  }
  .node-grid {
    grid-template-columns: 1fr;
  }
}
</style>
