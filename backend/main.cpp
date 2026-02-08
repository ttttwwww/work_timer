#include "db.hpp"
#include "WebServer.h"
#include "config.h"

int main() {
    // 加载配置文件
    config config("config.json");

    // 初始化数据库
    Database db(config.getDatabasePath());

    // 初始化 Web 服务器
    WebServer server(db);

    // 启动
    server.run(config.getPort());

    return 0;
}