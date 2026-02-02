import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
    base:'./',
    plugins: [vue()],
    resolve: {
        alias: {
            '@': fileURLToPath(new URL('./src', import.meta.url))
        }
    },
    // 👇 新增这部分 server 配置
    server: {
        proxy: {
            '/api': {
                target: 'http://localhost:8080', // 你的 C++ 后端地址
                changeOrigin: true,
                // rewrite: (path) => path.replace(/^\/api/, '') // 如果你后端路由也是 /api 开头，就不需要这行
            }
        }
    }
})