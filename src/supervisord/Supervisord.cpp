//
// Created by jiang on 2021/5/31.
//

#include "Supervisord.hpp"
#include <boost/interprocess/shared_memory_object.hpp>
#include <QCoreApplication>
#include <iostream>
#include <fstream>
#include <QUdpSocket>
#include <QProcess>
#include <QTimer>
#include <json.hpp>
#include <map>
#include <unistd.h>

using json = nlohmann::json;

class SupervisordPrivate
{
public:
    Q_DECLARE_PUBLIC(Supervisord)
    Supervisord *q_ptr;
    int mArgc;
    char **mArgv;
    QUdpSocket *mSocket;
    json mConf;
    std::list<std::string> mCurrentModes;
    std::map<std::string, std::list<QProcess *>> mProcessList;

    explicit SupervisordPrivate(Supervisord *p);
    void onReadyRead();
    void createProcesses();
    void startProcesses(const std::string &mode);
    void stopProcesses(const std::string &mode);
    void stopAllProcesses();
    void prepare();
};

SupervisordPrivate::SupervisordPrivate(Supervisord *p) :
        q_ptr(p)
{

}


void SupervisordPrivate::onReadyRead()
{
    QByteArray array = mSocket->readAll();

}

void SupervisordPrivate::createProcesses()
{
    Q_Q(Supervisord);
    for (const auto &item : mConf["start"]) {
        mCurrentModes.push_back(item);
    }
    for (const auto &item : mConf["modes"]) {
        std::string name = item["name"];
        mProcessList[name] = std::list<QProcess *>();
        for (const auto &proc : item["executables"]) {
            auto process = new QProcess(q);
            mProcessList[name].emplace_back(process);
            QString command;
            if (proc.contains("executor")) {
                command += QString::fromStdString(proc["executor"]) + " ";
            }
            QString path = QString::fromStdString(proc["path"]);
            if (!path.endsWith('/')) {
                path += "/";
            }
            QString procName = QString::fromStdString(proc["name"]);
            process->setProperty("name", procName);
            command += path + procName;
            process->setProgram(command);
            if (proc.contains("restart")) {
                process->setProperty("restart", proc["restart"].get<bool>());
            } else {
                process->setProperty("restart", false);
            }
            std::cout << "create: " << command.toStdString() << std::endl;
            QObject::connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>
            (&QProcess::finished), q, &Supervisord::onProcessFinished);
            QObject::connect(process, static_cast<void (QProcess::*)(QProcess::ProcessError)>
            (&QProcess::errorOccurred), q, &Supervisord::onErrorOccurred);
        }
    }
}


void SupervisordPrivate::startProcesses(const std::string &mode)
{
    for (auto process : mProcessList[mode]) {
        process->start(QIODevice::ReadOnly);
        std::cout << "start: " << process->program().toStdString() <<
                  " pid:" << process->pid() << std::endl;
    }
}

void SupervisordPrivate::stopProcesses(const std::string &mode)
{
    for (auto process : mProcessList[mode]) {
        if (process->isOpen()) {
            process->close();
        }
    }
}

void SupervisordPrivate::stopAllProcesses()
{
    for (auto &list : mProcessList) {
        for (auto process : list.second) {
            if (process->isOpen()) {
                process->close();
            }
        }
    }
}

void SupervisordPrivate::prepare()
{
    for (const auto &mode : mCurrentModes) {
        startProcesses(mode);
    }
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
    std::ifstream is("/data/caller_table.json");
    if (!is.is_open()) {
        std::cout << "打开配置文件失败 /data/caller_table.json" << std::endl;
        exit(-1);
    }
    is >> d->mConf;
    d->mSocket = new QUdpSocket(this);
    if (!d->mSocket->bind(QHostAddress::AnyIPv4, 33496)) {
        if (d->mSocket->error() == QUdpSocket::AddressInUseError) {
            std::cout << "已经有相同的实例在运行或者端口33496别占用" << std::endl;
            QCoreApplication::quit();
        }
    }
    d->createProcesses();
    connect(d->mSocket, &QUdpSocket::readyRead, [this] {
        d_ptr->onReadyRead();
    });
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
    QTimer::singleShot(100, [this] {
        d_ptr->prepare();
    });
    return QCoreApplication::exec();
}

void Supervisord::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto process = qobject_cast<QProcess *>(sender());
    std::cout << "process " << process->program().toStdString() << " "
              << "finished: " << process->errorString().toStdString() << std::endl;
    if (process->property("restart").toBool()) {
        QTimer::singleShot(1000, [process] {
            process->start();
            std::cout << "restart " << process->program().toStdString() << " pid:"
                      << process->pid() << std::endl;
        });
    }
}

void Supervisord::onErrorOccurred(QProcess::ProcessError error)
{
    auto process = qobject_cast<QProcess *>(sender());
    std::cout << "process " << process->program().toStdString() << " "
              << "error: " << process->errorString().toStdString() << std::endl;
}

int main(int argc, char *argv[])
{
    std::cout << "current pid: " << getpid() << std::endl;
    Supervisord daemon(argc, argv);
    return daemon.exec();
}