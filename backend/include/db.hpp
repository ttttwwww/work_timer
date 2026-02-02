#ifndef DB_HPP
#define DB_HPP

#include <sqlite3.h>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>

// 定义数据结构，对应前端需要的字段
struct WorkSession {
    int id;
    long long start_time;
    long long end_time;
    std::string type;
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

    void initTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS sessions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "start_time INTEGER NOT NULL,"
            "end_time INTEGER,"
            "type TEXT NOT NULL);";

        char* errMsg = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    }

    // --- 功能 1: 开始计时 (INSERT) ---
    void startSession(const long long startTime, const std::string& type) {
        // 为了简单演示，这里用 sprintf 拼接 SQL (实际生产建议用 bind 避免注入)
        // 注意：end_time 初始设为 0
        std::string sql = "INSERT INTO sessions (start_time, end_time, type) VALUES ("
                          + std::to_string(startTime) + ", 0, '" + type + "');";

        execute(sql);
    }

    // --- 功能 2: 停止计时 (UPDATE) ---
    // 逻辑：找到最近一条 end_time 为 0 的记录，把它填上当前时间
    void stopSession(long long endTime) {
        std::string sql = "UPDATE sessions SET end_time = " + std::to_string(endTime) +
                          " WHERE id = (SELECT id FROM sessions WHERE end_time = 0 ORDER BY id DESC LIMIT 1);";
        execute(sql);
    }

    // --- 功能 3: 获取所有历史 (SELECT) ---
    std::vector<WorkSession> getAllSessions() {
        std::vector<WorkSession> results;
        const char* sql = "SELECT id, start_time, end_time, type FROM sessions ORDER BY start_time DESC;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                WorkSession s;
                s.id = sqlite3_column_int(stmt, 0);
                s.start_time = sqlite3_column_int64(stmt, 1);
                s.end_time = sqlite3_column_int64(stmt, 2);

                // 取字符串稍微麻烦一点
                const unsigned char* typeText = sqlite3_column_text(stmt, 3);
                s.type = typeText ? reinterpret_cast<const char*>(typeText) : "unknown";

                results.push_back(s);
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

private:
    void execute(const std::string& sql) {
        char* errMsg = 0;
        int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL Error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }
};

#endif