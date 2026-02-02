#include "db.hpp"
#include "WebServer.h"

int main() {
    // 1. 初始化数据库模块
    Database db("worktimer.db");

    // 2. 初始化 Web 服务器模块 (把 db 传给它)
    WebServer server(db);

    // 3. 启动！
    server.run(8080);

    return 0;
}