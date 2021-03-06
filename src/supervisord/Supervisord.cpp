//
// Created by jiang on 2021/5/31.
//

#include "Supervisord.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>
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
        QByteArray array;
        array.resize((int) mSocket->pendingDatagramSize());
        QHostAddress address;
        quint16 port;
        mSocket->readDatagram(array.data(), array.size(), &address, &port);
        QJsonDocument doc = QJsonDocument::fromJson(array);
        auto object = doc.object();
        QJsonObject response;
        if (!object.contains("command")) {
            response["error"] = true;
            response["error_string"] = "不包含命令报文";
        } else if (object["command"] == "query") {

        } else if (object["command"] == "control") {
            if (!object.contains("mode_name") || !object.contains("enable")) {
                std::cout << "bad json object" << std::endl;
                return;
            }
            if (!mProcessList.contains(object["mode_name"].toString())) {
                std::cout << "mode not exist " << object["mode_name"].toString().toStdString()
                          << std::endl;
                return;
            }
            if (object["enable"].toBool()) {
                startProcesses(object["mode_name"].toString());
            } else {
                stopProcesses(object["mode_name"].toString());
            }
        } else {
            response["error"] = true;
            response["error_string"] = "报文命令错误";
        }

        QJsonArray total;
        for (const auto &key: mProcessList.keys()) {
            total.append(key);
        }
        QJsonArray enabled;
        for (const auto &key: mCurrentModes) {
            enabled.append(key);
        }
        QJsonObject status;
        status["total"] = total;
        status["enabled"] = enabled;
        response["status"] = status;
        QJsonDocument sendDoc(response);
        mSocket->writeDatagram(sendDoc.toJson(), address, port);
    }
}

void SupervisordPrivate::createProcesses()
{
    Q_Q(Supervisord);
    if (!CONF.contains("start_ros_core")) {
        CONF["start_ros_core"] = false;
    }
    if (!CONF.contains("roscore_delay")) {
        CONF["roscore_delay"] = 0;
    }
    for (const auto &item: CONF["modes"]) {
        QString name = QString::fromStdString(item["name"]);
        if (item.contains("pre_exec")) {
            mPreExec[name] = QStringList();
            for (const auto &cmd: item["pre_exec"]) {
                mPreExec[name].append(QString::fromStdString(cmd));
            }
        }
        mProcessList[name] = QList<QProcess *>();
        for (const auto &object: item["executables"]) {
            auto process = new QProcess(q);
            process->setEnvironment(QProcess::systemEnvironment());
            mProcessList[name].append(process);
            QString command;
            QStringList params;
            bool hasExecutor = object.contains("executor");
            if (hasExecutor) {
                command += QString::fromStdString(object["executor"]) + " ";
                process->setProperty("executor", true);
            } else {
                process->setProperty("executor", false);
            }
            QString path;
            if (object.contains("path")) {
                path = QString::fromStdString(object["path"]);
                if (!path.isEmpty() && !path.endsWith('/')) {
                    path += "/";
                }
            }
            QString procName = QString::fromStdString(object["name"]);
            process->setProperty("name", procName);
            if (!hasExecutor) {
                command += path + procName;
            } else {
                params.append(path + procName);
            }

            process->setProgram(command);
            if (object.contains("working_dir")) {
                process->setWorkingDirectory(QString::fromStdString(object["working_dir"]));
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
                    params << QString::fromStdString(param);
                }
                process->setArguments(params);
            }
            std::cout << "create: " << command.toStdString() << " "
                      << params.join(' ').toStdString() << std::endl;
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
            QProcess::startDetached(process->program());
        } else {
            process->setProperty("manually_close", false);
            int delay = process->property("delay").toInt();
            QTimer::singleShot(delay, [process] {
                if (process->property("executor").toBool()) {
                    process->start(process->program() + " " + process->arguments().join(' '),
                                   QIODevice::ReadOnly);
                } else {
                    process->start(QIODevice::ReadOnly);
                }
                std::cout << "start: " << process->program().toStdString() << " "
                          << process->arguments().join(' ').toStdString()
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

void SupervisordPrivate::killAllProcesses() {}

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
