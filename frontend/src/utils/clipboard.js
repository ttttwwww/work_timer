// Input: plain text. Output: true if copied, false when manual selection is needed.
// The fallback supports private HTTP deployments where Clipboard API is absent.
export async function copyText(text) {
  try {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(text)
      return true
    }
  } catch {
    /* Browser policy may reject clipboard access; try selection. */
  }
  const previous = document.activeElement
  const area = document.createElement('textarea')
  area.value = text
  area.readOnly = true
  area.style.cssText =
    'position:fixed;left:0;top:0;opacity:0;pointer-events:none;'
  document.body.appendChild(area)
  try {
    area.focus()
    area.select()
    return document.execCommand('copy')
  } catch {
    return false
  } finally {
    area.remove()
    previous?.focus?.({ preventScroll: true })
  }
}
