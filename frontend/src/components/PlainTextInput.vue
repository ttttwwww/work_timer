<script setup>
import { computed, ref } from 'vue'

const props = defineProps({
  modelValue: { type: String, default: '' },
  label: { type: String, default: '正文' },
  placeholder: String,
  disabled: Boolean,
  maxlength: { type: Number, default: 5000 },
  minRows: { type: Number, default: 5 },
})
const emit = defineEmits(['update:modelValue'])
const enlarged = ref(false)
// Input: either editor's text. Output: the same parent-owned draft.
// Enlarging changes only the workspace; closing it never submits or discards.
const draft = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value),
})
</script>

<template>
  <div class="plain-text-input">
    <el-input
      v-model="draft"
      type="textarea"
      :autosize="{ minRows, maxRows: 14 }"
      :maxlength="maxlength"
      show-word-limit
      :disabled="disabled"
      :aria-label="label"
      :placeholder="placeholder"
    />
    <div class="input-tools">
      <el-button
        text
        type="primary"
        size="small"
        :disabled="disabled"
        @click="enlarged = true"
        :aria-label="`放大编辑${label}`"
        >放大编辑</el-button
      >
    </div>
    <el-dialog
      v-model="enlarged"
      :title="`编辑${label}`"
      width="min(960px,96vw)"
      top="4vh"
      append-to-body
      :close-on-click-modal="false"
    >
      <p class="draft-hint">这是当前输入框的草稿，完成后返回原界面保存。</p>
      <el-input
        v-model="draft"
        type="textarea"
        :autosize="{ minRows: 14, maxRows: 24 }"
        :maxlength="maxlength"
        show-word-limit
        :disabled="disabled"
        :aria-label="`放大${label}`"
        :placeholder="placeholder"
        class="large-input"
      />
      <template #footer
        ><el-button type="primary" @click="enlarged = false"
          >完成编辑</el-button
        ></template
      >
    </el-dialog>
  </div>
</template>

<style scoped>
.plain-text-input {
  width: 100%;
  min-width: 0;
}
:deep(.el-textarea__inner) {
  font-size: 15px;
  line-height: 1.75;
  padding: 12px 14px 28px;
}
.input-tools {
  display: flex;
  justify-content: flex-end;
  margin-top: 4px;
}
.draft-hint {
  margin: 0 0 12px;
  color: #606266;
  font-size: 13px;
}
.large-input :deep(.el-textarea__inner) {
  min-height: 40vh !important;
  max-height: 60vh;
}
</style>
