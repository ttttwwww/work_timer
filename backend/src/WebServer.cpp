//
// Created by ttttwwww on 2026/2/2.
//

#include "WebServer.h"

// 🔧 调试开关：由 CMake 自动设置（不需要手动修改）
// 如果 CMake 没有定义，默认开启调试
#ifndef WEB_DEBUG_MODE
    #define WEB_DEBUG_MODE 1
#endif

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
        const long long now = std::time(nullptr);

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
        const auto sessions = this->db.getAllSessions();

#if WEB_DEBUG_MODE
        std::cout << "[DEBUG] 返回 " << sessions.size() << " 条记录\n";
#endif

        std::vector<crow::json::wvalue> jsonList;
        for (const auto& s : sessions) {
            crow::json::wvalue j;
            j["id"] = s.id;
            j["start_time"] = s.start_time;
            j["end_time"] = s.end_time;
            j["type"] = s.type;
            j["note"] = s.note;

#if WEB_DEBUG_MODE
            // 调试：打印每条记录的笔记
            if (!s.note.empty()) {
                std::cout << "  ID " << s.id << " 的笔记: [" << s.note << "]\n";
            }
#endif

            jsonList.push_back(j);
        }
        return crow::json::wvalue(jsonList);
    });
    // delete
    CROW_ROUTE(app, "/api/delete").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Bad JSON");

        const int id = x["id"].i();

        this->db.deleteSession(id);

        return crow::response(200, "Deleted");
    });

    // add note
    CROW_ROUTE(app, "/api/note").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        const auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Bad JSON");

        int id = x["id"].i();
        const std::string note = x["note"].s();

#if WEB_DEBUG_MODE
        std::cout << "[DEBUG] 保存笔记 - ID: " << id << ", Note: [" << note << "]\n";
#endif

        this->db.addNoteToSession(id, note);

#if WEB_DEBUG_MODE
        //调试：保存后立即打印数据库内容
        std::cout << "[DEBUG] 保存后的数据库状态:\n";
        this->db.debugPrintAllSessions();
#endif

        return crow::response(200, "Note added");
    });

    //4 .assets
    CROW_ROUTE(app, "/assets/<path>")
    ([](crow::response& res, std::string path){
        if (const std::string file_path = "dist/assets/" + path; std::filesystem::exists(file_path)) {
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

void WebServer::run(const int port)
{
    app.port(port).multithreaded().run();
}

