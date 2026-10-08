<script setup>
import { ref, watch, onMounted, onUnmounted, nextTick } from 'vue'

const props = defineProps({
  text: { type: String, default: '' },
  label: { type: String, default: '正文' },
  lines: { type: Number, default: 5 },
})
const content = ref(null)
const expanded = ref(false)
const overflowing = ref(false)
let observer

// Input: rendered paragraph. Output: whether its full text exceeds the preview.
// Measure actual wrapping so narrow screens and multiline paths work alike.
function measure() {
  const element = content.value
  if (!element) return
  const lineHeight = parseFloat(getComputedStyle(element).lineHeight)
  overflowing.value = element.scrollHeight > lineHeight * props.lines + 1
}
watch(
  () => props.text,
  async () => {
    await nextTick()
    measure()
  },
)
onMounted(() => {
  observer = new ResizeObserver(measure)
  observer.observe(content.value)
  measure()
})
onUnmounted(() => observer?.disconnect())
</script>

<template>
  <div class="expandable-text" :style="{ '--preview-lines': lines }">
    <p ref="content" :class="{ collapsed: !expanded }">{{ text }}</p>
    <el-button
      v-if="overflowing"
      text
      type="primary"
      size="small"
      :aria-label="`${expanded ? '收起' : '展开'}${label}`"
      :aria-expanded="expanded"
      @click="expanded = !expanded"
      >{{ expanded ? '收起' : '展开全文' }}</el-button
    >
  </div>
</template>

<style scoped>
.expandable-text {
  min-width: 0;
}
p {
  margin: 0;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  font-size: 14px;
  line-height: 1.75;
  color: #303133;
}
p.collapsed {
  max-height: calc(var(--preview-lines) * 1.75em);
  overflow: hidden;
}
.el-button {
  margin-top: 6px;
  padding-left: 0;
}
</style>
