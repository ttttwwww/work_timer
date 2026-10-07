# =============================================================================
# WorkTimer 全链路构建系统
# =============================================================================

# [配置区]
# -----------------------------------------------------------------------------

# 1. Vue前端
VUE_DIR     = ./frontend

# 2. 哪里是 C++ 后端？(找有 CMakeLists.txt 和 include 的那个文件夹)
CPP_DIR     = ./backend

# -----------------------------------------------------------------------------
# 内部变量 (一般不用改)
# -----------------------------------------------------------------------------
# CMake 构建类型（默认 Debug）
BUILD_TYPE ?= Debug

# C++ 构建目录（根据构建类型分离）
CPP_BUILD_DIR = $(CPP_DIR)/build-$(BUILD_TYPE)

# C++ 产出文件名
EXE_NAME      = WorkTimer

# Vue 产出目录 (Vite 默认输出到 dist)
VUE_DIST_DIR  = $(VUE_DIR)/dist

# 最终发布目录（根据构建类型分离）
RELEASE_DIR   = ./build-$(BUILD_TYPE)

# 数据目录（存放数据库等持久化文件）
DATA_DIR      = ./data

# 获取 CPU 核心数 (用于 make -j 加速)
NPROCS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

# 增量编译标记文件
VUE_BUILD_MARK = $(VUE_DIR)/.build_done
CPP_BUILD_MARK = $(CPP_BUILD_DIR)/.build_done


.PHONY: all clean install vue_build_force cpp_build_force distclean debug release

# 默认目标：智能增量构建
all: vue_build cpp_build install
	@echo "--------------------------------------------------"
	@echo "全部构建完成 ($(BUILD_TYPE) 模式)！"
	@echo "运行方式: cd $(RELEASE_DIR) && ./$(EXE_NAME)"
	@echo "--------------------------------------------------"

# 1. 构建 Vue 前端（增量）
vue_build: $(VUE_BUILD_MARK)

# Windows 下载标记带冒号，会被 GNU Make 当作静态模式规则语法。
$(VUE_BUILD_MARK): $(shell find $(VUE_DIR)/src -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(VUE_DIR)/package.json
	@echo "[1/3] 检测到前端代码变化，正在构建..."
	@# 检测是否存在 node_modules，没有才 install，加快速度
	@if [ ! -d "$(VUE_DIR)/node_modules" ]; then \
		echo "检测到首次运行，正在安装 npm 依赖..."; \
		cd $(VUE_DIR) && npm install; \
	fi
	@cd $(VUE_DIR) && npm run build
	@touch $(VUE_BUILD_MARK)
	@echo "前端构建完毕"

# 强制构建前端（不检查变化）
vue_build_force:
	@echo "[强制] 正在构建前端 (Vue)..."
	@if [ ! -d "$(VUE_DIR)/node_modules" ]; then \
		cd $(VUE_DIR) && npm install; \
	fi
	@cd $(VUE_DIR) && npm run build
	@touch $(VUE_BUILD_MARK)
	@echo "前端构建完毕"

# 2. 构建 C++ 后端（增量）
cpp_build: $(CPP_BUILD_MARK)

$(CPP_BUILD_MARK): $(shell find $(CPP_DIR)/src -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(shell find $(CPP_DIR)/include -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(CPP_DIR)/CMakeLists.txt
	@echo "[2/3] 检测到后端代码变化，正在编译 ($(BUILD_TYPE) 模式)..."
	@mkdir -p $(CPP_BUILD_DIR)
	@cd $(CPP_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && make -j$(NPROCS)
	@touch $(CPP_BUILD_MARK)
	@echo "后端编译完毕"

# 强制构建后端（不检查变化）
cpp_build_force:
	@echo "[强制] 正在构建后端 (C++, $(BUILD_TYPE) 模式)..."
	@mkdir -p $(CPP_BUILD_DIR)
	@cd $(CPP_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && make -j$(NPROCS)
	@touch $(CPP_BUILD_MARK)
	@echo "后端编译完毕"

# 3. 组装发布包
install:
	@echo "[3/3] 正在组装 Release 包..."
	@mkdir -p $(RELEASE_DIR)
	@mkdir -p $(DATA_DIR)

	# 3.1 拷贝 C++ 可执行文件
	@cp $(CPP_BUILD_DIR)/$(EXE_NAME) $(RELEASE_DIR)/

	# 3.2 拷贝前端静态资源 (先删旧的)
	@rm -rf $(RELEASE_DIR)/dist
	@cp -r $(VUE_DIST_DIR) $(RELEASE_DIR)/

	# 3.3 拷贝配置文件
	@cp $(CPP_DIR)/config.json $(RELEASE_DIR)/

	@echo "组装完毕 -> $(RELEASE_DIR)"
	@echo "数据文件将保存在 $(DATA_DIR)/ 目录（使用相对路径 ../data/）"

# 清理垃圾
clean:
	@echo "正在清理构建缓存..."
	@rm -rf $(CPP_DIR)/build-Debug
	@rm -rf $(CPP_DIR)/build-Release
	@rm -rf $(VUE_DIST_DIR)
	@rm -rf ./release-Debug
	@rm -rf ./release-Release
	@rm -f $(VUE_BUILD_MARK)
	@echo "清理完毕 (注：保留了 node_modules 和 $(DATA_DIR)/ 以保护用户数据)"

# 深度清理 (连 node_modules 一起删)
distclean: clean
	@echo "正在进行深度清理 (删除 node_modules)..."
	@rm -rf $(VUE_DIR)/node_modules

# 调试模式构建（开启调试输出）
debug:
	@echo "使用 Debug 模式构建..."
	@$(MAKE) BUILD_TYPE=Debug all

# 生产模式构建（关闭调试输出）
release:
	@echo "使用 Release 模式构建..."
	@$(MAKE) BUILD_TYPE=Release all
