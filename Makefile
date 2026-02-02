# =============================================================================
# WorkTimer 全链路构建系统
# =============================================================================

# 🛠️ [配置区] 请根据你的实际情况核对这两个路径！
# -----------------------------------------------------------------------------

# 1. 哪里是 Vue 前端？(找有 package.json 的那个文件夹)
# 默认通常是 frontend，但如果你的 package.json 真在 backend，请改成 ./backend
VUE_DIR     = ./frontend

# 2. 哪里是 C++ 后端？(找有 CMakeLists.txt 和 include 的那个文件夹)
CPP_DIR     = ./backend

# -----------------------------------------------------------------------------
# 内部变量 (一般不用改)
# -----------------------------------------------------------------------------
# C++ 构建目录
CPP_BUILD_DIR = $(CPP_DIR)/build
# C++ 产出文件名 (根据 CMakeLists.txt 里的 project 名决定，这里假设叫 WorkTimer)
EXE_NAME      = WorkTimer

# Vue 产出目录 (Vite 默认输出到 dist)
VUE_DIST_DIR  = $(VUE_DIR)/dist

# 最终发布目录
RELEASE_DIR   = ./release

# 获取 CPU 核心数 (用于 make -j 加速)
NPROCS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

.PHONY: all clean install vue_build cpp_build

# ✅ 默认目标：一键完成所有
all: vue_build cpp_build install
	@echo "--------------------------------------------------"
	@echo "🎉 全部构建完成！"
	@echo "👉 运行方式: cd $(RELEASE_DIR) && ./$(EXE_NAME)"
	@echo "--------------------------------------------------"

# 📦 1. 构建 Vue 前端
vue_build:
	@echo "🚀 [1/3] 正在构建前端 (Vue)..."
	@# 检测是否存在 node_modules，没有才 install，加快速度
	@if [ ! -d "$(VUE_DIR)/node_modules" ]; then \
		echo "📦 检测到首次运行，正在安装 npm 依赖..."; \
		cd $(VUE_DIR) && npm install; \
	fi
	@cd $(VUE_DIR) && npm run build
	@echo "✅ 前端构建完毕"

# ⚙️ 2. 构建 C++ 后端
cpp_build:
	@echo "🚀 [2/3] 正在构建后端 (C++)..."
	@mkdir -p $(CPP_BUILD_DIR)
	@# 进入构建目录，调用 CMake
	@cd $(CPP_BUILD_DIR) && cmake .. && make -j$(NPROCS)
	@echo "✅ 后端编译完毕"

# 🚚 3. 组装发布包
install:
	@echo "🚀 [3/3] 正在组装 Release 包..."
	@mkdir -p $(RELEASE_DIR)

	# 3.1 拷贝 C++ 可执行文件
	@cp $(CPP_BUILD_DIR)/$(EXE_NAME) $(RELEASE_DIR)/

	# 3.2 拷贝前端静态资源 (先删旧的)
	@rm -rf $(RELEASE_DIR)/dist
	@cp -r $(VUE_DIST_DIR) $(RELEASE_DIR)/

	@# 3.3 (可选) 如果你有数据库文件，可以在这里拷贝
	@# cp -n $(CPP_DIR)/worktimer.db $(RELEASE_DIR)/ || true

	@echo "✅ 组装完毕 -> $(RELEASE_DIR)"

# 🧹 清理垃圾
clean:
	@echo "🧹 正在清理构建缓存..."
	@rm -rf $(CPP_BUILD_DIR)
	@rm -rf $(VUE_DIST_DIR)
	@rm -rf $(RELEASE_DIR)
	@echo "✅ 清理完毕 (注：保留了 node_modules 以便下次快速编译)"

# 💥 深度清理 (连 node_modules 一起删)
distclean: clean
	@rm -rf $(VUE_DIR)/node_modules