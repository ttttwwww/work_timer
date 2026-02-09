cmake_minimum_required(VERSION 3.20)
project(WorkTimer)

set(CMAKE_CXX_STANDARD 17)

# 为 Crow 添加必要的定义
add_definitions(-D_GLIBCXX_USE_C99_MATH_TR1)

# 🔧 根据构建类型自动设置调试模式
# Debug 模式：开启调试输出
# Release 模式：关闭调试输出
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release 模式：关闭调试输出")
    add_definitions(-DDB_DEBUG_MODE=0 -DWEB_DEBUG_MODE=0)
else()
    message(STATUS "Debug 模式：开启调试输出")
    add_definitions(-DDB_DEBUG_MODE=1 -DWEB_DEBUG_MODE=1)
endif()

# 1. 查找并包含 Crow (Web框架)
# 假设你已经把 crow_all.h 下载到了 backend/include 或者配置了 find_package
include_directories(include)

# 2. 查找并链接 SQLite3
find_package(SQLite3 REQUIRED)

file(GLOB_RECURSE HEADER_FILES "include/*.h" "include/*.hpp")
# 3. 生成可执行文件
add_executable(WorkTimer main.cpp
        ${HEADER_FILES}
        src/WebServer.cpp
)

# 4. 链接库
# 查找 pthread
find_package(Threads REQUIRED)

if(TARGET SQLite3::SQLite3)
    target_link_libraries(WorkTimer SQLite3::SQLite3 Threads::Threads stdc++fs)
else ()
    target_include_directories(WorkTimer PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(WorkTimer ${SQLite3_LIBRARIES} Threads::Threads stdc++fs)
endif ()

# Windows 下可能还需要链接 ws2_32 (Winsock)
if(WIN32)
    target_link_libraries(WorkTimer ws2_32)
endif()

# 5. 复制配置文件到构建目录
add_custom_command(TARGET WorkTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/config.json
    $<TARGET_FILE_DIR:WorkTimer>/config.json
)

