<script setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import ExpandableText from './ExpandableText.vue'
import { copyText } from '../utils/clipboard'

const props = defineProps({ location: { type: String, default: '' } })
const copying = ref(false)
const manualCopy = ref(false)
const manualInput = ref(null)

// Input: saved location text. Output: clipboard content or a selectable fallback.
// Paths, URLs and prose are all text; no navigation or file access is attempted.
async function copyLocation() {
  if (copying.value) return
  copying.value = true
  try {
    if (await copyText(props.location)) ElMessage.success('笔记位置已复制')
    else manualCopy.value = true
  } finally {
    copying.value = false
  }
}
// Input: opened fallback dialog. Output: focused and selected location text.
function selectLocation() {
  manualInput.value?.focus()
  manualInput.value?.select()
}
</script>

<template>
  <section v-if="location" class="note-location" aria-label="笔记位置">
    <div class="location-heading">
      <span>笔记位置</span>
      <el-button
        text
        type="primary"
        size="small"
        :loading="copying"
        @click="copyLocation"
        >复制位置</el-button
      >
    </div>
    <ExpandableText :text="location" label="笔记位置" :lines="3" />
    <el-dialog
      v-model="manualCopy"
      title="复制笔记位置"
      width="min(640px,94vw)"
      append-to-body
      @opened="selectLocation"
    >
      <p>浏览器未允许自动复制，请复制下方文字。</p>
      <el-input
        ref="manualInput"
        :model-value="location"
        readonly
        type="textarea"
        :autosize="{ minRows: 3, maxRows: 10 }"
        aria-label="手动复制笔记位置"
      />
      <template #footer
        ><el-button @click="manualCopy = false">关闭</el-button></template
      >
    </el-dialog>
  </section>
</template>

<style scoped>
.note-location {
  min-width: 0;
  margin: 14px 0;
  padding: 10px 14px;
  background: #f5f7fa;
  border-radius: 6px;
}
.location-heading {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  margin-bottom: 4px;
  color: #606266;
  font-size: 13px;
}
</style>
