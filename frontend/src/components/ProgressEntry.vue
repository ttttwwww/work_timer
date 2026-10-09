<script setup>
import { computed } from 'vue'
import ExpandableText from './ExpandableText.vue'
import PlainTextInput from './PlainTextInput.vue'

const props = defineProps({
  entry: { type: Object, required: true },
  draft: { type: String, default: undefined },
  busy: Boolean,
  mutate: { type: Function, required: true },
})
const emit = defineEmits(['update:draft', 'delete'])
const editing = computed(() => props.draft !== undefined)
// Input: either editor's text. Output: parent-owned draft, retained across nodes.
// Undefined means viewing; an empty string is a valid unsaved editing state.
const text = computed({
  get: () => props.draft ?? '',
  set: (value) => emit('update:draft', value),
})

// Input: saved entry. Output: editable copy without changing the saved text.
function startEditing() {
  emit('update:draft', props.entry.content)
}

// Input: current draft. Output: discard local edits and show saved content.
function cancelEditing() {
  emit('update:draft', undefined)
}

// Input: nonblank edited body. Output: update this entry, retaining draft on failure.
async function save() {
  if (props.busy || !text.value.trim()) return
  if (
    await props.mutate(`/api/progress/${props.entry.id}`, 'PUT', {
      content: text.value,
    })
  ) {
    emit('update:draft', undefined)
  }
}
</script>

<template>
  <div class="entry" :aria-label="`进度记录 ${entry.id}`">
    <form v-if="editing" @submit.prevent="save">
      <PlainTextInput v-model="text" label="进度记录" :disabled="busy" />
      <div class="entry-actions">
        <el-button size="small" :disabled="busy" @click="cancelEditing"
          >取消</el-button
        >
        <el-button
          type="primary"
          size="small"
          native-type="submit"
          :disabled="busy || !text.trim()"
          >保存修改</el-button
        >
      </div>
    </form>
    <template v-else>
      <ExpandableText :text="entry.content" label="进度记录" />
      <div class="entry-actions">
        <el-button
          text
          type="primary"
          size="small"
          :disabled="busy"
          @click="startEditing"
          >编辑记录</el-button
        >
        <el-button
          text
          type="danger"
          size="small"
          :disabled="busy"
          @click="emit('delete', entry.id)"
          >删除记录</el-button
        >
      </div>
    </template>
  </div>
</template>

<style scoped>
.entry {
  padding: 10px 12px;
  border-radius: 6px;
  background: #f5f7fa;
}
.entry-actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 8px;
}
.entry-actions .el-button + .el-button {
  margin-left: 0;
}
</style>
