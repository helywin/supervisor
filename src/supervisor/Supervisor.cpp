//
// Created by jiang on 2021/5/31.
//

#include "Supervisor.hpp"
#include <iostream>
#include <cxxopts.hpp>
#include <json.hpp>

using json = nlohmann::json;

class SupervisorPrivate
{
public:
    json mSettings;

    void loadSettings();
};

void SupervisorPrivate::loadSettings()
{

}

Supervisor::Supervisor(int argc, char *argv[]) :
        d(new SupervisorPrivate)
{
    if (argc < 2) {
        std::cout << "supervisor命令：\n"
                  << "\tlist  -- 查看所有启动配置和运行中的配置\n"
                  << "\tstart -- 启动指定配置\n"
                  << "\tstart -- 停止指定配置" << std::endl;
        exit(0);
    }
    if (argc == 2 && std::string(argv[1]) == "list") {

    } else if (argc == 3 && std::string(argv[1]) == "list") {

    }
}

Supervisor::~Supervisor()
{
    delete d;
}

int Supervisor::exec()
{
    return 0;
}

int main(int argc, char *argv[])
{
    Supervisor supervisor(argc, argv);
    return supervisor.exec();
}
