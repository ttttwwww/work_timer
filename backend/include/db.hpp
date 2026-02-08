#ifndef DB_HPP
#define DB_HPP

#include <sqlite3.h>
#include <vector>
#include <string>
#include <iostream>

// 调试开关：由 CMake 自动设置（不需要手动修改）
// 如果 CMake 没有定义，默认开启调试
#ifndef DB_DEBUG_MODE
    #define DB_DEBUG_MODE 1
#endif

// 定义数据结构，对应前端需要的字段
struct WorkSession {
    int id{};
    long long start_time{};
    long long end_time{};
    std::string type;
    std::string note; // 可选：添加笔记字段
};

class Database {
private:
    sqlite3* db{};

public:
    explicit Database(const std::string& db_path) {
        // 1. 打开/创建数据库文件
        if (int rc = sqlite3_open(db_path.c_str(), &db)) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        } else {
            // 2. 确保表存在
            initTable();
        }
    }

    ~Database() {
        sqlite3_close(db);
    }

    void initTable() const
    {
        const auto sql =
            "CREATE TABLE IF NOT EXISTS sessions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "start_time INTEGER NOT NULL,"
            "end_time INTEGER,"
            "type TEXT NOT NULL,"
            "note TEXT DEFAULT '');";

        char* errMsg = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

        // 为已存在的数据库添加 note 列（如果不存在）
        const auto alterSql = "ALTER TABLE sessions ADD COLUMN note TEXT DEFAULT '';";
        sqlite3_exec(db, alterSql, nullptr, nullptr, &errMsg);
        // 忽略错误（如果列已存在会失败，这是正常的）
    }

    // --- 开始计时 (INSERT) ---
    void startSession(const long long startTime, const std::string& type) const
    {
        // 为了简单演示，这里用 sprintf 拼接 SQL (实际生产建议用 bind 避免注入)
        // 注意：end_time 初始设为 0
        const std::string sql = "INSERT INTO sessions (start_time, end_time, type) VALUES ("
                          + std::to_string(startTime) + ", 0, '" + type + "');";

        execute(sql);
    }

    // --- 停止计时 (UPDATE) ---
    // 逻辑：找到最近一条 end_time 为 0 的记录，把它填上当前时间
    void stopSession(const long long endTime) const
    {
        const std::string sql = "UPDATE sessions SET end_time = " + std::to_string(endTime) +
                          " WHERE id = (SELECT id FROM sessions WHERE end_time = 0 ORDER BY id DESC LIMIT 1);";
        execute(sql);
    }

    // --- 功能: 获取所有历史 (SELECT) ---
    [[nodiscard]] std::vector<WorkSession> getAllSessions() const
    {
        std::vector<WorkSession> results;
        sqlite3_stmt* stmt;

        // 🐛 修复：SQL 查询中必须包含 note 字段！
        if (const auto sql = "SELECT id, start_time, end_time, type, note FROM sessions ORDER BY start_time DESC;"; sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                WorkSession s;
                s.id = sqlite3_column_int(stmt, 0);
                s.start_time = sqlite3_column_int64(stmt, 1);
                s.end_time = sqlite3_column_int64(stmt, 2);

                // 取字符串稍微麻烦一点
                const unsigned char* typeText = sqlite3_column_text(stmt, 3);
                s.type = typeText ? reinterpret_cast<const char*>(typeText) : "unknown";
                const unsigned char* noteText = sqlite3_column_text(stmt, 4);
                s.note = noteText ? reinterpret_cast<const char*>(noteText) : "";
                results.push_back(s);
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }
    // 功能:删除指定id的记录
    void deleteSession(const int id) const
    {
        const auto sql = "DELETE FROM sessions WHERE id = " + std::to_string(id)+ ";";
        execute(sql);
    }
    // 功能:针对指定id添加笔记
    void addNoteToSession(const int id, const std::string& note) const
    {
        // 假设 sessions 表中有一个 note 字段
        const auto sql = "UPDATE sessions SET note = '" + note + "' WHERE id = " + std::to_string(id) + ";";
        execute(sql);
    }

    // 🔍 调试功能：打印所有数据库记录到控制台
    void debugPrintAllSessions() const
    {
#if DB_DEBUG_MODE
        std::cout << "\n========== 数据库内容 ==========\n";
        auto sessions = getAllSessions();
        if (sessions.empty()) {
            std::cout << "(数据库为空)\n";
        } else {
            for (const auto& s : sessions) {
                std::cout << "ID: " << s.id
                          << " | Type: " << s.type
                          << " | Start: " << s.start_time
                          << " | End: " << s.end_time
                          << " | Note: [" << s.note << "]\n";
            }
        }
        std::cout << "================================\n\n";
#endif
    }

private:
    // --- 辅助函数：执行 SQL 语句并处理错误 ---
    void execute(const std::string& sql) const
    {
        char* errMsg = nullptr;
        if (const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg); rc != SQLITE_OK) {
            std::cerr << "SQL Error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }
};

#endif