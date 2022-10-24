#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num)
{
    cout << "smash: got ctrl-Z" << endl;
    Command * fg = SmallShell::getInstance().fg;
    if(fg== nullptr)
    {
        return;
    }
    kill(fg->pid, SIGSTOP);
    cout << "smash: process " << fg->pid << " was stopped" << endl;
    SmallShell::getInstance().jobsList->addJob(fg,fg->pid,true);
}

void ctrlCHandler(int sig_num)
{
    Command * fg = SmallShell::getInstance().fg;
    cout << "smash: got ctrl-C" << endl;
    if(fg== nullptr)
    {
        return;
    }
    kill(fg->pid, SIGKILL);
    cout << "smash: process " << fg->pid << " was killed" << endl;
}

void alarmHandler(int sig_num)
{
}

