#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

class config {
private:
    int port;
    std::string database_path;

    // 简单的 JSON 解析（仅适用于简单的 key-value 格式）
    void parseJson(const std::string& content) {
        std::istringstream stream(content);
        std::string line;

        while (std::getline(stream, line)) {
            // 查找 "port"
            size_t portPos = line.find("\"port\"");
            if (portPos != std::string::npos) {
                size_t colonPos = line.find(':', portPos);
                if (colonPos != std::string::npos) {
                    std::string numStr;
                    for (size_t i = colonPos + 1; i < line.length(); i++) {
                        if (isdigit(line[i])) {
                            numStr += line[i];
                        }
                    }
                    if (!numStr.empty()) {
                        port = std::stoi(numStr);
                    }
                }
            }

            // 查找 "database_path"
            size_t dbPos = line.find("\"database_path\"");
            if (dbPos != std::string::npos) {
                size_t colonPos = line.find(':', dbPos);
                if (colonPos != std::string::npos) {
                    size_t firstQuote = line.find('"', colonPos);
                    if (firstQuote != std::string::npos) {
                        size_t secondQuote = line.find('"', firstQuote + 1);
                        if (secondQuote != std::string::npos) {
                            database_path = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                        }
                    }
                }
            }
        }
    }

public:
    config(const std::string& config_path = "config.json")
        : port(8080), database_path("worktimer.db") {

        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "警告: 无法打开配置文件 " << config_path
                      << "，使用默认配置 (端口: " << port
                      << ", 数据库: " << database_path << ")" << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        parseJson(content);

        std::cout << "配置加载成功 - 端口: " << port
                  << ", 数据库: " << database_path << std::endl;
    }

    int getPort() const { return port; }
    std::string getDatabasePath() const { return database_path; }
};

#endif // CONFIG_H
