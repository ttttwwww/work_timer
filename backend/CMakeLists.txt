cmake_minimum_required(VERSION 3.20)
project(WorkTimer)

set(CMAKE_CXX_STANDARD 17)

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
if(TARGET SQLite3::SQLite3)
    target_link_libraries(WorkTimer SQLite3::SQLite3)
else ()
    target_include_directories(WorkTimer PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(WorkTimer PRIVATE ${SQLite3_LIBRARIES})
endif ()
# Windows 下可能还需要链接 ws2_32 (Winsock)
if(WIN32)
    target_link_libraries(WorkTimer ws2_32)
endif()