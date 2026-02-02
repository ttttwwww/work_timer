import { createApp } from 'vue'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import App from './app.vue'

const app = createApp(App)

app.use(ElementPlus) // 挂载 Element Plus
app.mount('#app')