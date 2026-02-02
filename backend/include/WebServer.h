//
// Created by ttttwwww on 2026/2/2.
//

#ifndef WORKTIMER_WEBSERVER_H
#define WORKTIMER_WEBSERVER_H

#include "crow_all.h"
#include "db.hpp"

class WebServer
{
private:
    crow::SimpleApp app;
    void setupRoutes();
    Database& db;
public:
    explicit WebServer(Database& database);
    void run(int port);

};


#endif //WORKTIMER_WEBSERVER_H