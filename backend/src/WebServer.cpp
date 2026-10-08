#include "WebServer.h"
#include <limits>

namespace {
using Json = crow::json::wvalue;
using Input = crow::json::rvalue;
long long nowSeconds() { return std::time(nullptr); }
crow::response jsonResponse(Json value, int status = 200) {
    crow::response response(status, value.dump());
    response.set_header("Content-Type", "application/json; charset=utf-8");
    response.set_header("Cache-Control", "no-store");
    return response;
}
template <typename F> crow::response api(F action) {
    try { return action(); }
    catch (const RequestError& error) {
        return jsonResponse(Json{{"error",error.what()}},error.status);
    } catch (const std::exception& error) {
        CROW_LOG_ERROR << error.what();
        return jsonResponse(Json{{"error","服务器处理失败，请重试"}},500);
    }
}
Input parse(const crow::request& request) {
    auto data = crow::json::load(request.body);
    if (!data || data.t() != crow::json::type::Object) throw RequestError(400,"请求必须是 JSON 对象");
    return data;
}
std::string textField(const Input& data, const char* key, size_t limit, bool required = false) {
    if (!data.has(key) || data[key].t() != crow::json::type::String) throw RequestError(400,std::string("无效文本字段: ")+key);
    std::string value = data[key].s();
    if (value.size() > limit || (required && value.find_first_not_of(" \t\r\n") == std::string::npos))
        throw RequestError(400,std::string("文本为空或过长: ")+key);
    return value;
}
// Input: request object. Output: optional plain-text location, never opened.
// Missing preserves older clients' updates; an empty string clears the value.
std::optional<std::string> noteLocationField(const Input& data) {
    if (!data.has("note_location")) return std::nullopt;
    return textField(data,"note_location",4000);
}
long long integer(const Input& data, const char* key) {
    if (!data.has(key) || data[key].t() != crow::json::type::Number || data[key].nt() == crow::json::num_type::Floating_point)
        throw RequestError(400,std::string("无效整数字段: ")+key);
    const double value = data[key].d();
    if (value < 1 || value > 9007199254740991.0) throw RequestError(400,"数字超出范围");
    return data[key].i();
}
int idField(const Input& data, const char* key) {
    auto id = integer(data,key);
    if (id > std::numeric_limits<int>::max()) throw RequestError(400,"无效记录 ID");
    return static_cast<int>(id);
}
std::string statusField(const Input& data, bool task) {
    auto status = textField(data,"status",20,true);
    if (status != "doing" && (task ? status != "todo" && status != "done" : status != "open" && status != "resolved"))
        throw RequestError(400,"无效状态");
    return status;
}
Json sessionsJson(const std::vector<WorkSession>& sessions) {
    std::vector<Json> list;
    for (const auto& s : sessions) list.push_back(Json{{"id",s.id},{"start_time",static_cast<std::int64_t>(s.start_time)},{"end_time",static_cast<std::int64_t>(s.end_time)},{"type",s.type},{"note",s.note}});
    return Json(std::move(list));
}
}

WebServer::WebServer(Database& database) : db(database) { setupRoutes(); }
void WebServer::setupRoutes() {
    CROW_ROUTE(app,"/api/start").methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        return api([&] {
            const auto data = parse(req);
            const auto type = textField(data,"type",20,true);
            if (type != "formal" && type != "informal") throw RequestError(400,"无效计时类型");
            return jsonResponse(Json{{"id",db.startSession(nowSeconds(),type)}},201);
        });
    });
    CROW_ROUTE(app,"/api/stop").methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        return api([&] {
            const auto data = parse(req);
            const auto now = nowSeconds();
            const auto end = data.has("end_time") ? integer(data,"end_time") : now;
            db.stopSession(idField(data,"id"),end,now);
            return jsonResponse(Json{{"ok",true}});
        });
    });
    // Keep the original array response for existing history clients.
    CROW_ROUTE(app,"/api/history")([this] {
        return api([&] { return jsonResponse(sessionsJson(db.getAllSessions())); });
    });
    CROW_ROUTE(app,"/api/state")([this] {
        return api([&] {
            Json result;
            result["sessions"] = sessionsJson(db.getAllSessions());
            result["server_time"] = static_cast<std::int64_t>(nowSeconds());
            return jsonResponse(std::move(result));
        });
    });
    CROW_ROUTE(app,"/api/delete").methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        return api([&] { db.deleteSession(idField(parse(req),"id")); return jsonResponse(Json{{"ok",true}}); });
    });
    CROW_ROUTE(app,"/api/note").methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        return api([&] {
            const auto data = parse(req);
            db.addNoteToSession(idField(data,"id"),textField(data,"note",20000));
            return jsonResponse(Json{{"ok",true}});
        });
    });
    CROW_ROUTE(app,"/api/tasks")([this] {
        return api([&] {
            const auto board = db.getBoard();
            std::vector<Json> tasks, nodes, todos, progress;
            for (const auto& t : board.tasks) tasks.push_back(Json{{"id",t.id},{"title",t.title},{"description",t.description},{"status",t.status},{"note_location",t.note_location},{"created_at",static_cast<std::int64_t>(t.created_at)},{"updated_at",static_cast<std::int64_t>(t.updated_at)}});
            for (const auto& n : board.nodes) nodes.push_back(Json{{"id",n.id},{"task_id",n.task_id},{"title",n.title},{"description",n.description},{"status",n.status},{"note_location",n.note_location},{"created_at",static_cast<std::int64_t>(n.created_at)},{"updated_at",static_cast<std::int64_t>(n.updated_at)}});
            for (const auto& t : board.todos) todos.push_back(Json{{"id",t.id},{"node_id",t.node_id},{"title",t.title},{"done",t.done},{"note_location",t.note_location}});
            for (const auto& p : board.progress) progress.push_back(Json{{"id",p.id},{"node_id",p.node_id},{"todo_id",p.todo_id},{"content",p.content},{"created_at",static_cast<std::int64_t>(p.created_at)}});
            Json result;
            result["tasks"]=std::move(tasks); result["nodes"]=std::move(nodes);
            result["todos"]=std::move(todos); result["progress"]=std::move(progress);
            return jsonResponse(std::move(result));
        });
    });
    CROW_ROUTE(app,"/api/tasks").methods(crow::HTTPMethod::POST)([this](const crow::request& req) {
        return api([&] {
            const auto data = parse(req);
            return jsonResponse(Json{{"id",db.saveTask(0,textField(data,"title",800,true),textField(data,"description",20000),statusField(data,true),nowSeconds(),noteLocationField(data))}},201);
        });
    });
    CROW_ROUTE(app,"/api/tasks/<int>").methods(crow::HTTPMethod::PUT)([this](const crow::request& req,int id) {
        return api([&] {
            if (id <= 0) throw RequestError(400,"无效记录 ID");
            const auto data = parse(req);
            return jsonResponse(Json{{"id",db.saveTask(id,textField(data,"title",800,true),textField(data,"description",20000),statusField(data,true),nowSeconds(),noteLocationField(data))}});
        });
    });
    CROW_ROUTE(app,"/api/tasks/<int>/nodes").methods(crow::HTTPMethod::POST)([this](const crow::request& req,int taskId) {
        return api([&] {
            const auto data = parse(req);
            return jsonResponse(Json{{"id",db.saveNode(0,taskId,textField(data,"title",800,true),textField(data,"description",20000),statusField(data,false),nowSeconds(),noteLocationField(data))}},201);
        });
    });
    CROW_ROUTE(app,"/api/nodes/<int>").methods(crow::HTTPMethod::PUT)([this](const crow::request& req,int id) {
        return api([&] {
            if (id <= 0) throw RequestError(400,"无效记录 ID");
            const auto data = parse(req);
            return jsonResponse(Json{{"id",db.saveNode(id,0,textField(data,"title",800,true),textField(data,"description",20000),statusField(data,false),nowSeconds(),noteLocationField(data))}});
        });
    });
    CROW_ROUTE(app,"/api/nodes/<int>/todos").methods(crow::HTTPMethod::POST)([this](const crow::request& req,int nodeId) {
        return api([&] {
            const auto data = parse(req);
            return jsonResponse(Json{{"id",db.saveTodo(0,nodeId,textField(data,"title",800,true),false,noteLocationField(data))}},201);
        });
    });
    CROW_ROUTE(app,"/api/todos/<int>").methods(crow::HTTPMethod::PUT)([this](const crow::request& req,int id) {
        return api([&] {
            if (id <= 0) throw RequestError(400,"无效记录 ID");
            const auto data = parse(req);
            if (!data.has("done") || (data["done"].t() != crow::json::type::True && data["done"].t() != crow::json::type::False)) throw RequestError(400,"无效待办状态");
            return jsonResponse(Json{{"id",db.saveTodo(id,0,textField(data,"title",800,true),data["done"].b(),noteLocationField(data))}});
        });
    });
    CROW_ROUTE(app,"/api/todos/<int>/progress").methods(crow::HTTPMethod::POST)([this](const crow::request& req,int todoId) {
        return api([&] { return jsonResponse(Json{{"id",db.addProgress(todoId,textField(parse(req),"content",20000,true),nowSeconds())}},201); });
    });
    // Old pages must refresh rather than create another unassigned node log.
    CROW_ROUTE(app,"/api/nodes/<int>/progress").methods(crow::HTTPMethod::POST)([](int) {
        return jsonResponse(Json{{"error","进度记录已移入待办，请刷新页面后在具体待办内记录"}},410);
    });
    CROW_ROUTE(app,"/api/<string>/<int>").methods(crow::HTTPMethod::DELETE)([this](std::string kind,int id) {
        return api([&] { db.deleteBoardItem(kind,id); return jsonResponse(Json{{"ok",true}}); });
    });
    CROW_ROUTE(app,"/assets/<path>")([](crow::response& res,std::string path) {
        const auto relative = std::filesystem::path(path);
        bool safe = !relative.is_absolute();
        for (const auto& part : relative) if (part == "..") safe = false;
        const auto file = std::filesystem::path("dist/assets") / relative;
        if (safe && std::filesystem::is_regular_file(file)) res.set_static_file_info(file.string());
        else res.code = 404;
        res.end();
    });
    CROW_ROUTE(app,"/")([](crow::response& res) {
        if (std::filesystem::exists("dist/index.html")) res.set_static_file_info("dist/index.html");
        else { res.code=404; res.write("dist/index.html not found. Run make first."); }
        res.end();
    });
}
void WebServer::run(int port,const std::string& bind_address) {
    if (port <= 0 || port > 65535) throw std::runtime_error("Invalid port number");
    app.bindaddr(bind_address).port(static_cast<uint16_t>(port)).multithreaded().run();
}
