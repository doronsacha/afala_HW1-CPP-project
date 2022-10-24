#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#include <string>
#include <vector>

#include <map>


 using namespace std;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
 public:
    pid_t pid;
    int job_id;
    bool is_background;
    bool is_stopped;
    time_t starting_time;
    char** args;
    int num_arg;
    string cmd_line;
  Command(const char* cmd_line);
  virtual ~Command()=default;
  virtual void execute() = 0;
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
};

class ChpromptCommand : public BuiltInCommand {
public:
    ChpromptCommand(const char* cmd_line);
    virtual ~ChpromptCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
  ChangeDirCommand(const char* cmd_line);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
public:
  QuitCommand(const char* cmd_line);
  virtual ~QuitCommand() {}
  void execute() override;
};



class JobsList {
 public:
  class JobEntry
          {
  public:
      Command* ptr_cmd;
      bool is_stopped;
  };

 public:
    map<int, JobEntry>* job_list;
  JobsList();
  ~JobsList()=default;
  void addJob(Command* cmd,pid_t pid,bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
};

class JobsCommand : public BuiltInCommand {
 public:
  JobsCommand(const char* cmd_line);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 public:
  KillCommand(const char* cmd_line);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 public:
  ForegroundCommand(const char* cmd_line);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 public:
  BackgroundCommand(const char* cmd_line);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TailCommand : public BuiltInCommand {
 public:
  TailCommand(const char* cmd_line);
  virtual ~TailCommand() {}
  void execute() override;
};

class TouchCommand : public BuiltInCommand {
 public:
  TouchCommand(const char* cmd_line);
  virtual ~TouchCommand() {}
  void execute() override;
};


class SmallShell {
 private:
  SmallShell();
 public:
    pid_t pid;
    string prompt;
    string curr_dir;
    string prev_dir;
    JobsList* jobsList;
    Command* fg;
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete;
  void operator=(SmallShell const&)  = delete;
  static SmallShell& getInstance()
    {
    static SmallShell instance;
    return instance;
  }
  ~SmallShell()=default;
  void executeCommand(const char* cmd_line);
};

#endif
