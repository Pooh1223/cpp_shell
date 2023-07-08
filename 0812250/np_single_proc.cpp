#include <iostream>
#include <sys/types.h> 
#include <sys/socket.h>  
#include <sys/ioctl.h> 
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <stdlib.h>
#include <stdio.h> 
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <cstring>
#include <string>
#include <vector>
#include <map>
#define N 1003
#define M 1003
#define C 31
using namespace std;

typedef char* cstr;

// init var
struct Str{ // single line of the command
    vector<vector<string> > instr; // line -> a group of one command -> each arg
    bool isNumberPipe; // for |
    bool isNumberPipe2; // for !
    bool beNumberPiped; // for |
    bool pipeOpen;
    int fd[2];
    int nextNline;
};

struct UserDetail{
    string name;
    bool inuse;
    string ipAddr;
    string port;
    int isUserPipe; // whether this command is user pipe ,and pipe to who (in fd num)
    int recvFrom; // this is for receiving user pipe from specific user (in fd num)
    int fd[C][2]; // receive user pipe from others
    bool pipeOpen[C]; // to record whether pipe is open
    int fdNum; // remember real fd num
    map<string,string> env;
}user[C];

Str parsed[C][M];
int fd[C][N][2];
int linecnt[C];
int sockfd;
int clientfd[C];
fd_set readfds;
map<int,int> fdToUser;

// build-in commands
string Getenv(string envVar,int clientid);
void Setenv(string envVar,string newPath,int clientid);
void Who(int clientid);
void Name(string newName,int clientid);
void Yell(string message,int clientid);
void Tell(string receiver,string message,int clientid);

// init
void init();

// tool functions
bool EmptyStr(string s);
int toNumber(char* argv);
string addrToIP(long long addr);
void welcome(int cfd,long long addr,long long port);

// about parsing and executing commands
void SeparateArg(string s,int clientid);
void SeparateCommand(string s,int clientid);
void partialCommand(vector<string> &singleLine,int pipeid,int type,int clientid);
void exeCommand(string command,int clientid);
void exeShell(string line,int clientid);

// handlers
void childHandler(int signo);

int main(int argc,char* argv[]){
    char pa[3] = "% ";
    char buffer[15005];
    socklen_t server_len, client_len; 
    struct sockaddr_in sa; 
    struct sockaddr_in ca; 
    int result; 
    int flag = 1;

    fd_set testfds; 

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0){
        // do nothing?
    }

    bzero(&sa,sizeof(sa));
    bzero(&ca,sizeof(ca));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(toNumber(argv[1])); 
    server_len = sizeof(sa);

    bind(sockfd, (struct sockaddr *)&sa, server_len); 
    listen(sockfd,C); 
    
    FD_ZERO(&readfds); 
    FD_SET(sockfd, &readfds);

    init();
    memset(buffer,'\0',sizeof(buffer));

    do{
        int readlen;
        testfds = readfds;
        printf("server%d waiting\n",sockfd); 

        
        if(result = select(FD_SETSIZE, &testfds,NULL,NULL,NULL) < 0){ 
            printf("select fail!\n");
            continue;
        } 

        for(int i = 0;i < FD_SETSIZE;++i){
            if(FD_ISSET(i,&testfds)){ 
                printf("fd_isset : %d\n\n",i);
                if(i == sockfd){ // another client in
                    int clientFd;
                    client_len = sizeof(ca); 
                    clientFd = accept(sockfd, (struct sockaddr *)&ca, &client_len); 
                    FD_SET(clientFd, &readfds);
                    printf("adding client on fd %d\n\n", clientFd);

                    welcome(clientFd,ntohl(ca.sin_addr.s_addr),ntohs(ca.sin_port));
                    write(clientFd,pa,strlen(pa));
                } else if(fdToUser.find(i) != fdToUser.end()){ // read client input

                    ioctl(i, FIONREAD, &readlen);
                    // printf("readlen : %d\n",readlen);
                    
                    // if(readlen == 0){ 
                    //     close(fd); 
                    //     FD_CLR(fd, &readfds);
                    //     printf("removing client on fd %d\n", fd); 
                    // } else{ 

                    while(strlen(buffer) < readlen){
                        read(i,buffer,sizeof(buffer));
                    }
                     
                    printf("serving client on fd %d\n", i);

                    //int commandLen = strlen(buffer) - 2 < 0 ? 0 : strlen(buffer) - 2;
                    string line = "";
                    for(int j = 0;j < strlen(buffer);++j){
                        if(buffer[j] == '\r' || buffer[j] == '\n') continue;
                        line += buffer[j];
                    } 
                    cout << line << "line" << strlen(buffer) << "\n";

                    if(line == "exit"){
                        //finish = true;

                        string exitMessage = "*** User '" + user[fdToUser[i]].name + "' left. ***\n";

                        close(i);
                        FD_CLR(i, &readfds);

                        user[fdToUser[i]].name = "";
                        user[fdToUser[i]].inuse = false;
                        user[fdToUser[i]].ipAddr = "";
                        user[fdToUser[i]].port = "";
                        user[fdToUser[i]].isUserPipe = -1;
                        for(int j = 1;j < C;++j){

                            if(user[fdToUser[i]].pipeOpen[j] == true){
                                user[fdToUser[i]].pipeOpen[j] = false;
                                close(user[fdToUser[i]].fd[j][0]);
                                close(user[fdToUser[i]].fd[j][1]);
                            }

                            if(user[j].pipeOpen[fdToUser[i]] == true){
                                user[j].pipeOpen[fdToUser[i]] = false;
                                close(user[j].fd[fdToUser[i]][0]);
                                close(user[j].fd[fdToUser[i]][1]);
                            }
                        }
                        user[fdToUser[i]].recvFrom = -1;
                        user[fdToUser[i]].env.clear();
                        user[fdToUser[i]].env["PATH"] = "bin:.";
                        
                        for(int j = 1;j < C;++j){
                            if(user[j].inuse){
                                write(user[j].fdNum,exitMessage.c_str(),exitMessage.length());
                            }
                        }

                        fdToUser.erase(i);
                    } else {
                        exeShell(line,i);
                    }

                    memset(buffer,'\0',sizeof(buffer));
                    //if(line != "exit") write(i,pa,strlen(pa));
                    //} 
                } 
            } 
        } 
    }while(1);

    return 0;
}

// init

void init(){
    for(int i = 0;i < M;++i){
        for(int j = 1;j < C;++j){
            parsed[j][i].isNumberPipe = false;
            parsed[j][i].isNumberPipe2 = false;
            parsed[j][i].beNumberPiped = false;
            parsed[j][i].pipeOpen = false;
        } 
    }

    for(int i = 1;i < C;++i)
        linecnt[i] = 0;

    for(int i = 1;i < C;++i){
        user[i].name = "";
        user[i].inuse = false;
        user[i].ipAddr = "";
        user[i].port = "";
        user[i].isUserPipe = -1;
        for(int j = 1;j < C;++j){
            user[i].pipeOpen[j] = false;
        }
        user[i].recvFrom = -1;
        user[i].env.clear();
        user[i].env["PATH"] = "bin:.";
    }

    fdToUser.clear();
}

// handlers

void childHandler(int signo){
    int st;
    int pid;
    while(pid = waitpid(-1,&st,WNOHANG) > 0){
        //do nothing
    }
}

// tool functions

bool EmptyStr(string s){
    for(int i = 0;i < s.length();++i){
        if(s[i] != ' ') return false;
    }
    return true;
}

int toNumber(char* argv){
    int num = 0;
    for(int i = 0;i < strlen(argv);++i){
        num *= 10;
        num += argv[i] - '0';
    }
    return num;
}

string addrToIP(long long addr){
    string ip = to_string(addr / 16777216) + ".";
    addr %= 16777216;
    ip += to_string(addr / 65536) + ".";
    addr %= 65536;
    ip += to_string(addr / 256) + ".";
    addr %= 256;
    ip += to_string(addr);

    return ip;
}

void welcome(int cfd,long long addr,long long port){

    for(int i = 1;i < C;++i){
        if(!user[i].inuse){
            // assign a user id
            fdToUser[cfd] = i;
            user[fdToUser[cfd]].name = "(no name)";
            user[fdToUser[cfd]].inuse = true;
            user[fdToUser[cfd]].ipAddr = addrToIP(addr);
            user[fdToUser[cfd]].port = to_string(port);
            user[fdToUser[cfd]].fdNum = cfd;
            break;
        }
    }

    cout << user[fdToUser[cfd]].ipAddr << " " << user[fdToUser[cfd]].port << "\n";

    string tmp = user[fdToUser[cfd]].port;

    char welcomeMessage[] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    
    string welcomeUser = "*** User '(no name)' entered from " + user[fdToUser[cfd]].ipAddr + ":" + tmp + ". ***\n";

    write(cfd,welcomeMessage,strlen(welcomeMessage));

    // log
    for(int i = 1;i < C;++i){
        if(user[i].inuse){
            cout << "\nuser " << i << " in use\n";
            write(user[i].fdNum,welcomeUser.c_str(),welcomeUser.length());
        }
    }
    
}

// build-in commands

string Getenv(string envVar,int clientid){
    int clientUserId = fdToUser[clientid];
    string ans = "";

    if(user[clientUserId].env.find(envVar) == user[clientUserId].env.end()){
        // envVar not in env
        cout << "1" << envVar << "1\n";
        return ans;    
    } else {
        ans = user[clientUserId].env[envVar];
        return ans;
    }
}

void Setenv(string envVar,string newPath,int clientid){
    int clientUserId = fdToUser[clientid];
    user[clientUserId].env[envVar] = newPath;
}

void Who(int clientid){
    int clientUserId = fdToUser[clientid];
    string header = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    write(clientid,header.c_str(),header.length());

    string tmp = "";

    for(int i = 1;i < C;++i){
        if(user[i].inuse){
            tmp = to_string(i) + "\t" + user[i].name + "\t" + user[i].ipAddr + ":" + user[i].port;
            if(user[i].fdNum == clientid){
                tmp += "\t<-me";
            }
            tmp += "\n";

            write(clientid,tmp.c_str(),tmp.length());
            tmp = "";
        }
    }
}

void Name(string newName,int clientid){
    int clientUserId = fdToUser[clientid];
    string message = "";

    for(int i = 1;i < C;++i){
        if(user[i].inuse && i != clientUserId){
            if(newName == user[i].name){
                message = "*** User '" + newName + "' already exists. ***\n";
                write(clientid,message.c_str(),message.length());
                return;
            }
        }
    }

    user[clientUserId].name = newName;

    message = "*** User from " + user[clientUserId].ipAddr + ":" + user[clientUserId].port + " is named '" + newName + "'. ***\n";

    //boardcast
    for(int i = 1;i < C;++i){
        if(user[i].inuse){
            write(user[i].fdNum,message.c_str(),message.length());
        }
    }
}

void Yell(string message,int clientid){
    int clientUserId = fdToUser[clientid];
    string tmp = "*** " + user[clientUserId].name + " yelled ***: " + message + "\n";

    for(int i = 1;i < C;++i){
        if(user[i].inuse){
            write(user[i].fdNum,tmp.c_str(),tmp.length());
        }
    }
}

void Tell(string receiver,string message,int clientid){
    int clientUserId = fdToUser[clientid];
    int recv = 0;
    for(int i = 0;i < receiver.length();++i){
        recv *= 10;
        recv += receiver[i] - '0';
    }

    string tmp = "";
    //cout << receiver << "\n";

    if(user[recv].inuse){
        tmp = "*** " + user[clientUserId].name + " told you ***: " + message + "\n";
        cout << tmp << "11\n";
        write(user[recv].fdNum,tmp.c_str(),tmp.length());
    } else {
        tmp = "*** Error: user #" + to_string(recv) + " does not exist yet. ***\n";
        cout << tmp << "22\n";
        write(clientid,tmp.c_str(),tmp.length());
    }
}

// about parsing and executing commands
void SeparateArg(string s,int clientid){
    vector<string> args;
    string tmp =  "";
    int clientUserId = fdToUser[clientid];

    for(int i = 0;i < s.length();++i){
        if(s[i] == ' '){
            if(!EmptyStr(tmp)){
                args.push_back(tmp);
                tmp = "";
            }
        } else {
            tmp += s[i];

            if(i == s.length() - 1 && !EmptyStr(tmp)){
                args.push_back(tmp);
                tmp = "";
            }
        }
    }

    parsed[clientUserId][linecnt[clientUserId]].instr.push_back(args);
    // cout << "argSize : " << args.size() << "\n";
    // for(int i = 0;i < args.size();++i){
    //     cout << args[i] << "\n";
    // }
    // cout << "\n";
}

void SeparateCommand(string s,int clientid){
    // init
    int result;
    int clientUserId = fdToUser[clientid];
    string tmp = "";
    bool sendrecv = false; // checking for command that contains both send and receive

    for(int i = 0;i < parsed[clientUserId][linecnt[clientUserId]].instr.size();++i){
        parsed[clientUserId][linecnt[clientUserId]].instr[i].clear();
    }
    parsed[clientUserId][linecnt[clientUserId]].instr.clear();
    parsed[clientUserId][linecnt[clientUserId]].isNumberPipe = false;
    parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2 = false;
    user[clientUserId].isUserPipe = -1;
    user[clientUserId].recvFrom = -1;

    for(int i = 0;i < s.length();++i){
        if(i == 0){
            while(s[i] == ' ' && i < s.length()) ++i;
        }
        // line -> groups of one command
        if((s[i] == '|' || s[i] == '!') && !EmptyStr(tmp)){
            //cout << "in1\n";

            //every string that is of the same command

            //cout << "33" << tmp << "33\n";
            SeparateArg(tmp,clientid);
            
            //check whether it is number pipe
            if(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){

                if(s[i] == '|') {
                    parsed[clientUserId][linecnt[clientUserId]].isNumberPipe = true;
                } else if(s[i] == '!'){
                    parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2 = true;
                }

                int num = 0; // how many lines after should this command execute
                while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                    num *= 10;
                    num += s[i + 1] - '0';
                    i++;
                }

                // push to future command
                parsed[clientUserId][linecnt[clientUserId]].nextNline = (linecnt[clientUserId] + num) % M;

                if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe || parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2){
                    parsed[clientUserId][(linecnt[clientUserId] + num) % M].beNumberPiped = true;
                }
            }

            tmp = "";
        } else if((s[i] == '>' || s[i] == '<') && !EmptyStr(tmp)){
            //cout << "in2\n";

            //is user pipe
            if(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){

                bool separated = false;

                // check whether clientid is a sender or receiver
                if(s[i] == '>'){
                    // sender
                    user[clientUserId].isUserPipe = 0;

                    while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                        user[clientUserId].isUserPipe *= 10;
                        user[clientUserId].isUserPipe += s[i + 1] - '0';
                        ++i;
                    }

                    // set the user who is piped by this user

                    while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                    if(i + 1 < s.length() && s[i + 1] == '<'){
                        if(i + 2 < s.length() && s[i + 2] - '0' >= 0 && s[i + 2] - '0' <= 9){
                            // another user pipe appear
                            ++i;
                            user[clientUserId].recvFrom = 0;

                            while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                                user[clientUserId].recvFrom *= 10;
                                user[clientUserId].recvFrom += s[i + 1] - '0';
                                ++i;
                            }

                            while(i + 1 < s.length() && s[i + 1] == ' ') ++i;
                        }
                    }

                    SeparateArg(tmp,clientid);
                    tmp = "";
                    separated = true;

                    cout << "sender!\n";
                } else if(s[i] == '<'){
                    user[clientUserId].recvFrom = 0;

                    while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                        user[clientUserId].recvFrom *= 10;
                        user[clientUserId].recvFrom += s[i + 1] - '0';
                        ++i;
                    }

                    while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                    if(i + 1 < s.length() && s[i + 1] == '>'){
                        if(i + 2 < s.length() && s[i + 2] - '0' >= 0 && s[i + 2] - '0' <= 9){
                            // another user pipe
                            ++i;
                            user[clientUserId].isUserPipe = 0;

                            while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                                user[clientUserId].isUserPipe *= 10;
                                user[clientUserId].isUserPipe += s[i + 1] - '0';
                                ++i;
                            }

                            // set the user who is piped by this user
                            while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                            SeparateArg(tmp,clientid);
                            tmp = "";
                            separated = true;
                        } else {
                            // write to file
                            // do nothing?
                            continue;
                        }
                    }

                    cout << "receiver!\n";
                }

                if(!separated && i >= s.length() - 1 && !EmptyStr(tmp)){
                    SeparateArg(tmp,clientid);
                    tmp = "";
                }

            } else { // write to file?
                tmp += s[i];

                if(i == s.length() - 1 && !EmptyStr(tmp)){
                    SeparateArg(tmp,clientid);
                    tmp = "";
                }
            }
        } else {
            tmp += s[i];

            //cout << "in3\n";

            if(tmp == "yell" && parsed[clientUserId][linecnt[clientUserId]].instr.size() == 0){
                vector<string> args;
                args.push_back(tmp);
                tmp = "";

                // skip all space
                while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                while(i + 1 < s.length()){
                    tmp += s[i + 1];
                    ++i;
                }
                cout << "11" << tmp << "11\n";
                args.push_back(tmp);
                tmp = "";
                parsed[clientUserId][linecnt[clientUserId]].instr.push_back(args);
            } else if(tmp == "tell" && parsed[clientUserId][linecnt[clientUserId]].instr.size() == 0){
                vector<string> args;
                args.push_back(tmp);
                tmp = "";

                while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                    tmp += s[i + 1];
                    ++i;
                }

                args.push_back(tmp);
                tmp = "";

                while(i + 1 < s.length() && s[i + 1] == ' ') ++i;

                while(i + 1 < s.length()){
                    tmp += s[i + 1];
                    ++i;
                }

                args.push_back(tmp);
                tmp = "";

                parsed[clientUserId][linecnt[clientUserId]].instr.push_back(args);
            }

            if(i == s.length() - 1 && !EmptyStr(tmp)){
                SeparateArg(tmp,clientid);
                tmp = "";
            }
        }
    }

    cout << "in separate command :\n";
    for(int i = 0;i < parsed[clientUserId][linecnt[clientUserId]].instr.size();++i){
        cout << i << " ";
        for(int j = 0;j < parsed[clientUserId][linecnt[clientUserId]].instr[i].size();++j){
            cout << parsed[clientUserId][linecnt[clientUserId]].instr[i][j] << " ";
        }
        cout << "\n";
    }
    cout << "\n";
}

void partialCommand(vector<string> &singleLine,int pipeid,int type,int clientid){ //single command without |
    string filename = "./";
    cstr arg[M];
    int argcnt = 0;
    int fileid = -1;
    int clientUserId = fdToUser[clientid];

    for(int i = 0;i < singleLine.size();++i){
        //cerr << singleLine[i] << "\n";
        if(singleLine[i] == ">"){
            i++;
            filename += singleLine[i];
            fileid = open(filename.c_str(),O_CREAT|O_TRUNC|O_RDWR,0777);
        } else {
            arg[argcnt] = strdup(singleLine[i].c_str());
            argcnt++;
        }
    }

    arg[argcnt] = NULL;
    argcnt++;

    // type = 0 : begin
    // type = 1 : middle
    // type = 2 : end

    if(fileid == -1){
        // no output to file
        if(type == 0){
            // input part
            if(parsed[clientUserId][linecnt[clientUserId]].beNumberPiped){
                dup2(parsed[clientUserId][linecnt[clientUserId]].fd[0],STDIN_FILENO);
            } else if(user[clientUserId].recvFrom != -1){
                if(user[clientUserId].recvFrom == -2){
                    // means that it attemp to change buf fail
                    int devNull = open("/dev/null",O_RDONLY);
                    dup2(devNull,STDIN_FILENO);
                } else if(user[user[clientUserId].recvFrom].inuse){
                    dup2(user[clientUserId].fd[user[clientUserId].recvFrom][0],STDIN_FILENO);
                } else {
                    int devNull = open("/dev/null",O_RDONLY);
                    dup2(devNull,STDIN_FILENO);
                }
            }

            // output part
            if(parsed[clientUserId][linecnt[clientUserId]].instr.size() == 1){ // only one command
                if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe){
                    dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDOUT_FILENO);
                    dup2(clientid,STDERR_FILENO);
                    //cerr << parsed[linecnt].nextNline << "\n";
                } else if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2){
                    dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDOUT_FILENO);
                    dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDERR_FILENO);
                    //cerr << "no\n";
                } else if(user[clientUserId].isUserPipe != -1){
                    if(user[clientUserId].isUserPipe == -2){
                        // illegal user pipe
                        int devNull = open("/dev/null",O_WRONLY);
                        dup2(devNull,STDOUT_FILENO);
                        dup2(clientid,STDERR_FILENO);
                    } else if(user[user[clientUserId].isUserPipe].inuse){
                        // legal user pipe
                        dup2(user[user[clientUserId].isUserPipe].fd[clientUserId][1],STDOUT_FILENO);
                        dup2(clientid,STDERR_FILENO);
                    } else {
                        // illegal user pipe
                        int devNull = open("/dev/null",O_WRONLY);
                        dup2(devNull,STDOUT_FILENO);
                        dup2(clientid,STDERR_FILENO);
                    }
                    
                } else {
                    dup2(clientid,STDOUT_FILENO);
                    dup2(clientid,STDERR_FILENO);
                }
            } else {
                dup2(fd[clientUserId][pipeid % N][1],STDOUT_FILENO);
                dup2(clientid,STDERR_FILENO);
            }
            
        } else if(type == 1){
            dup2(fd[clientUserId][(pipeid - 1 + N) % N][0],STDIN_FILENO);
            dup2(fd[clientUserId][pipeid % N][1],STDOUT_FILENO);
        } else {
            // number pipe redirect output
            if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe){
                dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDOUT_FILENO);
            } else if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2){
                dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDOUT_FILENO);
                dup2(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd[1],STDERR_FILENO);
            } else if(user[clientUserId].isUserPipe != -1){
                if(user[clientUserId].isUserPipe == -2){
                    // illegal user pipe
                    int devNull = open("/dev/null",O_WRONLY);
                    dup2(devNull,STDOUT_FILENO);
                    dup2(clientid,STDERR_FILENO);
                } else if(user[user[clientUserId].isUserPipe].inuse){
                    // legal user pipe
                    dup2(user[user[clientUserId].isUserPipe].fd[clientUserId][1],STDOUT_FILENO);
                    dup2(clientid,STDERR_FILENO);
                } else {
                    // illegal user pipe
                    int devNull = open("/dev/null",O_WRONLY);
                    dup2(devNull,STDOUT_FILENO);
                    dup2(clientid,STDERR_FILENO);
                }
                
            } else {
                dup2(clientid,STDOUT_FILENO);
                dup2(clientid,STDERR_FILENO);
            }

            dup2(fd[clientUserId][(pipeid - 1 + N) % N][0],STDIN_FILENO); 
        }
    } else {
        if(type == 0){ // begin
            // input part
            if(parsed[clientUserId][linecnt[clientUserId]].beNumberPiped){
                dup2(parsed[clientUserId][linecnt[clientUserId]].fd[0],STDIN_FILENO);
            } else if(user[clientUserId].recvFrom != -1){
                if(user[clientUserId].recvFrom == -2){
                    int devNull = open("/dev/null",O_RDONLY);
                    dup2(devNull,STDIN_FILENO);
                } else if(user[user[clientUserId].recvFrom].inuse){
                    dup2(user[clientUserId].fd[user[clientUserId].recvFrom][0],STDIN_FILENO);
                } else {
                    int devNull = open("/dev/null",O_RDONLY);
                    dup2(devNull,STDIN_FILENO);
                }
            }
            // output part

            dup2(fileid,STDOUT_FILENO);
            dup2(clientid,STDERR_FILENO);
        } else if(type == 2){ // command such as ls | number > test.txt
            dup2(fd[clientUserId][(pipeid - 1 + N) % N][0],STDIN_FILENO);
            dup2(fileid,STDOUT_FILENO);
            dup2(clientid,STDERR_FILENO);
        } else {
            //cerr << "jizz?\n";
        }
    }

    if(type == 0){
        // single command
        close(fd[clientUserId][pipeid % N][0]);
        close(fd[clientUserId][pipeid % N][1]);
    } else {
        close(fd[clientUserId][(pipeid - 1 + N) % N][0]);
        close(fd[clientUserId][(pipeid - 1 + N) % N][1]);
        close(fd[clientUserId][pipeid % N][0]);
        close(fd[clientUserId][pipeid % N][1]);
    }

    for(int i = 0;i < N;++i){
        if(parsed[clientUserId][i].pipeOpen){
            close(parsed[clientUserId][i].fd[0]);
            close(parsed[clientUserId][i].fd[1]);
        }
    }

    // close child socket
    close(sockfd);
    for(int i = 1;i < C;++i){
        if(user[i].inuse)
            close(user[i].fdNum); 
    }

    // close child user pipe
    for(int i = 1;i < C;++i){
        for(int j = 1;j < C;++j){
            if(user[i].pipeOpen[j]){
                close(user[i].fd[j][0]);
                close(user[i].fd[j][1]);
            }
        }
    }

    string tmpPath(Getenv("PATH",clientid));
    string commandPath[N];
    int pathcnt = 0;

    // separate the union of env path
    for(int i = 0;i < tmpPath.length();++i){
        if(tmpPath[i] == ':'){
            pathcnt++;
            continue;
        }

        commandPath[pathcnt] += tmpPath[i];

        if(i == tmpPath.length() - 1){
            pathcnt++;
        }
    }

    for(int i = 0;i < pathcnt;++i){
        if(commandPath[i] == ".") commandPath[i] += "/";
        else commandPath[i] = "./" + commandPath[i] + "/";
    }
    

    for(int i = 0;i < pathcnt;++i)
        execvp(strcat(strdup(commandPath[i].c_str()),arg[0]),arg);

    //cerr << "1" << arg[0] << "2\n";
    cerr << "Unknown command: [" << arg[0] << "].\n";
    exit(0);
}

void exeCommand(string line,int clientid){
    // string without pipe 
    // belongs to the first command
    // the remainings are arguments
    int clientUserId = fdToUser[clientid];
    int pipeId = 0;
    int instrLen = parsed[clientUserId][linecnt[clientUserId]].instr.size();
    string filename = "./";

    // open future pipe
    if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe || parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2){
        if(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].pipeOpen == false){
            parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].pipeOpen = true;
            if(pipe(parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].fd) == -1){
                //cerr << "error creating fpipe\n";
                parsed[clientUserId][parsed[clientUserId][linecnt[clientUserId]].nextNline].pipeOpen = false;
            }
        }
    }

    // all contain user ids
    int pipeTo = user[clientUserId].isUserPipe;
    int pipeBy = user[clientUserId].recvFrom;

    cout << "pipeTo : " << pipeTo << "\n";
    cout << "pipeBy : " << pipeBy << "\n";

    // receive pipe from specific user
    if(pipeBy != -1){
        if(pipeBy < 0 || (pipeBy >= 0 && !user[pipeBy].inuse)){
            // user not exist yet
            string errMes = "*** Error: user #" + to_string(pipeBy) + " does not exist yet. ***\n";
            write(clientid,errMes.c_str(),errMes.length());
            user[clientUserId].recvFrom = -2;
            pipeBy = -2;
        } else if(user[clientUserId].pipeOpen[pipeBy] == false){
            // the pipe does not exist yet
            string errMes = "*** Error: the pipe #" + to_string(pipeBy) + "->#" + to_string(clientUserId) + " does not exist yet. ***\n";
            write(clientid,errMes.c_str(),errMes.length());
            user[clientUserId].recvFrom = -2;
            pipeBy = -2;
        } else {
            //user[clientUserId].pipeOpen = false;

            string mes = "*** " + user[clientUserId].name + " (#" + to_string(clientUserId) + ") just received from " + user[pipeBy].name + " (#" + to_string(pipeBy) + ") by '" + line + "' ***\n";

            for(int i = 1;i < C;++i){
                if(user[i].inuse){
                    write(user[i].fdNum,mes.c_str(),mes.length());
                }
            }
        }
    }

        // open future user pipe (pipe to other user)
    if(pipeTo != -1){
        if(pipeTo < 0 || (pipeTo >= 0 && !user[pipeTo].inuse)){
            // user not exist
            string errMes = "*** Error: user #" + to_string(pipeTo) + " does not exist yet. ***\n";
            write(clientid,errMes.c_str(),errMes.length());
            user[clientUserId].isUserPipe = -2;
            pipeTo = -2;
        } else if(user[pipeTo].pipeOpen[clientUserId]){
            // already created this pipe
            string errMes = "*** Error: the pipe #" + to_string(clientUserId) + "->#" + to_string(pipeTo) + " already exists. ***\n";
            write(clientid,errMes.c_str(),errMes.length());
            user[clientUserId].isUserPipe = -2;
            pipeTo = -2;
        } else {

            user[pipeTo].pipeOpen[clientUserId] = true;

            if(pipe(user[pipeTo].fd[clientUserId]) == -1){
                cout << "pipe creation fail\n";
                user[pipeTo].pipeOpen[clientUserId] = false;
            }

            string mes = "*** " + user[clientUserId].name + " (#" + to_string(clientUserId) + ") just piped '" + line + "' to " + user[pipeTo].name + " (#" + to_string(pipeTo) + ") ***\n";

            for(int i = 1;i < C;++i){
                if(user[i].inuse){
                    write(user[i].fdNum,mes.c_str(),mes.length());
                }
            }
            
        }
    }

    vector<int> pid;
    signal(SIGCHLD,childHandler);

    for(int i = 0;i < instrLen;++i){
        int childpid;
        int st;

        if(pipe(fd[clientUserId][(i % N)]) == -1){
            --i;
            continue;
        }

        if((childpid = fork()) < 0){
            // error
            while((childpid = fork()) < 0){
                waitpid(-1,&st,0);
            }
            if(childpid > 0){
                pid.push_back(childpid);
                close(fd[clientUserId][(i % N)][0]);
                close(fd[clientUserId][(i % N)][1]);
                --i;
                continue;
            } else {
                close(fd[clientUserId][(i % N)][0]);
                close(fd[clientUserId][(i % N)][1]);
                close(fd[clientUserId][((i - 1 + N) % N)][0]);
                close(fd[clientUserId][((i - 1 + N) % N)][1]);
                exit(0);
            }
        } else if(childpid == 0){
            // child process
            // fd[i] : pipe from i -> i + 1

            if(i == 0) partialCommand(parsed[clientUserId][linecnt[clientUserId]].instr[i],(i % N),0,clientid);
            else if(i != instrLen - 1) partialCommand(parsed[clientUserId][linecnt[clientUserId]].instr[i],(i % N),1,clientid);
            else partialCommand(parsed[clientUserId][linecnt[clientUserId]].instr[i],(i % N),2,clientid);
        } else {
            pid.push_back(childpid);
        }

        if(i - 1 >= 0){
            close(fd[clientUserId][((i - 1 + N) % N)][0]);
            close(fd[clientUserId][((i - 1 + N) % N)][1]);
        }

    }
    
    if(instrLen >= 2){
        close(fd[clientUserId][(instrLen - 1 + N) % N][0]);
        close(fd[clientUserId][(instrLen - 1 + N) % N][1]);
    } else {
        close(fd[clientUserId][(instrLen - 1 + N) % N][0]);
        close(fd[clientUserId][(instrLen - 1 + N) % N][1]);
    }

    // close number pipe
    if(parsed[clientUserId][linecnt[clientUserId]].pipeOpen == true){
        close(parsed[clientUserId][linecnt[clientUserId]].fd[0]);
        close(parsed[clientUserId][linecnt[clientUserId]].fd[1]);
    }

    // close user pipe
    if(pipeBy != -1 && pipeBy != -2 && pipeBy >= 0 && user[clientUserId].pipeOpen[pipeBy] && user[pipeBy].inuse){
        close(user[clientUserId].fd[pipeBy][0]);
        close(user[clientUserId].fd[pipeBy][1]);
        user[clientUserId].pipeOpen[pipeBy] = false;
    }

    int st;
    for(int i = 0;i < pid.size();++i){
        if(parsed[clientUserId][linecnt[clientUserId]].isNumberPipe == false && parsed[clientUserId][linecnt[clientUserId]].isNumberPipe2 == false
         && user[clientUserId].isUserPipe == -1) // need to change
            waitpid(pid[i],&st,0);
    }

    parsed[clientUserId][linecnt[clientUserId]].beNumberPiped = false;
    parsed[clientUserId][linecnt[clientUserId]].pipeOpen = false;
    
}

void exeShell(string line,int clientid){
    int clientUserId = fdToUser[clientid];

    char pa[3] = "% ";

    if(EmptyStr(line)) return;

    SeparateCommand(line,clientid);

    if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "printenv"){

        string tmp = Getenv(parsed[clientUserId][linecnt[clientUserId]].instr[0][1],clientid);

        if(tmp.length() != 0){
            write(clientid,tmp.c_str(),tmp.length());
            write(clientid,"\n",1);
        }

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    } else if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "setenv"){
        // cstr arg = strdup(parsed[clientUserId][linecnt[clientUserId]].instr[0][1].c_str());
        // cstr val = strdup(parsed[clientUserId][linecnt[clientUserId]].instr[0][2].c_str());
        
        // setenv(arg,val,1);

        Setenv(parsed[clientUserId][linecnt[clientUserId]].instr[0][1],parsed[clientUserId][linecnt[clientUserId]].instr[0][2],clientid);

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    } else if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "who"){
        Who(clientid);

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    } else if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "name"){

        Name(parsed[clientUserId][linecnt[clientUserId]].instr[0][1],clientid);

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    } else if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "tell"){

        Tell(parsed[clientUserId][linecnt[clientUserId]].instr[0][1],parsed[clientUserId][linecnt[clientUserId]].instr[0][2],clientid);

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    } else if(parsed[clientUserId][linecnt[clientUserId]].instr[0][0] == "yell"){

        Yell(parsed[clientUserId][linecnt[clientUserId]].instr[0][1],clientid);

        linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
        write(clientid,pa,strlen(pa));

        return;
    }
    
    exeCommand(line,clientid);

    cout << "finish exeCommand\n";
    linecnt[clientUserId] = (linecnt[clientUserId] + 1) % N;
    write(clientid,pa,strlen(pa));
}