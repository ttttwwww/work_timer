<script setup>
import { ref, computed } from 'vue'
import { formatDuration } from '../utils/time'

const props = defineProps({
  dailyStats: Object,
  saveNote: Function,
})

const emit = defineEmits(['delete'])

const drawerVisible = ref(false)
const selectedDate = ref('')

// 使用 computed 让 currentLogs 自动响应 dailyStats 的变化
const currentLogs = computed(() => {
  if (selectedDate.value && props.dailyStats[selectedDate.value]) {
    return props.dailyStats[selectedDate.value].logs
  }
  return []
})

// 笔记编辑对话框
const dialogVisible = ref(false)
const editingLog = ref(null)
const tempNote = ref('')

// 打开抽屉
const handleDateClick = (data) => {
  selectedDate.value = data.day
  drawerVisible.value = true
}

// 处理删除点击
const handleDeleteClick = (id) => {
  emit('delete', id)
}

// 打开笔记编辑对话框
const openNoteDialog = (log) => {
  editingLog.value = log
  tempNote.value = log.note || ''
  dialogVisible.value = true
}

// 保存笔记
const savingNote = ref(false)
const saveNote = async () => {
  if (!editingLog.value || savingNote.value) return
  savingNote.value = true
  try {
    if (await props.saveNote(editingLog.value.id, tempNote.value))
      dialogVisible.value = false
  } finally {
    savingNote.value = false
  }
}

// --- 可视化核心算法 ---
// 计算某个时间块在 24小时进度条中的位置和宽度
const calculateBarStyle = (log) => {
  const start = log.displayStart
  const midnight = new Date(
    start.getFullYear(),
    start.getMonth(),
    start.getDate(),
  ).getTime()
  const next = new Date(
    start.getFullYear(),
    start.getMonth(),
    start.getDate() + 1,
  ).getTime()
  return {
    left: `${(100 * (start.getTime() - midnight)) / (next - midnight)}%`,
    width: `${(100 * (log.displayEnd - start)) / (next - midnight)}%`,
    backgroundColor: log.type === 'formal' ? '#409EFF' : '#67C23A',
  }
}
const formatTimeRange = (log) => {
  const format = (d) =>
    d.toLocaleTimeString([], {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    })
  const endLabel =
    log.displayEnd.getDate() !== log.displayStart.getDate()
      ? '24:00:00'
      : format(log.displayEnd)
  return `${format(log.displayStart)} - ${endLabel}${log.ongoing ? '（进行中）' : ''}`
}
</script>

<template>
  <el-card class="calendar-card">
    <template #header><span>📅 工作记录日历</span></template>
    <el-calendar>
      <template #date-cell="{ data }">
        <div
          class="date-cell"
          :class="{ 'has-work': dailyStats[data.day] }"
          @click="handleDateClick(data)"
        >
          <p class="day-number">{{ data.day.split('-').slice(2).join('') }}</p>
          <div v-if="dailyStats[data.day]" class="work-tag">
            ⏱ {{ dailyStats[data.day].totalHours }}h
          </div>
        </div>
      </template>
    </el-calendar>
  </el-card>

  <el-drawer
    v-model="drawerVisible"
    :title="selectedDate + ' 时间分布'"
    size="min(620px, 100vw)"
  >
    <div class="visual-container" v-if="currentLogs.length">
      <div class="time-ruler">
        <span>00:00</span><span>06:00</span><span>12:00</span><span>18:00</span
        ><span>24:00</span>
      </div>

      <div class="timeline-track">
        <el-tooltip
          v-for="log in currentLogs"
          :key="log.id"
          :content="`${log.type === 'formal' ? '💼' : '🐟'} ${formatTimeRange(log)}`"
          placement="top"
        >
          <div class="time-block" :style="calculateBarStyle(log)"></div>
        </el-tooltip>
      </div>
    </div>

    <div class="list-container">
      <h3>📋 详细记录</h3>
      <el-empty v-if="!currentLogs.length" description="这一天还没有计时记录" />
      <el-timeline>
        <el-timeline-item
          v-for="log in currentLogs"
          :key="log.id"
          :type="log.type === 'formal' ? 'primary' : 'success'"
          :timestamp="formatTimeRange(log)"
        >
          <div class="log-item">
            <div class="log-header">
              <span class="log-title">
                {{ log.type === 'formal' ? '💼 正式工作' : '🐟 摸鱼学习' }}
                ({{ formatDuration(log.duration) }})
              </span>
              <el-button
                type="danger"
                size="small"
                @click="handleDeleteClick(log.id)"
              >
                删除
              </el-button>
            </div>

            <div class="log-note-preview" @click="openNoteDialog(log)">
              <div v-if="log.note" class="note-content">
                {{ log.note }}
              </div>
              <div v-else class="note-placeholder">点击添加笔记...</div>
            </div>
          </div>
        </el-timeline-item>
      </el-timeline>
    </div>
  </el-drawer>

  <!-- 笔记编辑对话框 -->
  <el-dialog
    v-model="dialogVisible"
    title="编辑笔记"
    width="min(500px, 94vw)"
    :close-on-click-modal="false"
    :close-on-press-escape="!savingNote"
    :show-close="!savingNote"
  >
    <el-input
      v-model="tempNote"
      :disabled="savingNote"
      type="textarea"
      :rows="10"
      placeholder="请输入笔记内容..."
      maxlength="500"
      show-word-limit
    />

    <template #footer>
      <el-button :disabled="savingNote" @click="dialogVisible = false"
        >取消</el-button
      >
      <el-button type="primary" :loading="savingNote" @click="saveNote"
        >确定</el-button
      >
    </template>
  </el-dialog>
</template>

<style scoped>
/* 可视化条样式 */
.visual-container {
  margin-bottom: 40px;
  padding: 10px;
  background-color: #f5f7fa;
  border-radius: 8px;
}
.time-ruler {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  color: #909399;
  margin-bottom: 5px;
}
.timeline-track {
  position: relative;
  height: 30px;
  background-color: #e4e7ed;
  border-radius: 15px;
  overflow: hidden; /* 防止溢出 */
  width: 100%;
}
.time-block {
  position: absolute;
  height: 100%;
  top: 0;
  cursor: pointer;
  transition: opacity 0.2s;
}
.time-block:hover {
  opacity: 0.8;
  box-shadow: 0 0 5px rgba(0, 0, 0, 0.2);
}

/* 复用之前的日历样式 */
.date-cell {
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  cursor: pointer;
}
.date-cell.has-work {
  background-color: #f0f9eb;
}
.work-tag {
  background-color: #67c23a;
  color: white;
  border-radius: 4px;
  font-size: 12px;
  padding: 2px 6px;
}

/* 详细记录样式 */
.list-container {
  margin-top: 30px;
}

.log-item {
  width: 100%;
}

.log-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.log-title {
  font-weight: bold;
  font-size: 14px;
}

.log-note-preview {
  margin-top: 8px;
  padding: 10px;
  border: 1px dashed #dcdfe6;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.3s;
  min-height: 40px;
}

.log-note-preview:hover {
  border-color: #409eff;
  background-color: #f5f7fa;
}

.note-content {
  color: #303133;
  font-size: 13px;
  line-height: 1.5;
  word-break: break-word;
  white-space: pre-wrap; /* 保留换行和空格 */
}

.note-placeholder {
  color: #c0c4cc;
  font-size: 13px;
  font-style: italic;
}
</style>
