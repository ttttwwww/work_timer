<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import WorkControls from './components/WorkControls.vue'
import HistoryCalendar from './components/HistoryCalendar.vue'
import TaskPanel from './components/TaskPanel.vue'
import { api } from './utils/api'
import {
  createServerClock,
  aggregateDays,
  formatDuration,
  toLocalInput,
} from './utils/time'

const tab = ref('timer')
const rawWorkLogs = ref([])
const active = computed(() => rawWorkLogs.value.find((log) => !log.endTime))
const serverNow = ref(0)
const clock = createServerClock()
const busy = ref(false)
const ready = ref(false)
const connectionError = ref('')
const currentSessionTime = computed(() =>
  active.value
    ? Math.max(
        0,
        Math.floor((serverNow.value - active.value.startTime.getTime()) / 1000),
      )
    : 0,
)
const dailyStats = computed(() =>
  aggregateDays(rawWorkLogs.value, serverNow.value),
)
let tick,
  poll,
  requestVersion = 0

async function syncState() {
  const version = ++requestVersion
  const sent = performance.now()
  try {
    const data = await api('/api/state')
    if (version !== requestVersion) return
    clock.sync(data.server_time, performance.now() - sent)
    serverNow.value = clock.now()
    rawWorkLogs.value = data.sessions.map((item) => ({
      id: item.id,
      type: item.type,
      note: item.note || '',
      startTime: new Date(item.start_time * 1000),
      endTime: item.end_time ? new Date(item.end_time * 1000) : null,
    }))
    ready.value = true
    connectionError.value = ''
  } catch (error) {
    if (version === requestVersion)
      connectionError.value = `连接失败：${error.message}`
  }
}
async function mutate(path, body, success) {
  if (busy.value) return false
  busy.value = true
  try {
    await api(path, 'POST', body)
    await syncState()
    ElMessage.success(success)
    return true
  } catch (error) {
    ElMessage.error(error.message)
    await syncState()
    return false
  } finally {
    busy.value = false
  }
}
const handleStart = (type) => mutate('/api/start', { type }, '已开始计时')
const stopVisible = ref(false)
const stoppingSession = ref(null)
const stopLimit = ref(0)
const stopInput = ref('')
const stopEnd = computed(() => new Date(stopInput.value).getTime())
const stopValid = computed(
  () =>
    Number.isFinite(stopEnd.value) &&
    stoppingSession.value &&
    stopEnd.value >= stoppingSession.value.startTime.getTime() &&
    stopEnd.value <= stopLimit.value,
)
const keptSeconds = computed(() =>
  stopValid.value
    ? (stopEnd.value - stoppingSession.value.startTime.getTime()) / 1000
    : 0,
)
const trimmedSeconds = computed(() =>
  stopValid.value ? (stopLimit.value - stopEnd.value) / 1000 : 0,
)
function openStop() {
  if (!active.value) return
  stoppingSession.value = { ...active.value }
  stopLimit.value = Math.floor(clock.now() / 1000) * 1000
  stopInput.value = toLocalInput(stopLimit.value)
  stopVisible.value = true
}
async function confirmStop() {
  if (!stopValid.value) return
  if (
    await mutate(
      '/api/stop',
      {
        id: stoppingSession.value.id,
        end_time: Math.floor(stopEnd.value / 1000),
      },
      '记录已保存',
    )
  )
    stopVisible.value = false
}
async function handleDelete(id) {
  if (!rawWorkLogs.value.find((log) => log.id === id)?.endTime)
    return ElMessage.warning('请先结束计时再删除记录')
  try {
    await ElMessageBox.confirm(
      '删除这条工作记录？此操作无法撤销。',
      '删除记录',
      { type: 'warning', confirmButtonText: '删除', cancelButtonText: '取消' },
    )
  } catch {
    return
  }
  await mutate('/api/delete', { id }, '记录已删除')
}
const handleUpdateNote = (id, note) =>
  mutate('/api/note', { id, note }, '笔记已保存')
function onVisible() {
  if (!document.hidden) syncState()
}
onMounted(() => {
  syncState()
  tick = setInterval(() => {
    if (ready.value) serverNow.value = clock.now()
  }, 1000)
  poll = setInterval(() => {
    if (!document.hidden && !busy.value) syncState()
  }, 15000)
  document.addEventListener('visibilitychange', onVisible)
})
onUnmounted(() => {
  requestVersion++
  clearInterval(tick)
  clearInterval(poll)
  document.removeEventListener('visibilitychange', onVisible)
})
</script>

<template>
  <div class="app-container">
    <header class="app-header">
      <div>
        <h1>牛马钟</h1>
        <p>记录时间，也记录每一步进展。</p>
      </div>
      <el-tag v-if="active" type="success"
        >正在计时 · {{ formatDuration(currentSessionTime) }}</el-tag
      >
    </header>
    <el-alert
      v-if="connectionError"
      :title="connectionError"
      type="error"
      :closable="false"
      show-icon
    >
      <el-button text @click="syncState">重新连接</el-button>
    </el-alert>
    <el-tabs v-model="tab">
      <el-tab-pane label="工作计时" name="timer">
        <WorkControls
          :is-working="!!active"
          :work-type="active?.type"
          :current-session-time="currentSessionTime"
          :disabled="busy || !ready || !!connectionError"
          @start="handleStart"
          @stop="openStop"
        />
        <HistoryCalendar
          :daily-stats="dailyStats"
          :save-note="handleUpdateNote"
          @delete="handleDelete"
        />
      </el-tab-pane>
      <el-tab-pane label="任务面板" name="tasks" lazy
        ><TaskPanel :active="tab === 'tasks'"
      /></el-tab-pane>
    </el-tabs>
    <el-dialog
      v-model="stopVisible"
      title="确认结束计时"
      width="min(500px, 94vw)"
      :close-on-click-modal="false"
      :show-close="!busy"
      :close-on-press-escape="!busy"
    >
      <p>如果忘记关闭计时，可以把结束时间提前，删去实际未工作的时间。</p>
      <p v-if="stoppingSession">
        开始时间：{{ stoppingSession.startTime.toLocaleString() }}
      </p>
      <label for="stop-time">实际结束时间（本地时区）</label>
      <input
        id="stop-time"
        class="stop-time"
        type="datetime-local"
        step="1"
        v-model="stopInput"
        :min="
          stoppingSession && toLocalInput(stoppingSession.startTime.getTime())
        "
        :max="toLocalInput(stopLimit)"
        :disabled="busy"
      />
      <el-button
        text
        :disabled="busy"
        @click="stopInput = toLocalInput(stopLimit)"
        >恢复为点击结束时的时间</el-button
      >
      <p v-if="stopValid">
        保留 <strong>{{ formatDuration(keptSeconds) }}</strong> · 扣除
        {{ formatDuration(trimmedSeconds) }}
      </p>
      <el-alert
        v-else
        title="请选择开始时间至点击结束时之间的时间"
        type="warning"
        :closable="false"
      />
      <template #footer
        ><el-button :disabled="busy" @click="stopVisible = false"
          >继续计时</el-button
        ><el-button
          type="primary"
          :loading="busy"
          :disabled="!stopValid"
          @click="confirmStop"
          >确认并保存</el-button
        ></template
      >
    </el-dialog>
  </div>
</template>

<style>
body {
  margin: 0;
  background: #f5f7fa;
  color: #303133;
  font-family:
    system-ui,
    -apple-system,
    'Segoe UI',
    sans-serif;
}
* {
  box-sizing: border-box;
}
button,
input,
textarea {
  font: inherit;
}
</style>
<style scoped>
.app-container {
  max-width: 1120px;
  margin: 28px auto;
  padding: 0 20px;
}
.app-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  margin-bottom: 18px;
}
h1 {
  font-size: 26px;
  margin: 0;
}
.app-header p {
  color: #909399;
  margin: 8px 0 0;
  font-size: 14px;
}
.stop-time {
  display: block;
  width: 100%;
  padding: 10px;
  margin: 10px 0;
  border: 1px solid #dcdfe6;
  border-radius: 6px;
}
@media (max-width: 600px) {
  .app-container {
    padding: 0 12px;
  }
  .app-header {
    flex-wrap: wrap;
  }
}
</style>
