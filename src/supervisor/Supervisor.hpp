//
// Created by jiang on 2021/5/31.
//

#ifndef GARBAGE_UI_SUPERVISOR_HPP
#define GARBAGE_UI_SUPERVISOR_HPP

class SupervisorPrivate;

class Supervisor
{
public:
    Supervisor(int argc, char **argv);
    int exec();
    ~Supervisor();
private:
    SupervisorPrivate *d;
};


#endif //GARBAGE_UI_SUPERVISOR_HPP
