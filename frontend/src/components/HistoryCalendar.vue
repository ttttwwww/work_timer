<script setup>
import { ref, computed } from 'vue'

const props = defineProps({
  dailyStats: Object // 接收父组件处理好的数据
})

const emit = defineEmits(['delete', 'update-note'])

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
const saveNote = () => {
  if (editingLog.value) {
    emit('update-note', editingLog.value.id, tempNote.value)
    dialogVisible.value = false
  }
}

// --- 可视化核心算法 ---
// 计算某个时间块在 24小时进度条中的位置和宽度
const calculateBarStyle = (log) => {
  const date = new Date(log.startTime)
  const end = log.endTime || new Date();
  const duration = (end-date) / 1000; // 持续时间，单位秒
  // 算出这是当天的第几秒 (0 - 86400)
  const startSeconds = date.getHours() * 3600 + date.getMinutes() * 60 + date.getSeconds()

  // 计算百分比
  const leftPercent = (startSeconds / 86400) * 100
  const widthPercent = (duration / 86400) * 100

  return {
    left: `${leftPercent}%`,
    width: `${widthPercent}%`,
    // 颜色区分：正式(蓝色)，摸鱼(绿色)
    backgroundColor: log.type === 'formal' ? '#409EFF' : '#67C23A'
  }
}

// 辅助文字格式化
const formatTimeRange = (log) => {
  const s = new Date(log.startTime )
  const e = new Date(log.endTime)
  const pad = (n) => n.toString().padStart(2, '0');
  const format = (d) => `${d.getHours()}:${pad(d.getMinutes())}`;
  if (e) {
    return `${format(s)} - ${format(e)}`;
  } else {
    return `${format(s)} - 进行中...`;
  }
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

  <el-drawer v-model="drawerVisible" :title="selectedDate + ' 时间分布'" size="40%">

    <div class="visual-container" v-if="currentLogs.length">
      <div class="time-ruler">
        <span>00:00</span><span>06:00</span><span>12:00</span><span>18:00</span><span>24:00</span>
      </div>

      <div class="timeline-track">
        <el-tooltip
            v-for="log in currentLogs"
            :key="log.id"
            :content="`${log.type === 'formal'?'💼':'🐟'} ${formatTimeRange(log)}`"
            placement="top"
        >
          <div class="time-block" :style="calculateBarStyle(log)"></div>
        </el-tooltip>
      </div>
    </div>

    <div class="list-container">
      <h3>📋 详细记录</h3>
      <el-timeline>
        <el-timeline-item
            v-for="log in currentLogs" :key="log.id"
            :type="log.type === 'formal' ? 'primary' : 'success'"
            :timestamp="formatTimeRange(log)"
        >
          <div class="log-item">
            <div class="log-header">
              <span class="log-title">
                {{ log.type === 'formal' ? '💼 正式工作' : '🐟 摸鱼学习' }}
                ({{ (log.duration / 60).toFixed(0) }} 分钟)
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
              <div v-else class="note-placeholder">
                点击添加笔记...
              </div>
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
    width="500px"
  >
    <el-input
      v-model="tempNote"
      type="textarea"
      :rows="10"
      placeholder="请输入笔记内容..."
      maxlength="500"
      show-word-limit
    />

    <template #footer>
      <el-button @click="dialogVisible = false">取消</el-button>
      <el-button type="primary" @click="saveNote">确定</el-button>
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
  box-shadow: 0 0 5px rgba(0,0,0,0.2);
}

/* 复用之前的日历样式 */
.date-cell { height: 100%; display: flex; flex-direction: column; align-items: center; cursor: pointer; }
.date-cell.has-work { background-color: #f0f9eb; }
.work-tag { background-color: #67c23a; color: white; border-radius: 4px; font-size: 12px; padding: 2px 6px; }

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
  white-space: pre-wrap;  /* 保留换行和空格 */
}

.note-placeholder {
  color: #c0c4cc;
  font-size: 13px;
  font-style: italic;
}
</style>