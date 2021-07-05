//
// Created by jiang on 2021/5/31.
//

#ifndef GARBAGE_UI_SUPERVISORD_HPP
#define GARBAGE_UI_SUPERVISORD_HPP

#include <QObject>
#include <QProcess>

class SupervisordPrivate;

class Supervisord : public QObject
{
Q_OBJECT
public:
    Supervisord(int argc, char *argv[]);
    ~Supervisord() override;
    int exec();
    void stop();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onErrorOccurred(QProcess::ProcessError error);
private:
    QScopedPointer<SupervisordPrivate> d_ptr;
    Q_DISABLE_COPY(Supervisord)
    Q_DECLARE_PRIVATE(Supervisord)
};



#endif //GARBAGE_UI_SUPERVISORD_HPP
