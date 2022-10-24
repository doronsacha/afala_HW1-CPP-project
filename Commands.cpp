#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    if (idx == string::npos) {
        return;
    }
    if (cmd_line[idx] != '&') {
        return;
    }
    cmd_line[idx] = ' ';
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}



SmallShell::SmallShell()
{
    this->prompt="smash> ";
    this->jobsList= new JobsList;
    this->curr_dir= "";
    this->prev_dir= "";
    this->pid=getpid();
}

JobsList::JobsList()
{
    this->job_list= new map<int, JobEntry>;
}

int get_next_free_id(JobsList* job_lst)
{
    if(job_lst->job_list->empty())
    {
        return 1;
    }
    else
    {
        return prev(job_lst->job_list->end())->first+1;
    }
}

void JobsList::addJob(Command *cmd, pid_t pid,bool isStopped)
{
    JobEntry new_job;
    new_job.is_stopped=isStopped;
    new_job.ptr_cmd=cmd;
    new_job.ptr_cmd->is_stopped = isStopped;
    new_job.ptr_cmd->pid=pid;
    if(cmd->starting_time==-1)
    {
        new_job.ptr_cmd->starting_time = time(0);
    }
    if(new_job.ptr_cmd->job_id==-1)
    {
        new_job.ptr_cmd->job_id=get_next_free_id(this);
    }
    job_list->insert(pair<int, JobEntry>(new_job.ptr_cmd->job_id, new_job));

}

void JobsList::removeJobById(int jobId)
{


    if(this->getJobById(jobId)== nullptr)
    {
        return;
    }
    this->job_list->erase(jobId);
}

JobsList::JobEntry* JobsList::getJobById(int jobId)
{
    if(job_list->find(jobId) != job_list->end() )
    {
        return &(job_list->find(jobId)->second);
    }
    else
    {
        return nullptr;
    }
}

JobsList::JobEntry* JobsList::getLastJob(int *lastJobId)
{
    if(job_list->empty())
    {
        *lastJobId=-1;
        return nullptr;
    }
    *lastJobId=(prev(this->job_list->end())->first);
    return &(prev(this->job_list->end())->second);
}


JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId)
{
    JobEntry* max_last= nullptr;
    map<int,JobEntry>::iterator it;
    for (it = job_list->begin(); it != job_list->end(); it++)
    {
        if(it->second.is_stopped==true)
        {
            max_last=&(it->second);
            *jobId=it->first;
        }
    }
    return max_last;
}

void JobsList::removeFinishedJobs()
{

    map<int,JobEntry>::iterator it, it1;
    it = job_list->begin();
    pid_t pid;
    while(it != job_list->end())
    {
        it1=it;
        it1++;
        pid= waitpid(it->second.ptr_cmd->pid, nullptr,WNOHANG);
        if(pid==-1)
        {
            perror("smash error: wait failed");
            return;
        }
        if(pid>0)
        {
            this->removeJobById(it->first);
        }
        it=it1;
    }
}

void JobsList::killAllJobs()
{
    map<int,JobEntry>::iterator it;
    for (it = job_list->begin(); it != job_list->end(); it++)
    {
        int err=kill(it->second.ptr_cmd->pid, SIGKILL);
        if(err==-1)
        {
            perror("smash error: kill failed");
            return;
        }
    }
    return ;
}


void JobsList::printJobsList()
{

   if(getpid() == SmallShell::getInstance().pid)
    {
        SmallShell::getInstance().jobsList->removeFinishedJobs();
    }
    if(job_list->empty())
    {
        return;
    }
    map<int,JobEntry>::iterator it;
    for (it = job_list->begin(); it != job_list->end(); it++)
    {
        if(it->second.is_stopped ==false)
        {
            cout << "[" << it->first << "] " << it->second.ptr_cmd->cmd_line << " : " <<it->second.ptr_cmd->pid<<" "<< (difftime(time(0), it->second.ptr_cmd->starting_time)) << " secs"<<endl;
        }
        else
        {
            cout << "[" << it->first << "] " << it->second.ptr_cmd->cmd_line << " : " <<it->second.ptr_cmd->pid<<" "<< (difftime(time(0), it->second.ptr_cmd->starting_time)) << " secs (stopped)"<<endl;
        }
    }
    SmallShell::getInstance().fg= nullptr;
    return;
}


Command::Command(const char* cmd_line)
{
    args= (char**)malloc(sizeof(char*)*COMMAND_MAX_ARGS);
    this->cmd_line=cmd_line;

    is_background=false;
    if(_isBackgroundComamnd(cmd_line)==true)
    {
        is_background=true;
    }
    this->pid=-1;
    this->job_id=-1;
    this->is_stopped=false;
    this->num_arg= _parseCommandLine(_trim(cmd_line).c_str(),this->args);
    this->starting_time= -1;
}
BuiltInCommand::BuiltInCommand(const char* cmd_line): Command(cmd_line)
{
}

ChpromptCommand::ChpromptCommand(const char* cmd_line): BuiltInCommand::BuiltInCommand(cmd_line){}

void ChpromptCommand::execute()
{
    if(this->num_arg==1)
    {
        SmallShell::getInstance().prompt="smash> ";
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    else
    {
        SmallShell::getInstance().prompt=string(args[1])+"> ";
        SmallShell::getInstance().fg= nullptr;
        return;
    }
}
ShowPidCommand::ShowPidCommand(const char* cmd_line): BuiltInCommand(cmd_line){}

void ShowPidCommand::execute()
{
    pid_t pid=getpid();
    if(pid==-1)
    {
        perror("smash error: getpid failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    cout << "smash pid is "<< pid<<endl;
    SmallShell::getInstance().fg= nullptr;
    return;
}


GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line){}

void GetCurrDirCommand::execute()
{
    char buff[4096];
    char* curr_dir_to_print=getcwd(buff,4096);
    if(curr_dir_to_print== nullptr)
    {
        perror("smash error: getcwd failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    cout<< buff<<endl;
    SmallShell::getInstance().fg= nullptr;
    return;
}
std::string get_parent(char* directory)
{
    int i = strlen(directory)-1;
    while(directory[i] !='/')
    {
        i--;
    }
    std::string str = std::string("");
    int j=0;
    while( j<i )
    {
        str+=directory[j];
        j++;
    }
    str+='\0';
    return str;
}
ChangeDirCommand::ChangeDirCommand(const char *cmd_line): BuiltInCommand(cmd_line){}

void ChangeDirCommand::execute()
{
    if(num_arg==1)
    {
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if(num_arg>2)
    {
        cerr<<"smash error: cd: too many arguments"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if(SmallShell::getInstance().curr_dir=="")
    {
        char buff[4096];
        SmallShell::getInstance().curr_dir= getcwd(buff,4096);
    }

    if(strcmp(args[1],"-")==0)
    {
        string prev=SmallShell::getInstance().prev_dir;
        if(prev== "")
        {
            cerr<<"smash error: cd: OLDPWD not set"<<endl;
            SmallShell::getInstance().fg= nullptr;
            return;
        }
        else
        {
            int err= chdir(prev.c_str());
            if(err==-1)
            {
                perror("smash error: chdir failed");
                SmallShell::getInstance().fg= nullptr;
                return;
            }
            SmallShell::getInstance().prev_dir=SmallShell::getInstance().curr_dir;
            SmallShell::getInstance().curr_dir=prev;
        }
    }
    else
    {
        char buff[4096];
        int err= chdir((args[1]));
        if(err==-1)
        {
            perror("smash error: chdir failed");
            return;
        }
        SmallShell::getInstance().prev_dir=SmallShell::getInstance().curr_dir;
        SmallShell::getInstance().curr_dir= getcwd(buff,4096);
    }
    SmallShell::getInstance().fg= nullptr;
}

JobsCommand::JobsCommand(const char *cmd_line): BuiltInCommand(cmd_line){}

void JobsCommand::execute()
{
    SmallShell::getInstance().jobsList->printJobsList();
    SmallShell::getInstance().fg= nullptr;
}


bool isNumber(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

bool validArguments(char** arguments,int num_of_arg)
{
    if (num_of_arg!=3)
    {
        return false;
    }
    if(arguments[1][0]!= '-'){
        return false;
    }
    if(arguments[1][0]== '-')
    {
        int length = strlen(arguments[1]);
        string s=arguments[1];
        std::string newStr = s.substr(1,length);
        if (!(isNumber(newStr)))
        {
            return false;
        }
        if(stoi(newStr)>31)
        {
            return false;
        }
    }
    string b= arguments[2];
    if (!(isNumber(b)))
    {
        return false;
    }
    return true;
}
KillCommand::KillCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void KillCommand::execute()
{
    if(getpid() == SmallShell::getInstance().pid)
    {
        SmallShell::getInstance().jobsList->removeFinishedJobs();
    }
    if (num_arg==3 && args[2][0]=='-' && isNumber(string(args[2]).substr(1,strlen(args[2]))) ){
        cerr<<"smash error: kill: job-id "<<stoi(args[2])<< " does not exist"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if(validArguments(args,num_arg)==false)
    {
        cerr<<"smash error: kill: invalid arguments"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    JobsList::JobEntry* a= SmallShell::getInstance().jobsList->getJobById(stoi(args[2]));
    if(a== nullptr)
    {
        cerr<<"smash error: kill: job-id "<<args[2]<< " does not exist"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    int errr= kill(a->ptr_cmd->pid,abs(atoi(args[1])));
    if(errr==-1)
    {
        perror("smash error: kill failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    cout<<"signal number "<<abs(atoi(args[1]))<<" was sent to pid "<<a->ptr_cmd->pid<<endl;
}

ForegroundCommand::ForegroundCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void ForegroundCommand::execute()
{
    if(getpid() == SmallShell::getInstance().pid)
    {
        SmallShell::getInstance().jobsList->removeFinishedJobs();
    }
    JobsList* a= SmallShell::getInstance().jobsList;
    if(a->job_list->empty() && num_arg==1)
    {
        cerr<<"smash error: fg: jobs list is empty"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if (num_arg==2&& args[1][0]=='-' && isNumber(string(args[1]).substr(1,strlen(args[1]))) ){
        cerr<<"smash error: fg: job-id "<<stoi(args[1])<< " does not exist"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if(num_arg==2 && (isNumber(args[1])) &&  (a->getJobById(stoi(args[1])) == nullptr))
    {
        cerr<<"smash error: fg: job-id "<<stoi(args[1])<< " does not exist"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    if(num_arg>2 || (num_arg==2 && !isNumber(args[1])))
    {
        cerr<<"smash error: fg: invalid arguments"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    int* c= (int*)malloc(sizeof (int));
    int job_i=a->getLastJob(c)->ptr_cmd->job_id;
    if(num_arg==2)
    {
        job_i=stoi(args[1]);
    }
    SmallShell::getInstance().fg=a->getJobById(job_i)->ptr_cmd;
    cout<<a->getJobById(job_i)->ptr_cmd->cmd_line<< " : "<<a->getJobById(job_i)->ptr_cmd->pid<<endl;
    int err=kill(SmallShell::getInstance().fg->pid,SIGCONT);
    if(err==-1)
    {
        perror("smash error: kill failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    a->removeJobById(job_i);
    err= waitpid(SmallShell::getInstance().fg->pid, nullptr,WUNTRACED);
    if(err==-1)
    {
        perror("smash error: wait failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    SmallShell::getInstance().fg->is_background=false;
    SmallShell::getInstance().fg->is_stopped=false;
    SmallShell::getInstance().fg= nullptr;

}

QuitCommand::QuitCommand(const char* cmd_line): BuiltInCommand(cmd_line){}

void QuitCommand::execute()
{
    if(getpid() == SmallShell::getInstance().pid){
        SmallShell::getInstance().jobsList->removeFinishedJobs();
    }
    SmallShell& a=SmallShell::getInstance();
    if(num_arg>1 && (string(args[1]))=="kill")
    {
        cout<<"smash: sending SIGKILL signal to "<< a.jobsList->job_list->size()<<" jobs:"<<endl;
        map<int,JobsList::JobEntry>::iterator it;
        for (it = a.jobsList->job_list->begin(); it != a.jobsList->job_list->end(); it++)
        {
            cout << it->second.ptr_cmd->pid << ": " << it->second.ptr_cmd->cmd_line << endl;
        }
        a.jobsList->killAllJobs();
    }
    SmallShell::getInstance().fg= nullptr;
    exit(0);
}

Command *SmallShell::CreateCommand(const char *cmd_line)
{
    string cmd_s = _trim(string(cmd_line));
    /*if(cmd_s.empty())
    {
        return nullptr;
    }*/
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if (cmd_s.find(">") != string::npos)
    {
        return new RedirectionCommand(cmd_line);
    }
    else if (cmd_s.find("|") != string::npos)
    {
        return new PipeCommand(cmd_line);
    }
    else if (firstWord.compare("chprompt") == 0 )
    {
        return new ChpromptCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0  )
    {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0 )
    {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0 )
    {
        return new ChangeDirCommand(cmd_line);
    }
    else if (firstWord.compare("jobs") == 0 )
    {
        return new JobsCommand(cmd_line);
    }
    else if (firstWord.compare("fg") == 0 )
    {
        return new ForegroundCommand(cmd_line);
    }
    else if (firstWord.compare("bg") == 0 )
    {
        return new BackgroundCommand(cmd_line);
    }
    else if (firstWord.compare("quit") == 0 )
    {
        return new QuitCommand(cmd_line);
    }
    else if (firstWord.compare("kill") == 0 )
    {
        return new KillCommand(cmd_line);
    }
    else if(firstWord.compare("tail") == 0 ){
        return new TailCommand(cmd_line);
    }
    else if(firstWord.compare("touch") == 0 ){
        return new TouchCommand(cmd_line);
    }
    return new ExternalCommand(cmd_line);
}

bool back_valid_arg(char** arguments, int num_arg)
{
    if(num_arg>2)
    {
        return false;
    }
    if(num_arg==2)
    {
        if(!isNumber(arguments[1]))
        {
            return false;
        }
    }
    return true;
}

BackgroundCommand::BackgroundCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}


void BackgroundCommand::execute()
{
    SmallShell& smash= SmallShell::getInstance();
    if(num_arg==2 && smash.jobsList->getJobById(stoi(args[1]))==nullptr)
    {
        std::cerr <<"smash error: bg: job-id "<< args[1] << " does not exist"<< endl;
        SmallShell::getInstance().fg= nullptr;
        return ;
    }
    if(back_valid_arg(args,num_arg)==false)
    {
        std::cerr<< "smash error: bg: invalid arguments"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return ;
    }


    if(num_arg==2 && smash.jobsList->getJobById(stoi(args[1]))!=nullptr  && smash.jobsList->getJobById(stoi(args[1]))->is_stopped==false)
    {
        std::cerr<< "smash error: bg: job-id "<<stoi(args[1]) <<" is already running in the background"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return ;
    }
    int* a = (int*) malloc(sizeof (int));
    if(smash.jobsList->getLastStoppedJob(a)==nullptr)
    {
        std::cerr<<"smash error: bg: there is no stopped jobs to resume"<<endl;
        SmallShell::getInstance().fg= nullptr;
        return ;
    }
    JobsList::JobEntry* job_to_back;
    if(num_arg==1)
    {
        job_to_back=smash.jobsList->getLastStoppedJob(a);
    }
    else
    {
        job_to_back = smash.jobsList->getJobById(stoi(args[1]));
    }
    pid_t pid= job_to_back->ptr_cmd->pid;
    cout<<job_to_back->ptr_cmd->cmd_line<<" : "<<pid<<endl;
    job_to_back->is_stopped=false;
    int err=kill(pid,SIGCONT);
    if(err==-1)
    {
        perror("smash error: kill failed");
        SmallShell::getInstance().fg= nullptr;
        return;
    }
    job_to_back->ptr_cmd->is_background=true;
    time(&job_to_back->ptr_cmd->starting_time);
    SmallShell::getInstance().fg= nullptr;
    return ;
}

void SmallShell::executeCommand(const char *cmd_line)
{
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
}



ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute()
{
    pid_t pid = fork();
    if (pid == 0)
    {
        setpgrp();
        std::string cmd = this->cmd_line;
        _removeBackgroundSign((char *)cmd.c_str());
        char* first=(char *)"/bin/bash";
        char* sec=(char *)"-c";
        char * third=(char *)cmd.c_str();
        char* last= nullptr;
        char *args[] = {first,sec,third,last};
        execv(args[0], args);
    }
    else if (pid > 0)
    {
        this->pid = pid;
        if (this->is_background == true)
        {

            SmallShell::getInstance().jobsList->addJob(this, pid, false);
        }
        else
        {
            SmallShell::getInstance().fg = this;
            waitpid(pid, nullptr, WUNTRACED);
            SmallShell::getInstance().fg = nullptr;
        }
    }
    else
    {
        perror("smash error: fork failed");
        return;
    }
}



bool validTailArguments(char** arguments,int num_arg){
    if (num_arg<2 || num_arg>3)
    {
        return false;
    }
    if(arguments[1][0]!= '-' && num_arg == 3)
    {
        return false;
    }
    if(arguments[1][0]== '-')
    {
        int length = strlen(arguments[1]);
        std::string newStr = string(arguments[1]).substr(1,length-1);
        if (!(isNumber(newStr)))
        {
            return false;
        }
    }
    return true;
}

int get_number_of_lines(int fd)
{
    int lines_counter=1;
    char ch;
    int read_byte = read(fd, &ch, 1);
    if( read_byte == -1)
    {
        perror("smash error: read failed");
        return -1;
    }
    while(read_byte)
    {
        if(read_byte == -1)
        {
            perror("smash error: read failed");
            int err=close(fd);
            if(err==-1)
            {
                perror("smash error: close failed");
                return -1;
            }
        }
        else
        {
            if(ch == '\n')
            {
                lines_counter++;
            }
        }
        read_byte = read(fd, &ch, 1);
    }
    return lines_counter;
}

int go_to_line_number(int fd, int line_number)
{
    int lines_counter=1;
    char ch;
    int read_byte = read(fd, &ch, 1);
    if( read_byte == -1)
    {
        perror("smash error: read failed");
        return -1;
    }
    while(true)
    {
        if(read_byte == -1)
        {
            perror("smash error: read failed");
            int err=close(fd);
            if(err==-1)
            {
                perror("smash error: close failed");
                return -1;
            }
            return -1;
        }
        else
        {
            if(ch == '\n')
            {
                lines_counter++;
            }
        }
        if(lines_counter==line_number)
        {
            break;
        }
        read_byte = read(fd, &ch, 1);
    }
    return 0;
}

TailCommand::TailCommand(const char *cmd_line) :BuiltInCommand(cmd_line) {}

void TailCommand::execute(){
    int n;
    if (num_arg == 1)
    {
        std::cerr<<("smash error: tail: invalid arguments") <<endl;
        return;
    }
    std::string filename;
    if (num_arg == 2)
    {
        n = 10;
        filename = std::string(args[1]);
    }
    else
    {
        if (!validTailArguments(args, num_arg))
        {
            std::cerr<<("smash error: tail: invalid arguments") <<endl;
            return;
        }
        n = atoi(args[1]) * -1;
        filename = std::string(args[2]);
        if(n==0)
        {
            return;
        }
    }
    filename = _trim(filename);
    char ch;
    int read_byte;
    int fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1)
    {
        perror("smash error: open failed");
        return;
    }
    int lines_number= get_number_of_lines(fd);
    if(lines_number==-1)
    {
        return;
    }
    int line_to_go= lines_number-n >0 ? lines_number-n : 1;
    close(fd);
    fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1)
    {
        perror("smash error: open failed");
        return;
    }
    if(line_to_go!=1)
    {
        if(go_to_line_number(fd,line_to_go+1)==-1)
        {
            return;
        }
    }
    read_byte = read(fd, &ch, 1);
    if( read_byte == -1)
    {
        perror("smash error: read failed");
    }
    while(read_byte)
    {
        if(read_byte == -1){
            perror("smash error: read failed");
            close(fd);
            return;
        }
        else
        {
            if(write(1, &ch, 1) == -1){
                perror("smash error: write failed");
                close(fd);
                return;
            }
        }
        read_byte = read(fd, &ch, 1);
    }
    close(fd);
}

TouchCommand::TouchCommand(const char* cmd_line):BuiltInCommand(cmd_line){}

void TouchCommand::execute()
{
    if(num_arg!=3)
    {
        std::cerr<<"smash error: touch: invalid arguments"<<endl;
        return ;
    }
    string time_argunt= _trim(args[2]).c_str();
    int time_array[6];
    for(int i=0;i<6;i++)
    {
        int index= time_argunt.find(":");
        time_array[i]=stoi(time_argunt.substr(0,index));
        time_argunt=time_argunt.substr(index+1);
    }
    tm* time= (tm*)malloc(sizeof(*time));
    time->tm_hour=time_array[2];
    time->tm_sec=time_array[0];
    time->tm_mday=time_array[3];
    time->tm_min=time_array[1];
    time->tm_year=time_array[5]-1900;
    time->tm_mon=time_array[4]-1;
    time->tm_wday=0;
    time->tm_yday=0;
    time->tm_gmtoff=0;

    time_t tm= mktime(time);
    utimbuf ti_bu;
    ti_bu.actime=tm;
    ti_bu.modtime=tm;
    int ret = utime(args[1],&ti_bu);
    if(ret==-1)
    {
        perror("smash error: utime failed");
        return ;
    }
    return ;
}




RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line)
{
}

void RedirectionCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    std::string cmd;
    std::string file;
    std::string fullCMD = cmd_line;
    int flg;
    if (string::npos != fullCMD.find(">>"))
    {
        flg = O_APPEND;
        std::size_t idxe = fullCMD.find(">>");
        if (idxe != string::npos)
        {
            cmd = fullCMD.substr(0, idxe);
            file = fullCMD.substr(idxe + 2);
        }

        cmd = _trim(cmd);
        file = _trim(file);
    }
    else if (fullCMD.find(">") != string::npos)
    {
        flg = O_TRUNC;
        std::size_t idxe = fullCMD.find(">");
        if (idxe != string::npos)
        {
            cmd = fullCMD.substr(0, idxe);
            file = fullCMD.substr(idxe + 1);
        }

        cmd = _trim(cmd);
        file = _trim(file);
    }
    int stdout_fdeee;
    stdout_fdeee = dup(1) ;
    close(1);
    int myFile = (open((char *)file.c_str(), O_WRONLY | O_CREAT | flg, 0655));
    if ((myFile == -1)) {
        perror("smash error: open failed");
    }
    else
    {
        smash.executeCommand(cmd.c_str());
        close(myFile);
    }
    dup2(stdout_fdeee, 1);
    close(stdout_fdeee);
    return;
}


PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line)
{
}

void PipeCommand::execute()
{
    SmallShell& smash =SmallShell::getInstance();
    int pipe_out_ch;
    std::string cmd1;
    std::string cmd2;
    std::string fullCMD = this->cmd_line;
    if (string::npos != fullCMD.find("|&"))
    {
        pipe_out_ch = 2;
        std::size_t idxe = fullCMD.find("|&");
        if (idxe != string::npos)
        {
            cmd1 = fullCMD.substr(0, idxe);
            cmd2 = fullCMD.substr(idxe + 2);
        }

        cmd1 = _trim(cmd1);
        cmd2 = _trim(cmd2);
    }
    else if (fullCMD.find("|") != string::npos)
    {

        pipe_out_ch = 1;
        std::size_t idxe = fullCMD.find("|");
        if (idxe != string::npos)
        {
            cmd1 = fullCMD.substr(0, idxe);
            cmd2 = fullCMD.substr(idxe + 1);
        }

        cmd1 = _trim(cmd1);
        cmd2 = _trim(cmd2);
    }
    int in = 0;
    int myPipe_fd[2];
    pipe(myPipe_fd);
    pid_t CMDR1_pid, CMDR2_pid;
    CMDR1_pid = fork();
    Command *CMDR1_ptr = smash.CreateCommand(cmd1.c_str());
    CMDR1_ptr->pid = CMDR1_pid;
    if (CMDR1_pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }
    else if (CMDR1_pid == 0)
    {

        setpgrp();
        dup2(myPipe_fd[1], pipe_out_ch);
        close(myPipe_fd[0]);
        close(myPipe_fd[1]);
        CMDR1_ptr->execute();
        exit(0);
    }
    else
    {
        CMDR2_pid = fork();
        Command *CMDR2_ptr = smash.CreateCommand(cmd2.c_str());
        CMDR2_ptr->pid = CMDR2_pid;

        if (CMDR2_pid == -1)
        {
            perror("smash error: fork failed");
            return;
        }
        else if (CMDR2_pid == 0)
        {
            setpgrp();
            dup2(myPipe_fd[0], in);
            close(myPipe_fd[0]);
            close(myPipe_fd[1]);
            CMDR2_ptr->execute();
            exit(0);
        }
        close(myPipe_fd[0]);
        close(myPipe_fd[1]);
        smash.fg = CMDR1_ptr;
        waitpid(CMDR1_pid, nullptr, WUNTRACED);
        smash.fg = CMDR2_ptr;
        waitpid(CMDR2_pid, nullptr, WUNTRACED);
    }
}
