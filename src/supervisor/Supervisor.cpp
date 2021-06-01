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

Supervisor::Supervisor(int argc, char **argv) :
        d(new SupervisorPrivate)
{
    cxxopts::Options options("Superv", "进程管理工具命令行");
    options.add_options()
            ("s,status", "")
            ("l,list", "Int param", cxxopts::value<int>())
            ("e,enable", "File name", cxxopts::value<std::string>())
            ("d,disable", "Verbose output", cxxopts::value<bool>()->default_value("false"));
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
