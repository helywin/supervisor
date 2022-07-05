//
// Created by jiang on 2021/5/31.
//

#include "Supervisord.hpp"
#include "QStringJson.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>
#include <QRegExp>
#include <algorithm>
#include <boost/process.hpp>
#include <cstdio>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

bool DEBUG = false;
std::string JSON_FILE = "/data/caller_table.json";
json CONF;
std::string MODE;
QVector<QString> AUTO_START_LIST;

std::string &replace_all(std::string &src, const std::string &old_value,
                         const std::string &new_value)
{
    // 每次重新定位起始位置，防止上轮替换后的字符串形成新的old_value
    for (std::string::size_type pos(0); pos != std::string::npos; pos += new_value.length()) {
        if ((pos = src.find(old_value, pos)) != std::string::npos) {
            src.replace(pos, old_value.length(), new_value);
        } else {
            break;
        };
    }
    return src;
}

std::string parseEnv(std::string s)
{
    std::regex regex(R"(\$\{(.*?)\})");
    auto words_begin = std::sregex_iterator(s.begin(), s.end(), regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        const std::smatch &match = *i;
        assert(match.size() == 2);
        const char *env = std::getenv(match[1].str().c_str());
        if (!env) {
            qWarning() << "环境变量不存在:" << QString::fromStdString(match[1].str());
        }
        replace_all(s, match[0].str(), env);
    }
    return s;
}

struct ProcessConfig
{
    QString executor;
    QString path;
    QString name;
    QString working_dir;
    bool detach;
    QString stdout;
    QString stderr;
    bool restart;
    int delay;
    QStringList params;
};

class SupervisordPrivate
{
public:
    Q_DECLARE_PUBLIC(Supervisord)
    Supervisord *q_ptr;
    int mArgc;
    char **mArgv;
    QUdpSocket *mSocket;
    QList<QString> mCurrentModes;
    QMap<QString, QList<QProcess *>> mProcessList;
    QMap<QString, QStringList> mPreExec;

    explicit SupervisordPrivate(Supervisord *p);
    void onReadyRead();
    void createProcesses();
    void startProcesses(const QString &mode);
    void stopProcesses(const QString &mode);
    void stopAllProcesses();
    void killAllProcesses();
    void prepare();
};

SupervisordPrivate::SupervisordPrivate(Supervisord *p) :
        q_ptr(p)
{}


void SupervisordPrivate::onReadyRead()
{
    while (mSocket->hasPendingDatagrams()) {
        std::cout << "recv datagram" << std::endl;
        std::string array(mSocket->pendingDatagramSize(), 0);
        QHostAddress address;
        quint16 port;
        mSocket->readDatagram(array.data(), array.length(), &address, &port);
        nlohmann::json json = nlohmann::json::parse(array);
        nlohmann::json response;
        if (!json.contains("command")) {
            response["error"] = true;
            response["error_string"] = "不包含命令报文";
        } else if (json["command"] == "query") {

        } else if (json["command"] == "control") {
            if (!json.contains("mode_name") || !json.contains("enable")) {
                std::cout << "bad json object" << std::endl;
                return;
            }
            if (!mProcessList.contains(json["mode_name"])) {
                std::cout << "mode not exist " << json["mode_name"] << std::endl;
                return;
            }
            if (json["enable"].get<bool>()) {
                startProcesses(json["mode_name"]);
            } else {
                stopProcesses(json["mode_name"]);
            }
        } else {
            response["error"] = true;
            response["error_string"] = "报文命令错误";
        }

        nlohmann::json total;
        for (const auto &key: mProcessList.keys()) {
            total.emplace_back(key);
        }
        nlohmann::json enabled;
        for (const auto &key: mCurrentModes) {
            enabled.emplace_back(key);
        }
        response["status"] = {
                {"total",   total},
                {"enabled", enabled},
        };
        std::string data = response.dump(4);
        mSocket->writeDatagram(data.data(), data.length(), address, port);
    }
}

void SupervisordPrivate::createProcesses()
{
    Q_Q(Supervisord);
    if (!CONF.contains("start_ros_core")) {
        CONF["start_ros_core"] = false;
    }
    if (!CONF.contains("terminal_cmd")) {
        CONF["terminal_cmd"] = "gnome-terminal -x bash -c \"%1\"";
    }
    if (!CONF.contains("roscore_delay")) {
        CONF["roscore_delay"] = 0;
    }
    for (const auto &item: CONF["modes"]) {
        QString name = item["name"];
        if (item.contains("pre_exec")) {
            mPreExec[name] = QStringList();
            for (const auto &cmd: item["pre_exec"]) {
                mPreExec[name].append(QString::fromStdString(parseEnv(cmd)));
            }
        }
        mProcessList[name] = QList<QProcess *>();
        for (const auto &object: item["executables"]) {
            auto process = new QProcess(q);
            process->setEnvironment(QProcess::systemEnvironment());
            mProcessList[name].append(process);
            QStringList command;
            bool terminal = false;
            if (object.contains("terminal")) {
                terminal = true;
            }
            bool hasExecutor = object.contains("executor");
            if (hasExecutor) {
                command << QString::fromStdString(object["executor"]);
            }
            QString path;
            if (object.contains("path")) {
                path = QString::fromStdString(parseEnv(object["path"]));
                if (!path.isEmpty() && !path.endsWith('/')) {
                    path += "/";
                }
            }
            QString procName = QString::fromStdString(object["name"]);
            process->setProperty("name", procName);
            command << path + procName;

            //            process->setProgram(command);
            if (object.contains("working_dir")) {
                process->setWorkingDirectory(
                        QString::fromStdString(parseEnv(object["working_dir"])));
            }
            if (object.contains("detach")) {
                process->setProperty("detach", QString::fromStdString(object["detach"]));
            } else {
                process->setProperty("detach", false);
            }
            if (object.contains("stdout")) {
                process->setStandardOutputFile(QString::fromStdString(object["stdout"]));
            }
            if (object.contains("stderr")) {
                process->setStandardErrorFile(QString::fromStdString(object["stderr"]));
            }
            if (object.contains("restart")) {
                process->setProperty("restart", object["restart"].get<bool>());
            } else {
                process->setProperty("restart", false);
            }
            if (object.contains("delay")) {
                process->setProperty("delay", object["delay"].get<int>());
            } else {
                process->setProperty("delay", 0);
            }
            if (object.contains("params")) {
                for (const auto &param: object["params"]) {
                    command << QString::fromStdString(param);
                }
            }
            QString fullCommand = command.join(' ');
            if (terminal) {
                fullCommand = QString::fromStdString(CONF["terminal_cmd"]).arg(fullCommand);
            }
            process->setProperty("command", fullCommand);
            std::cout << "create: " << fullCommand.toStdString() << std::endl;
            QObject::connect(
                    process,
                    static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                    q, &Supervisord::onProcessFinished);
            QObject::connect(process,
                             static_cast<void (QProcess::*)(QProcess::ProcessError)>(
                                     &QProcess::errorOccurred),
                             q, &Supervisord::onErrorOccurred);
        }
    }
}


void SupervisordPrivate::startProcesses(const QString &mode)
{
    std::cout << "---------------------------------------------------" << std::endl;
    if (mCurrentModes.contains(mode)) {
        return;
    } else {
        mCurrentModes.append(mode);
    }
    std::cout << "pre exec: " << std::endl;
    for (const auto &cmd: mPreExec[mode]) {
        std::cout << "  " << cmd.toStdString() << std::endl;
        QProcess::startDetached(cmd);
    }
    for (auto process: mProcessList[mode]) {
        if (process->property("detach").toBool()) {
            QProcess::startDetached(process->property("command").toString());
        } else {
            process->setProperty("manually_close", false);
            int delay = process->property("delay").toInt();
            QTimer::singleShot(delay, [process] {
                process->start(process->property("command").toString(),
                               QIODevice::ReadOnly);
                std::cout << "start: " << process->property("command").toString().toStdString()
                          << " pid:" << process->pid() << std::endl;
            });
        }
    }
}

void SupervisordPrivate::stopProcesses(const QString &mode)
{
    std::cout << "---------------------------------------------------" << std::endl;
    if (!mCurrentModes.contains(mode)) {
        return;
    } else {
        mCurrentModes.removeOne(mode);
    }
    for (auto process: mProcessList[mode]) {
        if (process->isOpen()) {
            process->setProperty("manually_close", true);
            process->kill();
            process->close();
        }
    }
    std::cout << "---------------------------------------------------" << std::endl;
}

void SupervisordPrivate::stopAllProcesses()
{
    for (auto &list: mProcessList) {
        for (auto process: list) {
            if (process->isOpen()) {
                process->setProperty("manually_close", true);
                process->kill();
                process->close();
            }
        }
    }
    mCurrentModes.clear();
}

void SupervisordPrivate::killAllProcesses()
{}

void SupervisordPrivate::prepare()
{
    Q_Q(Supervisord);
    for (const auto &mode: AUTO_START_LIST) { startProcesses(mode); }
    mSocket = new QUdpSocket(q);
    if (!mSocket->bind(CONF["port"].get<int>())) {
        if (mSocket->error() == QUdpSocket::AddressInUseError) {
            std::cout << "已经有相同的实例在运行或者端口33496别占用" << std::endl;
            QCoreApplication::quit();
        }
    }
    QObject::connect(mSocket, &QUdpSocket::readyRead, [this] { onReadyRead(); });
}

Supervisord::Supervisord(int argc, char **argv) :
        d_ptr(new SupervisordPrivate(this))
{
    Q_D(Supervisord);
    d->mArgc = argc;
    d->mArgv = argv;

    //    cxxopts::Options options("supervd", "进程管理程序");
    //    options.add_options()
    //            ("c", "", );
    d->createProcesses();
}

Supervisord::~Supervisord()
{
    Q_D(Supervisord);
    d->stopAllProcesses();
    d->mSocket->blockSignals(true);
    d->mSocket->close();
}

int Supervisord::exec()
{
    Q_D(Supervisord);
    QCoreApplication app(d->mArgc, d->mArgv);
    if (CONF.contains("start_ros_core") && CONF["start_ros_core"].get<bool>()) {
        system("gnome-terminal -x bash -c 'roscore'&");
        std::this_thread::sleep_for(std::chrono::milliseconds(CONF["roscore_delay"]));
    }
    QTimer::singleShot(100, [this] { d_ptr->prepare(); });
    return QCoreApplication::exec();
}

void Supervisord::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto process = qobject_cast<QProcess *>(sender());
    std::cout << "process " << process->program().toStdString() << " "
              << "finished: ";
    if (process->error() == QProcess::UnknownError) {
        std::cout << "normal exit" << std::endl;
    } else {
        std::cout << process->errorString().toStdString() << std::endl;
    }
    if (process->property("restart").toBool() && !process->property("manually_close").toBool()) {
        QTimer::singleShot(1000, [process] {
            process->start();
            std::cout << "restart " << process->program().toStdString() << " pid:" << process->pid()
                      << std::endl;
        });
    }
}

void Supervisord::onErrorOccurred(QProcess::ProcessError error)
{
    auto process = qobject_cast<QProcess *>(sender());
    std::cout << "process " << process->program().toStdString() << " "
              << "error: " << process->errorString().toStdString() << std::endl;
}

void Supervisord::stop()
{
    Q_D(Supervisord);
    d->killAllProcesses();
}

std::shared_ptr<Supervisord> sp;

void sigHandler(int sig)
{
    std::string sigStr;
    switch (sig) {
        case SIGINT:
            sigStr = "SIGINT";
            break;
        case SIGILL:
            sigStr = "SIGILL";
            break;
        case SIGABRT:
            sigStr = "SIGABRT";
            break;
        case SIGFPE:
            sigStr = "SIGFPE";
            break;
        case SIGSEGV:
            sigStr = "SIGSEGV";
            break;
        case SIGTERM:
            sigStr = "SIGTERM";
            break;
        default:
            break;
    }
    std::cout << "accept signal: " << sigStr << std::endl;
    sp->stop();
    exit(-1);
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("supervisord", "ros process starter and manager");
    // clang-format off
    options.add_options()
            ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
            ("s,select", "Select process group to start")
            ("f,file", "Start specified json file to start", cxxopts::value<std::string>())
            ("h,help", "Print usage");
    // clang-format on
    auto result = options.parse(argc, argv);
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    if (result.count("file")) {
        auto file = result["file"].as<std::string>();
        if (QFile::exists(QString::fromStdString(file))) { JSON_FILE = file; }
    }
    DEBUG = result["debug"].as<bool>();
    bool select = result.count("select");


    std::ifstream is(JSON_FILE);
    if (!is.is_open()) {
        std::cout << "打开配置文件失败 /data/caller_table.json: " << std::strerror(errno)
                  << std::endl;
        exit(-1);
    }
    is >> CONF;

    if (select) {
        std::cout << "Select Mode: ";
        for (int i = 0; i < CONF["modes"].size(); ++i) {
            std::cout << CONF["modes"][i]["name"].get<std::string>() << "(" << i << ") ";
        }
        std::cout << "  (more than one separated with ,)>";
        std::string input;
        std::cin >> input;
        auto strList = QString::fromStdString(input).split(",", QString::SkipEmptyParts);
        QVector<int> idxList(strList.size());
        std::transform(strList.begin(), strList.end(), idxList.begin(),
                       [](const QString &str) { return str.toInt(); });
        std::for_each(idxList.begin(), idxList.end(), [](int index) {
            AUTO_START_LIST.append(QString::fromStdString(CONF["modes"][index]["name"]));
        });
    } else {
        for (const auto &item: CONF["start"]) {
            AUTO_START_LIST.append(QString::fromStdString(item));
        }
    }

    sp = std::make_shared<Supervisord>(argc, argv);
    signal(SIGINT, sigHandler);
    signal(SIGILL, sigHandler);
    signal(SIGABRT, sigHandler);
    signal(SIGFPE, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);
    return sp->exec();
}
