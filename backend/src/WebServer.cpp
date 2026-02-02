//
// Created by ttttwwww on 2026/2/2.
//

#include "WebServer.h"
WebServer::WebServer(Database& database) : db(database)
{
    setupRoutes();
}
void WebServer::setupRoutes()
{
    // 1. /api/start
    CROW_ROUTE(app, "/api/start").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Bad JSON");

        std::string type = x["type"].s();
        long long now = std::time(nullptr);

        // 访问成员变量 db
        this->db.startSession(now, type);

        return crow::response(200, "Started");
    });

    // 2. /api/stop
    CROW_ROUTE(app, "/api/stop").methods(crow::HTTPMethod::POST)
    ([this]() {
        long long now = std::time(nullptr);
        this->db.stopSession(now);
        return crow::response(200, "Stopped");
    });

    // 3. /api/history
    CROW_ROUTE(app, "/api/history")
    ([this]() {
        auto sessions = this->db.getAllSessions();

        std::vector<crow::json::wvalue> jsonList;
        for (const auto& s : sessions) {
            crow::json::wvalue j;
            j["id"] = s.id;
            j["start_time"] = s.start_time;
            j["end_time"] = s.end_time;
            j["type"] = s.type;
            jsonList.push_back(j);
        }
        return crow::json::wvalue(jsonList);
    });
    //4 .assets
    CROW_ROUTE(app, "/assets/<path>")
    ([](crow::response& res, std::string path){
        std::string file_path = "dist/assets/" + path;
        if (std::filesystem::exists(file_path)) {
            res.set_static_file_info(file_path);
        } else {
            res.code = 404;
        }
        res.end();
    });
    CROW_ROUTE(app, "/")
([](crow::response& res){
    // 强制指定返回这个 HTML 文件
    std::string target = "dist/index.html";

    // 简单的容错检查
    if (std::filesystem::exists(target)) {
        res.set_static_file_info(target);
    } else {
        res.code = 404;
        res.write("Error: dist/index.html not found. Did you run 'make'?");
    }
    res.end();
});
}

void WebServer::run(int port)
{
    std::cout << "Server starting on port " << port<<std::endl;
    app.port(port).multithreaded().run();
}