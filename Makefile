# =============================================================================
# WorkTimer 全链路构建系统
# =============================================================================

# [配置区]
# -----------------------------------------------------------------------------

# 1. Vue前端
VUE_DIR     = ./frontend

# npm 自带进度显示取决于版本和终端；同时显示下载日志，避免只有旋转指示。
# 可通过 make release NPM_INSTALL_FLAGS="--loglevel=verbose --foreground-scripts" 调整。
NPM_INSTALL_FLAGS ?= --progress=true --loglevel=http --foreground-scripts

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
VUE_DEPS_MARK = $(VUE_DIR)/node_modules/.worktimer-deps-ready
CPP_BUILD_MARK = $(CPP_BUILD_DIR)/.build_done


.PHONY: all clean install vue_build vue_build_force cpp_build cpp_build_force distclean debug release check-deps

# 输入：命令名、用途与修复提示。输出：命令可运行时继续，否则明确报错并退出。
# 保留 --version 的原始错误，便于定位已安装但因 GLIBC 等原因无法运行的情况。
define require_tool
@if ! command -v $(1) >/dev/null 2>&1; then \
	printf '\n[构建失败] 找不到 $(1)，无法$(2)。\n$(3)\n验证命令：$(1) --version\n\n' >&2; \
	exit 1; \
fi; \
if ! $(1) --version; then \
	printf '\n[构建失败] $(1) 已找到，但无法正常运行。\n请检查上方错误及工具与系统的兼容性。\n\n' >&2; \
	exit 1; \
fi
endef

# 输入：当前 PATH。输出：前端工具可用；仅在实际构建前端时调用。
define check_frontend_tools
$(call require_tool,node,构建前端,请安装 Node.js（包含 npm），并将安装目录加入 PATH。)
$(call require_tool,npm,安装和构建前端依赖,请安装 npm（通常随 Node.js 提供），并将安装目录加入 PATH。)
@node -e 'const major = Number(process.versions.node.split(".")[0]); if (major !== 18 && major < 20) { console.error("[构建失败] 当前 Node.js " + process.version + " 不满足 Vite 5 要求：需要 18.x 或 20+。请切换 Node.js 版本后重试。"); process.exit(1); }'
endef

# 输入：PATH 和可选的 CXX。输出：后端工具可用；具体编译能力由 CMake 检测。
define check_backend_tools
$(call require_tool,cmake,配置 C++ 后端,请安装 CMake 3.20 或更高版本，并将安装目录加入 PATH。)
@if [ -n "$$CXX" ]; then \
	if ! $$CXX --version; then \
		printf '\n[构建失败] CXX 指定的 C++ 编译器无法运行：%s\n请检查 CXX 和 PATH。\n\n' "$$CXX" >&2; \
		exit 1; \
	fi; \
elif ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then \
	printf '\n[构建失败] 找不到 C++ 编译器，无法编译后端。\n请安装支持 C++17 的 GCC 或 Clang；自定义编译器可通过环境变量 CXX 指定。\n验证命令：g++ --version 或 clang++ --version\n\n' >&2; \
	exit 1; \
fi
endef

# 输入：依赖清单、安装成功标记和本地 Vite。输出：按锁文件准备含开发工具的依赖。
# 安装失败或中断时不写标记，下次构建会重试，不能仅凭 node_modules 目录判断成功。
define ensure_frontend_dependencies
@if [ ! -f "$(VUE_DEPS_MARK)" ] || [ ! -x "$(VUE_DIR)/node_modules/.bin/vite" ] || \
	[ "$(VUE_DIR)/package.json" -nt "$(VUE_DEPS_MARK)" ] || \
	[ "$(VUE_DIR)/package-lock.json" -nt "$(VUE_DEPS_MARK)" ]; then \
	echo "前端依赖未就绪或清单已更新，正在按锁文件安装（含 Vite，显示下载日志）..."; \
	rm -f "$(VUE_DEPS_MARK)"; \
	if ! (cd "$(VUE_DIR)" && npm ci --include=dev $(NPM_INSTALL_FLAGS)); then \
		printf '\n[构建失败] npm 依赖安装未完成，已停止前端构建。\n请根据上方 npm 错误检查网络、代理或安装配置，再重试。\n\n' >&2; \
		exit 1; \
	fi; \
	if [ ! -x "$(VUE_DIR)/node_modules/.bin/vite" ]; then \
		printf '\n[构建失败] 安装后仍找不到本地 Vite，请检查 npm 的 dev 依赖和 bin-links 配置。\n\n' >&2; \
		exit 1; \
	fi; \
	touch "$(VUE_DEPS_MARK)"; \
fi
endef

# 默认目标：智能增量构建
all: vue_build cpp_build install
	@echo "--------------------------------------------------"
	@echo "全部构建完成 ($(BUILD_TYPE) 模式)！"
	@echo "运行方式: cd $(RELEASE_DIR) && ./$(EXE_NAME)"
	@echo "--------------------------------------------------"

# 仅检查工具，不安装依赖或执行构建。
check-deps:
	$(check_frontend_tools)
	$(check_backend_tools)
	@echo "构建工具检查通过；SQLite3、Asio 和编译器能力将在后端配置/编译时检查。"

# 1. 构建 Vue 前端（增量）
vue_build: $(VUE_BUILD_MARK)

# Windows 下载标记带冒号，会被 GNU Make 当作静态模式规则语法。
$(VUE_BUILD_MARK): $(shell find $(VUE_DIR)/src -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(VUE_DIR)/package.json $(VUE_DIR)/package-lock.json
	$(check_frontend_tools)
	@echo "[1/3] 检测到前端代码变化，正在构建..."
	$(ensure_frontend_dependencies)
	@cd $(VUE_DIR) && npm run build
	@touch $(VUE_BUILD_MARK)
	@echo "前端构建完毕"

# 强制构建前端（不检查变化）
vue_build_force:
	$(check_frontend_tools)
	@echo "[强制] 正在构建前端 (Vue)..."
	$(ensure_frontend_dependencies)
	@cd $(VUE_DIR) && npm run build
	@touch $(VUE_BUILD_MARK)
	@echo "前端构建完毕"

# 2. 构建 C++ 后端（增量）
cpp_build: $(CPP_BUILD_MARK)

$(CPP_BUILD_MARK): $(shell find $(CPP_DIR)/src -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(shell find $(CPP_DIR)/include -type f ! -name '*:Zone.Identifier' 2>/dev/null) $(CPP_DIR)/CMakeLists.txt
	$(check_backend_tools)
	@echo "[2/3] 检测到后端代码变化，正在编译 ($(BUILD_TYPE) 模式)..."
	@mkdir -p $(CPP_BUILD_DIR)
	@cd $(CPP_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && make -j$(NPROCS)
	@touch $(CPP_BUILD_MARK)
	@echo "后端编译完毕"

# 强制构建后端（不检查变化）
cpp_build_force:
	$(check_backend_tools)
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
