// Use the server epoch for dates and monotonic elapsed time between syncs.
// Changing the device wall clock cannot make a running timer jump.
export function createServerClock(monotonicNow = () => performance.now()) {
  let epoch = 0
  let anchor = 0
  return {
    sync(serverSeconds, roundTripMs = 0) {
      epoch = serverSeconds * 1000 + Math.max(0, roundTripMs) / 2
      anchor = monotonicNow()
    },
    now() {
      return epoch + Math.max(0, monotonicNow() - anchor)
    },
  }
}
export function formatDuration(seconds) {
  const value = Number.isFinite(seconds) ? Math.max(0, Math.floor(seconds)) : 0
  return [Math.floor(value / 3600), Math.floor(value / 60) % 60, value % 60]
    .map((part) => String(part).padStart(2, '0'))
    .join(':')
}
export function localDateKey(date) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`
}
export function toLocalInput(ms) {
  const d = new Date(ms)
  return `${localDateKey(d)}T${[d.getHours(), d.getMinutes(), d.getSeconds()].map((n) => String(n).padStart(2, '0')).join(':')}`
}
// Clip each session to local calendar days, including DST and midnight boundaries.
export function aggregateDays(logs, now) {
  const days = {}
  for (const log of logs) {
    const start = log.startTime.getTime()
    const end = Math.max(start, log.endTime?.getTime() ?? now)
    let cursor = start
    do {
      const day = new Date(cursor)
      const next = new Date(
        day.getFullYear(),
        day.getMonth(),
        day.getDate() + 1,
      ).getTime()
      const segmentEnd = Math.min(end, next)
      const key = localDateKey(day)
      const group = (days[key] ??= { logs: [], totalHours: 0 })
      const duration = Math.max(0, (segmentEnd - cursor) / 1000)
      group.logs.push({
        ...log,
        displayStart: new Date(cursor),
        displayEnd: new Date(segmentEnd),
        ongoing: !log.endTime && segmentEnd === end,
        duration,
      })
      group.totalHours += duration / 3600
      cursor = next
    } while (cursor < end)
  }
  for (const group of Object.values(days))
    group.totalHours = group.totalHours.toFixed(1)
  return days
}
