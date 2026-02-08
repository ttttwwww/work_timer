#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

class config {
private:
    int port;
    std::string bind_address;
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

            // 查找 "bind_address"
            size_t bindPos = line.find("\"bind_address\"");
            if (bindPos != std::string::npos) {
                size_t colonPos = line.find(':', bindPos);
                if (colonPos != std::string::npos) {
                    size_t firstQuote = line.find('"', colonPos);
                    if (firstQuote != std::string::npos) {
                        size_t secondQuote = line.find('"', firstQuote + 1);
                        if (secondQuote != std::string::npos) {
                            bind_address = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                        }
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
        : port(8080), bind_address("127.0.0.1"), database_path("../data/worktimer.db") {

        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open config file " << config_path
                      << ", using defaults (port: " << port
                      << ", bind_address: " << bind_address
                      << ", database: " << database_path << ")" << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        parseJson(content);

        std::cout << "Config loaded - Port: " << port
                  << ", Bind Address: " << bind_address
                  << ", Database: " << database_path << std::endl;
    }

    int getPort() const { return port; }
    std::string getBindAddress() const { return bind_address; }
    std::string getDatabasePath() const { return database_path; }
};

#endif // CONFIG_H
