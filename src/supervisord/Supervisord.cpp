//
// Created by jiang on 2021/5/31.
//

#include "Supervisord.hpp"
#include <boost/process.hpp>
#include <QCoreApplication>
#include <iostream>
#include <fstream>
#include <QUdpSocket>
#include <QHostAddress>
#include <QProcess>
#include <QTimer>
#include <json.hpp>
#include <map>
#include <unistd.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

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
    QList<QString> mAutoStart;
    QList<QString> mCurrentModes;
    QMap<QString, QList<QProcess *>> mProcessList;

    explicit SupervisordPrivate(Supervisord *p);
    void onReadyRead();
    void createProcesses();
    void startProcesses(const QString &mode);
    void stopProcesses(const QString &mode);
    void stopAllProcesses();
    void prepare();
};

SupervisordPrivate::SupervisordPrivate(Supervisord *p) :
        q_ptr(p)
{

}


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
            if (!object.contains("mode_name") ||
                !object.contains("enable")) {
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
        for (const auto &key : mProcessList.keys()) {
            total.append(key);
        }
        QJsonArray enabled;
        for (const auto &key : mCurrentModes) {
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
    for (const auto &item : mConf["start"]) {
        mAutoStart.append(QString::fromStdString(item));
    }
    for (const auto &item : mConf["modes"]) {
        QString name = QString::fromStdString(item["name"]);
        mProcessList[name] = QList<QProcess *>();
        for (const auto &object : item["executables"]) {
            auto process = new QProcess(q);
            mProcessList[name].append(process);
            QString command;
            if (object.contains("executor")) {
                command += QString::fromStdString(object["executor"]) + " ";
            }
            QString path = QString::fromStdString(object["path"]);
            if (!path.endsWith('/')) {
                path += "/";
            }
            QString procName = QString::fromStdString(object["name"]);
            process->setProperty("name", procName);
            command += path + procName;
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
            std::cout << "create: " << command.toStdString() << std::endl;
            QObject::connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>
            (&QProcess::finished), q, &Supervisord::onProcessFinished);
            QObject::connect(process, static_cast<void (QProcess::*)(QProcess::ProcessError)>
            (&QProcess::errorOccurred), q, &Supervisord::onErrorOccurred);
        }
    }
}


void SupervisordPrivate::startProcesses(const QString &mode)
{
    if (mCurrentModes.contains(mode)) {
        return;
    } else {
        mCurrentModes.append(mode);
    }
    for (auto process : mProcessList[mode]) {
        if (process->property("detach").toBool()) {
            QProcess::startDetached(process->program());
        } else {
            process->setProperty("manually_close", false);
            process->start(QIODevice::ReadOnly);
            std::cout << "start: " << process->program().toStdString() <<
                      " pid:" << process->pid() << std::endl;
        }
    }
}

void SupervisordPrivate::stopProcesses(const QString &mode)
{
    if (!mCurrentModes.contains(mode)) {
        return;
    } else {
        mCurrentModes.removeOne(mode);
    }
    for (auto process : mProcessList[mode]) {
        if (process->isOpen()) {
            process->setProperty("manually_close", true);
            process->close();
        }
    }
}

void SupervisordPrivate::stopAllProcesses()
{
    for (auto &list : mProcessList) {
        for (auto process : list) {
            if (process->isOpen()) {
                process->setProperty("manually_close", true);
                process->close();
            }
        }
    }
    mCurrentModes.clear();
}

void SupervisordPrivate::prepare()
{
    Q_Q(Supervisord);
    for (const auto &mode : mAutoStart) {
        startProcesses(mode);
    }
    mSocket = new QUdpSocket(q);
    if (!mSocket->bind(mConf["port"].get<int>())) {
        if (mSocket->error() == QUdpSocket::AddressInUseError) {
            std::cout << "已经有相同的实例在运行或者端口33496别占用" << std::endl;
            QCoreApplication::quit();
        }
    }
    QObject::connect(mSocket, &QUdpSocket::readyRead, [this] {
        onReadyRead();
    });
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
    if (d->mConf.contains("start_ros_core") && d->mConf["start_ros_core"].get<bool>()) {
        system("gnome-terminal -x bash -c 'roscore'&");
    }
    QTimer::singleShot(100, [this] {
        d_ptr->prepare();
    });
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
    if (process->property("restart").toBool() &&
        !process->property("manually_close").toBool()) {
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