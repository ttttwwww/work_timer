export async function api(path, method = 'GET', body) {
  const response = await fetch(path, {
    method,
    headers:
      body === undefined ? undefined : { 'Content-Type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
    cache: 'no-store',
  })
  const data = await response.json()
  if (!response.ok)
    throw new Error(data.error || `请求失败 (${response.status})`)
  return data
}
