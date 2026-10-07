<script setup>
import { computed } from 'vue'
import TodoItem from './TodoItem.vue'
const props = defineProps({
  node: Object,
  draft: Object,
  todos: Array,
  progress: Array,
  busy: Boolean,
  mutate: Function,
})
defineEmits([
  'edit-node',
  'delete-node',
  'edit-todo',
  'delete-todo',
  'delete-progress',
])
const todoDraft = computed({
  get: () => props.draft.todo,
  set: (value) => {
    props.draft.todo = value
  },
})
// Input: node logs. Output: logs grouped by their owning todo ID.
const progressByTodo = computed(() => {
  const grouped = {}
  for (const entry of props.progress) {
    if (!grouped[entry.todo_id]) grouped[entry.todo_id] = []
    grouped[entry.todo_id].push(entry)
  }
  return grouped
})
const completed = computed(() => props.todos.filter((item) => item.done).length)
const percentage = computed(() =>
  props.todos.length
    ? Math.round((100 * completed.value) / props.todos.length)
    : 0,
)
// Input: new todo title. Output: saved todo, expanded for recording progress.
async function addTodo() {
  const title = todoDraft.value.trim()
  if (!title) return
  if (
    await props.mutate(
      `/api/nodes/${props.node.id}/todos`,
      'POST',
      { title },
      (result) => {
        props.draft.expanded[result.id] = true
      },
    )
  )
    todoDraft.value = ''
}
const labels = { open: '待解决', doing: '处理中', resolved: '已解决' }
const changeStatus = (status) =>
  props.mutate(`/api/nodes/${props.node.id}`, 'PUT', {
    title: props.node.title,
    description: props.node.description,
    status,
  })
</script>

<template>
  <section class="node-detail">
    <div class="detail-heading">
      <h3>{{ node.title }}</h3>
      <div class="actions">
        <el-select
          :model-value="node.status"
          :disabled="busy"
          aria-label="节点状态"
          class="node-status"
          @change="changeStatus"
          ><el-option
            v-for="(label, value) in labels"
            :key="value"
            :label="label"
            :value="value"
        /></el-select>
        <el-button text :disabled="busy" @click="$emit('edit-node')"
          >编辑</el-button
        ><el-button
          type="danger"
          text
          :disabled="busy"
          @click="$emit('delete-node')"
          >删除</el-button
        >
      </div>
    </div>
    <p v-if="node.description" class="description">{{ node.description }}</p>
    <div class="section-heading">
      <h4>待办清单</h4>
      <span class="muted">{{ completed }}/{{ todos.length }} 已完成</span>
    </div>
    <el-progress
      v-if="todos.length"
      :percentage="percentage"
      :stroke-width="6"
      :show-text="false"
    />
    <ul class="todos">
      <TodoItem
        v-for="item in todos"
        :key="item.id"
        :todo="item"
        :entries="progressByTodo[item.id] || []"
        v-model:draft="draft.progress[item.id]"
        v-model:expanded="draft.expanded[item.id]"
        :busy="busy"
        :mutate="mutate"
        @edit="(item) => $emit('edit-todo', item)"
        @delete="(id) => $emit('delete-todo', id)"
        @delete-progress="(id) => $emit('delete-progress', id)"
      />
    </ul>
    <p v-if="!todos.length" class="muted">写下解决这个问题需要做的下一步。</p>
    <form class="todo-form" @submit.prevent="addTodo">
      <el-input
        v-model="todoDraft"
        maxlength="200"
        :disabled="busy"
        placeholder="添加待办，按 Enter 保存"
        aria-label="新待办"
      /><el-button
        type="primary"
        plain
        native-type="submit"
        :disabled="busy || !todoDraft.trim()"
        >添加</el-button
      >
    </form>
  </section>
</template>

<style scoped>
.node-detail {
  border-top: 1px solid #e4e7ed;
  padding-top: 20px;
}
.detail-heading,
.section-heading {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}
h3 {
  margin: 0;
  font-size: 17px;
  overflow-wrap: anywhere;
}
h4 {
  margin: 0;
  font-size: 14px;
}
.actions {
  display: flex;
  align-items: center;
  flex-shrink: 0;
}
.node-status {
  width: 112px;
}
.description {
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  font-size: 14px;
  line-height: 1.7;
}
.muted {
  color: #909399;
  font-size: 13px;
}
.section-heading {
  margin: 24px 0 12px;
}
.todos {
  padding: 0;
  list-style: none;
  margin: 12px 0;
}
.todo-form {
  display: flex;
  gap: 8px;
}
@media (max-width: 560px) {
  .detail-heading {
    align-items: flex-start;
    flex-direction: column;
  }
}
</style>
