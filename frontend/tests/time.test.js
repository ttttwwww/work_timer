import test from 'node:test'
import assert from 'node:assert/strict'
import {
  createServerClock,
  formatDuration,
  aggregateDays,
} from '../src/utils/time.js'

test('clock uses server epoch and monotonic ticks, not device wall clock', () => {
  let mono = 50
  const clock = createServerClock(() => mono)
  clock.sync(1000, 100)
  assert.equal(clock.now(), 1000050)
  const original = Date.now
  try {
    Date.now = () => -999999999
    mono += 2300
    assert.equal(clock.now(), 1002350)
    Date.now = () => 99999999999999
    mono += 1000
    assert.equal(clock.now(), 1003350)
  } finally {
    Date.now = original
  }
})
test('duration cannot display negative or invalid time and hours can exceed 24', () => {
  assert.equal(formatDuration(-1), '00:00:00')
  assert.equal(formatDuration(NaN), '00:00:00')
  assert.equal(formatDuration(3661.9), '01:01:01')
  assert.equal(formatDuration(90061), '25:01:01')
})
test('running record duration and displayed end advance with reactive now', () => {
  const start = new Date(2026, 8, 18, 10, 0, 0)
  const logs = [{ id: 1, startTime: start, endTime: null }]
  const initial = aggregateDays(logs, +start + 1000)['2026-09-18'].logs[0]
  const later = aggregateDays(logs, +start + 62000)['2026-09-18'].logs[0]
  assert.equal(initial.duration, 1)
  assert.equal(later.duration, 62)
  assert.equal(+later.displayEnd, +start + 62000)
  assert.equal(later.ongoing, true)
})
test('midnight sessions split across dates without double counting', () => {
  const start = new Date(2026, 8, 18, 23, 30)
  const end = new Date(2026, 8, 19, 0, 30)
  const days = aggregateDays([{ id: 1, startTime: start, endTime: end }], +end)
  assert.equal(days['2026-09-18'].totalHours, '0.5')
  assert.equal(days['2026-09-19'].totalHours, '0.5')
  assert.equal(days['2026-09-18'].logs[0].ongoing, false)
  const running = aggregateDays(
    [{ id: 1, startTime: start, endTime: null }],
    +end,
  )
  assert.equal(running['2026-09-18'].logs[0].ongoing, false)
  assert.equal(running['2026-09-19'].logs[0].ongoing, true)
})
test('zero length and future-start legacy records have zero duration', () => {
  const start = new Date(2026, 8, 18, 10)
  assert.equal(
    aggregateDays([{ startTime: start, endTime: null }], +start - 1000)[
      '2026-09-18'
    ].logs[0].duration,
    0,
  )
})
