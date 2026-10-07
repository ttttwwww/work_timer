<script setup>
import { formatDuration } from '../utils/time'

// 1. 定义 props: 接收父组件传来的数据
defineProps({
  disabled: Boolean,
  isWorking: Boolean,
  workType: String,
  currentSessionTime: Number, // 当前会话的工作时长（秒）
})

// 2. 定义 emits: 声明我会发出哪些信号
const emit = defineEmits(['start', 'stop'])
</script>

<template>
  <el-card class="control-card">
    <div class="timer-display">
      <h2 v-if="!isWorking">👋 准备开始什么工作？</h2>
      <h2 v-else>
        {{ workType === 'formal' ? '💼 正式工作' : '🐟 摸鱼时间' }} 进行中...
      </h2>
      <div class="time-counter" v-if="isWorking">
        {{ formatDuration(currentSessionTime) }}
      </div>
    </div>

    <div class="button-group">
      <template v-if="!isWorking">
        <el-button
          :disabled="disabled"
          type="primary"
          size="large"
          class="action-btn"
          @click="emit('start', 'formal')"
        >
          <div class="btn-content">
            <span class="emoji">💼</span><span>正式工作</span>
          </div>
        </el-button>
        <el-button
          :disabled="disabled"
          type="success"
          size="large"
          class="action-btn"
          @click="emit('start', 'informal')"
        >
          <div class="btn-content">
            <span class="emoji">🐟</span><span>摸鱼/学习</span>
          </div>
        </el-button>
      </template>
      <template v-else>
        <el-button
          :disabled="disabled"
          type="danger"
          size="large"
          circle
          class="stop-btn"
          @click="emit('stop')"
        >
          <div class="btn-content">
            <span class="stop-icon">⏹</span><span>结束</span>
          </div>
        </el-button>
      </template>
    </div>
  </el-card>
</template>

<style scoped>
/* 把原 App.vue 里相关的 CSS 剪切过来 */
.control-card {
  text-align: center;
  margin-bottom: 20px;
  padding: 30px 20px;
}
.time-counter {
  font-size: 56px;
  font-weight: bold;
  color: #409eff;
  font-family: monospace;
  margin: 20px 0;
}
.button-group {
  display: flex;
  justify-content: center;
  gap: 16px;
  flex-wrap: wrap;
  margin-top: 30px;
}
.action-btn {
  width: 140px;
  height: 140px;
  border-radius: 16px;
}
.stop-btn {
  width: 120px;
  height: 120px;
  animation: pulse 2s infinite;
}
.btn-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  font-size: 18px;
}
.emoji {
  font-size: 40px;
}
.stop-icon {
  font-size: 36px;
}
@keyframes pulse {
  0% {
    box-shadow: 0 0 0 0 rgba(245, 108, 108, 0.7);
  }
  70% {
    box-shadow: 0 0 0 15px rgba(245, 108, 108, 0);
  }
  100% {
    box-shadow: 0 0 0 0 rgba(245, 108, 108, 0);
  }
}
</style>
