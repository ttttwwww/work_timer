#ifndef DB_HPP
#define DB_HPP

#include <sqlite3.h>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct RequestError : std::runtime_error {
    int status;
    RequestError(int code, const std::string& message) : std::runtime_error(message), status(code) {}
};
struct WorkSession {
    int id{};
    long long start_time{}, end_time{};
    std::string type, note;
};
struct Task {
    int id{};
    std::string title, description, status;
    long long created_at{}, updated_at{};
    std::string note_location;
};
struct ProblemNode {
    int id{}, task_id{};
    std::string title, description, status;
    long long created_at{}, updated_at{};
    std::string note_location;
};
struct Todo {
    int id{}, node_id{};
    std::string title;
    bool done{};
    std::string note_location;
};
struct ProgressEntry {
    int id{}, node_id{}, todo_id{};
    std::string content;
    long long created_at{};
};
struct TaskBoard {
    std::vector<Task> tasks;
    std::vector<ProblemNode> nodes;
    std::vector<Todo> todos;
    std::vector<ProgressEntry> progress;
};

class Database {
    sqlite3* db{};
    mutable std::mutex mutex;

    // Statements own their SQLite resources; all user text is bound, never SQL.
    class Statement {
        sqlite3* db;
        sqlite3_stmt* stmt{};
    public:
        Statement(sqlite3* database, const char* sql) : db(database) {
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db));
        }
        ~Statement() { sqlite3_finalize(stmt); }
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
        void bind(int i, long long value) {
            if (sqlite3_bind_int64(stmt, i, value) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
        }
        void bind(int i, const std::string& value) {
            if (sqlite3_bind_text(stmt, i, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db));
        }
        // Input: optional text. Output: bound text or SQL NULL for omitted fields.
        void bindOptionalText(int i, const std::optional<std::string>& value) {
            if (value) bind(i, *value);
            else if (sqlite3_bind_null(stmt, i) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db));
        }
        bool next() {
            const int rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) return true;
            if (rc == SQLITE_DONE) return false;
            throw std::runtime_error(sqlite3_errmsg(db));
        }
        long long number(int i) const { return sqlite3_column_int64(stmt, i); }
        std::string text(int i) const {
            const auto* value = sqlite3_column_text(stmt, i);
            return value ? std::string(reinterpret_cast<const char*>(value), sqlite3_column_bytes(stmt, i)) : "";
        }
    };
    void execute(const char* sql) const {
        char* error = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error ? error : sqlite3_errmsg(db);
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }
    void requireChanged() const {
        if (!sqlite3_changes(db)) throw RequestError(404, "记录不存在，可能已在其他设备删除");
    }
    void requireParent(const char* sql, int id) const {
        Statement query(db, sql); query.bind(1, id);
        if (!query.next()) throw RequestError(404, "所属任务、节点或待办不存在，请刷新后重试");
    }
    // Input: the open database with the original task tables. Output: schema v1.
    // Move unassigned node logs into one explicit historical todo per node.
    // The transaction and version marker make migration atomic and repeatable.
    void migrateProgressToTodos() const {
        execute("BEGIN IMMEDIATE;");
        try {
            long long version = 0;
            {
                Statement query(db, "PRAGMA user_version;");
                if (query.next()) version = query.number(0);
            }
            if (version < 1) {
                execute("CREATE TABLE todo_progress (id INTEGER PRIMARY KEY AUTOINCREMENT, todo_id INTEGER NOT NULL REFERENCES node_todos(id) ON DELETE CASCADE, content TEXT NOT NULL, created_at INTEGER NOT NULL);"
                        "CREATE INDEX progress_todo ON todo_progress(todo_id);");
                std::vector<long long> nodeIds;
                {
                    Statement nodes(db, "SELECT DISTINCT node_id FROM node_progress ORDER BY node_id;");
                    while (nodes.next()) nodeIds.push_back(nodes.number(0));
                }
                for (const auto nodeId : nodeIds) {
                    Statement todo(db, "INSERT INTO node_todos(node_id,title,done) VALUES(?,?,0);");
                    todo.bind(1, nodeId);
                    todo.bind(2, std::string("历史进度（原节点记录）"));
                    todo.next();
                    const auto todoId = sqlite3_last_insert_rowid(db);
                    Statement copy(db, "INSERT INTO todo_progress(id,todo_id,content,created_at) SELECT id,?,content,created_at FROM node_progress WHERE node_id=?;");
                    copy.bind(1, todoId); copy.bind(2, nodeId); copy.next();
                }
                // Retain the original table as a migration backup; the new API
                // only reads/writes todo_progress after this point.
                execute("PRAGMA user_version = 1;");
            }
            execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
    // Input: schema v1 database. Output: schema v2 with empty note locations.
    // Keep all existing records and run the three column additions atomically.
    void migrateNoteLocations() const {
        execute("BEGIN IMMEDIATE;");
        try {
            long long version = 0;
            {
                Statement query(db, "PRAGMA user_version;");
                if (query.next()) version = query.number(0);
            }
            if (version < 2) {
                execute("ALTER TABLE tasks ADD COLUMN note_location TEXT NOT NULL DEFAULT '';"
                        "ALTER TABLE problem_nodes ADD COLUMN note_location TEXT NOT NULL DEFAULT '';"
                        "ALTER TABLE node_todos ADD COLUMN note_location TEXT NOT NULL DEFAULT '';"
                        "PRAGMA user_version = 2;");
            }
            execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
public:
    explicit Database(const std::string& path) {
        if (path != ":memory:") {
            const auto parent = std::filesystem::path(path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
        }
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            const std::string message = sqlite3_errmsg(db);
            sqlite3_close(db); db = nullptr;
            throw std::runtime_error(message);
        }
        try {
            sqlite3_busy_timeout(db, 5000);
            execute("PRAGMA foreign_keys = ON;");
            execute("CREATE TABLE IF NOT EXISTS sessions (id INTEGER PRIMARY KEY AUTOINCREMENT, start_time INTEGER NOT NULL, end_time INTEGER, type TEXT NOT NULL, note TEXT DEFAULT '');");
            bool hasNote = false;
            {
                Statement columns(db, "PRAGMA table_info(sessions);");
                while (columns.next()) if (columns.text(1) == "note") hasNote = true;
            }
            if (!hasNote) execute("ALTER TABLE sessions ADD COLUMN note TEXT DEFAULT '';");
            execute(
                "CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, description TEXT NOT NULL DEFAULT '', status TEXT NOT NULL DEFAULT 'todo' CHECK(status IN ('todo','doing','done')), created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL);"
                "CREATE TABLE IF NOT EXISTS problem_nodes (id INTEGER PRIMARY KEY AUTOINCREMENT, task_id INTEGER NOT NULL REFERENCES tasks(id) ON DELETE CASCADE, title TEXT NOT NULL, description TEXT NOT NULL DEFAULT '', status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','doing','resolved')), created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL);"
                "CREATE TABLE IF NOT EXISTS node_todos (id INTEGER PRIMARY KEY AUTOINCREMENT, node_id INTEGER NOT NULL REFERENCES problem_nodes(id) ON DELETE CASCADE, title TEXT NOT NULL, done INTEGER NOT NULL DEFAULT 0 CHECK(done IN (0,1)));"
                "CREATE TABLE IF NOT EXISTS node_progress (id INTEGER PRIMARY KEY AUTOINCREMENT, node_id INTEGER NOT NULL REFERENCES problem_nodes(id) ON DELETE CASCADE, content TEXT NOT NULL, created_at INTEGER NOT NULL);"
                "CREATE INDEX IF NOT EXISTS nodes_task ON problem_nodes(task_id);"
                "CREATE INDEX IF NOT EXISTS todos_node ON node_todos(node_id);"
                "CREATE INDEX IF NOT EXISTS progress_node ON node_progress(node_id);");
            migrateProgressToTodos();
            migrateNoteLocations();
        } catch (...) { sqlite3_close(db); db = nullptr; throw; }
    }
    ~Database() { sqlite3_close(db); }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    int startSession(long long now, const std::string& type) const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement insert(db, "INSERT INTO sessions(start_time,end_time,type) SELECT ?,0,? WHERE NOT EXISTS(SELECT 1 FROM sessions WHERE COALESCE(end_time,0)=0);");
        insert.bind(1, now); insert.bind(2, type); insert.next();
        if (!sqlite3_changes(db)) throw RequestError(409, "已有计时在进行中，请先结束");
        return static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    void stopSession(int id, long long end, long long now) const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement current(db, "SELECT start_time FROM sessions WHERE id=? AND COALESCE(end_time,0)=0;");
        current.bind(1, id);
        if (!current.next()) throw RequestError(409, "该计时已结束或不存在，请刷新后重试");
        if (end < current.number(0) || end > now)
            throw RequestError(400, "结束时间必须在开始时间与服务器当前时间之间");
        Statement update(db, "UPDATE sessions SET end_time=? WHERE id=? AND COALESCE(end_time,0)=0;");
        update.bind(1, end); update.bind(2, id); update.next();
    }
    std::vector<WorkSession> getAllSessions() const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement query(db, "SELECT id,start_time,COALESCE(end_time,0),type,note FROM sessions ORDER BY start_time DESC,id DESC;");
        std::vector<WorkSession> result;
        while (query.next()) result.push_back({static_cast<int>(query.number(0)), query.number(1), query.number(2), query.text(3), query.text(4)});
        return result;
    }
    void deleteSession(int id) const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement query(db, "SELECT COALESCE(end_time,0) FROM sessions WHERE id=?;"); query.bind(1,id);
        if (!query.next()) throw RequestError(404, "记录不存在");
        if (!query.number(0)) throw RequestError(409, "请先结束计时再删除记录");
        Statement remove(db, "DELETE FROM sessions WHERE id=?;"); remove.bind(1,id); remove.next();
    }
    void addNoteToSession(int id, const std::string& note) const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement update(db, "UPDATE sessions SET note=? WHERE id=?;");
        update.bind(1,note); update.bind(2,id); update.next(); requireChanged();
    }
    TaskBoard getBoard() const {
        std::lock_guard<std::mutex> lock(mutex);
        TaskBoard board;
        Statement tasks(db, "SELECT id,title,description,status,created_at,updated_at,note_location FROM tasks ORDER BY id DESC;");
        while (tasks.next()) board.tasks.push_back({static_cast<int>(tasks.number(0)),tasks.text(1),tasks.text(2),tasks.text(3),tasks.number(4),tasks.number(5),tasks.text(6)});
        Statement nodes(db, "SELECT id,task_id,title,description,status,created_at,updated_at,note_location FROM problem_nodes ORDER BY id;");
        while (nodes.next()) board.nodes.push_back({static_cast<int>(nodes.number(0)),static_cast<int>(nodes.number(1)),nodes.text(2),nodes.text(3),nodes.text(4),nodes.number(5),nodes.number(6),nodes.text(7)});
        Statement todos(db, "SELECT id,node_id,title,done,note_location FROM node_todos ORDER BY id;");
        while (todos.next()) board.todos.push_back({static_cast<int>(todos.number(0)),static_cast<int>(todos.number(1)),todos.text(2),todos.number(3)!=0,todos.text(4)});
        Statement progress(db, "SELECT p.id,t.node_id,p.todo_id,p.content,p.created_at FROM todo_progress p JOIN node_todos t ON t.id=p.todo_id ORDER BY p.id DESC;");
        while (progress.next()) board.progress.push_back({static_cast<int>(progress.number(0)),static_cast<int>(progress.number(1)),static_cast<int>(progress.number(2)),progress.text(3),progress.number(4)});
        return board;
    }
    // Input: task fields; omitted location preserves the saved value on update.
    // Output: created/updated task ID. Empty location explicitly clears it.
    int saveTask(int id, const std::string& title, const std::string& description, const std::string& status, long long now, const std::optional<std::string>& location = std::nullopt) const {
        std::lock_guard<std::mutex> lock(mutex);
        Statement query(db, id ? "UPDATE tasks SET title=?,description=?,status=?,updated_at=?,note_location=COALESCE(?,note_location) WHERE id=?;" : "INSERT INTO tasks(title,description,status,updated_at,note_location,created_at) VALUES(?,?,?,?,COALESCE(?,''),?);");
        query.bind(1,title); query.bind(2,description); query.bind(3,status); query.bind(4,now); query.bindOptionalText(5,location); query.bind(6,id ? id : now); query.next();
        if (id) requireChanged();
        return id ? id : static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    // Input: node fields and optional location. Output: saved node ID.
    // Updates retain parent ownership and retain location when omitted.
    int saveNode(int id, int taskId, const std::string& title, const std::string& description, const std::string& status, long long now, const std::optional<std::string>& location = std::nullopt) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (!id) requireParent("SELECT 1 FROM tasks WHERE id=?;",taskId);
        Statement query(db, id ? "UPDATE problem_nodes SET title=?,description=?,status=?,updated_at=?,note_location=COALESCE(?,note_location) WHERE id=?;" : "INSERT INTO problem_nodes(title,description,status,updated_at,note_location,created_at,task_id) VALUES(?,?,?,?,COALESCE(?,''),?,?);");
        query.bind(1,title); query.bind(2,description); query.bind(3,status); query.bind(4,now); query.bindOptionalText(5,location); query.bind(6,id ? id : now);
        if (!id) query.bind(7,taskId);
        query.next(); if (id) requireChanged();
        return id ? id : static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    // Input: todo fields and optional location. Output: saved todo ID.
    // Checkbox-only clients may omit location without erasing it.
    int saveTodo(int id, int nodeId, const std::string& title, bool done, const std::optional<std::string>& location = std::nullopt) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (!id) requireParent("SELECT 1 FROM problem_nodes WHERE id=?;",nodeId);
        Statement query(db,id ? "UPDATE node_todos SET title=?,done=?,note_location=COALESCE(?,note_location) WHERE id=?;" : "INSERT INTO node_todos(title,done,note_location,node_id) VALUES(?,?,COALESCE(?,''),?);");
        query.bind(1,title); query.bind(2,done ? 1 : 0); query.bindOptionalText(3,location); query.bind(4,id ? id : nodeId); query.next();
        if (id) requireChanged();
        return id ? id : static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    // Input: todo ID, validated text and server timestamp. Output: new log ID.
    // Every new progress entry belongs to exactly one existing todo.
    int addProgress(int todoId, const std::string& content, long long now) const {
        std::lock_guard<std::mutex> lock(mutex);
        requireParent("SELECT 1 FROM node_todos WHERE id=?;",todoId);
        Statement insert(db,"INSERT INTO todo_progress(todo_id,content,created_at) VALUES(?,?,?);");
        insert.bind(1,todoId); insert.bind(2,content); insert.bind(3,now); insert.next();
        return static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    void deleteBoardItem(const std::string& kind, int id) const {
        std::lock_guard<std::mutex> lock(mutex);
        // Only these hard-coded table names are accepted by callers.
        const char* sql = kind == "tasks" ? "DELETE FROM tasks WHERE id=?;" :
            kind == "nodes" ? "DELETE FROM problem_nodes WHERE id=?;" :
            kind == "todos" ? "DELETE FROM node_todos WHERE id=?;" :
            kind == "progress" ? "DELETE FROM todo_progress WHERE id=?;" : nullptr;
        if (!sql) throw RequestError(404,"未知资源");
        Statement remove(db,sql); remove.bind(1,id); remove.next(); requireChanged();
    }
};
#endif
