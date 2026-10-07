<script setup>
import { computed } from 'vue'

const props = defineProps({
  todo: Object,
  entries: Array,
  draft: { type: String, default: '' },
  expanded: Boolean,
  busy: Boolean,
  mutate: Function,
})
const emit = defineEmits([
  'update:draft',
  'update:expanded',
  'edit',
  'delete',
  'delete-progress',
])
const progressDraft = computed({
  get: () => props.draft,
  set: (value) => emit('update:draft', value),
})

// Input: checkbox state. Output: save result; keep this todo's logs intact.
const toggleDone = (done) =>
  props.mutate(`/api/todos/${props.todo.id}`, 'PUT', {
    title: props.todo.title,
    done,
  })

// Input: this todo's draft. Output: one saved log; clear only after success.
async function addProgress() {
  const content = progressDraft.value.trim()
  if (!content || props.busy) return
  if (
    await props.mutate(`/api/todos/${props.todo.id}/progress`, 'POST', {
      content,
    })
  ) {
    progressDraft.value = ''
  }
}
</script>

<template>
  <li class="todo-item">
    <div class="todo-heading">
      <el-checkbox
        :model-value="todo.done"
        :disabled="busy"
        :aria-label="todo.title"
        @change="toggleDone"
        ><span :class="{ done: todo.done }">{{ todo.title }}</span></el-checkbox
      >
      <div class="todo-actions">
        <el-button
          text
          type="primary"
          size="small"
          :aria-expanded="expanded"
          :aria-controls="`todo-progress-${todo.id}`"
          @click="emit('update:expanded', !expanded)"
          >{{ expanded ? '收起进度' : '展开进度' }}（{{
            entries.length
          }}）</el-button
        >
        <el-button
          text
          size="small"
          :disabled="busy"
          @click="emit('edit', todo)"
          >编辑</el-button
        >
        <el-button
          text
          size="small"
          type="danger"
          :disabled="busy"
          @click="emit('delete', todo.id)"
          >删除</el-button
        >
      </div>
    </div>
    <section
      v-show="expanded"
      :id="`todo-progress-${todo.id}`"
      class="todo-progress"
      :aria-label="`${todo.title}的进度`"
    >
      <form @submit.prevent="addProgress">
        <el-input
          v-model="progressDraft"
          type="textarea"
          :rows="3"
          :disabled="busy"
          maxlength="5000"
          show-word-limit
          :aria-label="`为${todo.title}记录进度`"
          placeholder="记录这项待办的尝试、结果和下一步…"
        />
        <div class="save-progress">
          <el-button
            type="primary"
            size="small"
            native-type="submit"
            :disabled="busy || !progressDraft.trim()"
            >记录进度</el-button
          >
        </div>
      </form>
      <p v-if="!entries.length" class="empty-progress">
        这项待办还没有进度记录。
      </p>
      <el-timeline v-else class="progress-list">
        <el-timeline-item
          v-for="entry in entries"
          :key="entry.id"
          :timestamp="new Date(entry.created_at * 1000).toLocaleString()"
          placement="top"
        >
          <div class="entry">
            <p>{{ entry.content }}</p>
            <el-button
              text
              size="small"
              type="danger"
              :disabled="busy"
              @click="emit('delete-progress', entry.id)"
              >删除记录</el-button
            >
          </div>
        </el-timeline-item>
      </el-timeline>
    </section>
  </li>
</template>

<style scoped>
.todo-item {
  padding: 12px;
  margin-bottom: 12px;
  border: 1px solid #e4e7ed;
  border-radius: 8px;
}
.todo-heading {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 8px;
}
.todo-actions {
  display: flex;
  align-items: center;
  flex-shrink: 0;
}
.todo-actions .el-button + .el-button {
  margin-left: 2px;
}
.el-checkbox {
  height: auto;
  min-width: 0;
  margin-right: 0;
  align-items: flex-start;
  padding: 4px 0;
}
:deep(.el-checkbox__input) {
  padding-top: 3px;
}
:deep(.el-checkbox__label) {
  white-space: normal;
  overflow-wrap: anywhere;
  line-height: 1.5;
}
.done {
  text-decoration: line-through;
  color: #909399;
}
.todo-progress {
  border-top: 1px solid #ebeef5;
  margin-top: 12px;
  padding-top: 14px;
}
.save-progress {
  text-align: right;
  margin-top: 10px;
}
.empty-progress {
  color: #909399;
  font-size: 13px;
  margin-bottom: 0;
}
.progress-list {
  padding-left: 4px;
  margin: 18px 0 0;
}
.entry {
  padding: 10px 12px;
  border-radius: 6px;
  background: #f5f7fa;
}
.entry p {
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  margin: 0;
  font-size: 14px;
  line-height: 1.7;
}
.entry .el-button {
  display: block;
  margin-left: auto;
}
@media (max-width: 600px) {
  .todo-heading {
    flex-direction: column;
  }
  .todo-actions {
    align-self: flex-end;
  }
}
</style>
