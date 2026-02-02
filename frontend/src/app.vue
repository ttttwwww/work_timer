<script setup>
import { ref, onMounted, computed, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
// 确保路径正确，如果报错请检查文件名大小写
import WorkControls from './components/WorkControls.vue'
import HistoryCalendar from './components/HistoryCalendar.vue'

// --- 状态数据 ---
const isWorking = ref(false)
const currentWorkType = ref('')
const startTime = ref(null)
const todayWorkTime = ref(0)
const timerInterval = ref(null)

// 1. 原始流水账
const rawWorkLogs = ref([])
// 2. 每日聚合数据
// --- 核心加工厂：把流水账变成按日期归档的字典 ---
const dailyStats = computed(() => {
  const map = {}

  rawWorkLogs.value.forEach(log => {
    // 1. 获取日期对象
    // log.startTime 已经在 fetchHistory 里被转成 Date 对象了
    const d = log.startTime

    // 2. 生成标准的 "2026-02-02" 格式 Key
    // ⚠️ 必须用 getFullYear/Month (本地时间)，不能用 toISOString (UTC时间)
    // 否则晚上玩的时候，记录会被记到前一天去
    const year = d.getFullYear()
    const month = (d.getMonth() + 1).toString().padStart(2, '0') // 补0
    const day = d.getDate().toString().padStart(2, '0')          // 补0
    const dateKey = `${year}-${month}-${day}`

    // 3. 初始化这一天的格子
    if (!map[dateKey]) {
      map[dateKey] = {
        logs: [],
        totalHours: 0
      }
    }

    // 4. 计算时长
    // 如果没有 endTime (正在进行中)，就暂时算到现在
    const end = log.endTime || new Date()
    const durationMs = end - log.startTime
    const durationSeconds = durationMs / 1000

    // 5. 塞入记录（带上 duration 字段）
    map[dateKey].logs.push({
      ...log,
      duration: durationSeconds
    })

    // 6. 累加时长 (毫秒 -> 小时)
    map[dateKey].totalHours += durationMs / (1000 * 60 * 60)
  })

  // 6. 最后把所有时长保留1位小数 (比如 5.2 小时)
  for (let key in map) {
    map[key].totalHours = map[key].totalHours.toFixed(1)
  }

  return map
})


// --- 核心业务逻辑 ---

// 处理"开始"信号
const handleStart = async (type) => {
  // 先检查后端状态，避免重复开始
  const checkRes = await fetch('/api/history')
  const checkData = await checkRes.json()
  const hasActiveSession = checkData.some(item => item.end_time === 0)

  if (hasActiveSession) {
    // 后端已经有正在进行的会话了（可能是其他设备开始的，或者是本设备之前没关闭的）
    ElMessage.warning('已有工作在进行中，请先结束')
    // 同步本地状态
    await fetchHistory()
    return
  }

  await fetch('/api/start', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type })
  })

  isWorking.value = true
  currentWorkType.value = type
  startTime.value = Date.now() / 1000 // 存秒级时间戳，方便计算

  // 启动计时器 (用于界面显示 HH:MM:SS)
  timerInterval.value = setInterval(() => {
    todayWorkTime.value = Math.floor((Date.now() / 1000) - startTime.value)
  }, 1000)
  await fetchHistory()
  ElMessage.success(`开始 ${type === 'formal' ? '正式工作' : '摸鱼'}！`)
}

// 处理"结束"信号
const handleStop = async () => {
  await fetch('/api/stop', {
    method: 'POST'
  })

  clearInterval(timerInterval.value)
  timerInterval.value = null
  isWorking.value = false
  currentWorkType.value = ''
  startTime.value = null
  todayWorkTime.value = 0

  // 重新从后端获取数据，保证数据一致性
  await fetchHistory()
  ElMessage.warning('记录已保存')
}

// --- 数据处理逻辑 ---
const fetchHistory = async () =>{
  try{
    const res = await fetch('/api/history')
    const data = await res.json()
    rawWorkLogs.value = data.map(item => ({
      id: item.id,
      type: item.type,
      startTime: new Date(item.start_time * 1000),
      // 如果 end_time 是 0，说明还没结束
      endTime: item.end_time === 0 ? null : new Date(item.end_time * 1000)
    }))
    // 检查是否有正在进行的会话（多端同步）
    const activeSession = rawWorkLogs.value.find(log=>log.endTime === null)
    if(activeSession){
      isWorking.value = true
      currentWorkType.value = activeSession.type
      startTime.value = activeSession.startTime.getTime() / 1000 // 转换为秒级时间戳

      // 计算已经工作的时间
      todayWorkTime.value = Math.floor((Date.now() / 1000) - startTime.value)

      // 只在没有计时器时才启动（避免重复创建）
      if(!timerInterval.value){
        timerInterval.value = setInterval(() => {
          todayWorkTime.value = Math.floor((Date.now() / 1000) - startTime.value)
        }, 1000)
      }
    } else {
      // 如果没有正在进行的会话，确保停止计时器
      if(timerInterval.value){
        clearInterval(timerInterval.value)
        timerInterval.value = null
      }
      isWorking.value = false
      currentWorkType.value = ''
      startTime.value = null
      todayWorkTime.value = 0
    }
  }catch (err){
    console.error("连接后端失败",err)
    ElMessage.error("连接后端失败")
  }

}


// --- 加载时处理 ---
onMounted(() => {
  // 页面加载时检查一次后端状态（实现多端同步）
  fetchHistory()
})

// 组件卸载时清理定时器
onUnmounted(() => {
  if (timerInterval.value) {
    clearInterval(timerInterval.value)
  }
})
</script>

<template>
  <div class="app-container">
    <WorkControls
        :is-working="isWorking"
        :work-type="currentWorkType"
        :today-work-time="todayWorkTime"
        @start="handleStart"
        @stop="handleStop"
    />

    <HistoryCalendar :daily-stats="dailyStats" />
  </div>
</template>

<style scoped>
.app-container {
  max-width: 900px;
  margin: 30px auto;
  padding: 0 20px;
}
</style>