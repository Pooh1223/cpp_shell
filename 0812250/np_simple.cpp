#include <iostream>
#include <unistd.h>
#include <string>
#include <cstring>
#include <vector>
#include <sys/wait.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#define N 1003
#define M 1003
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

Str parsed[M];
int fd[N][2];
int linecnt = 0;
int sockfd;
int clientfd;

// init
void init(){
    for(int i = 0;i < M;++i){
        parsed[i].isNumberPipe = false;
        parsed[i].isNumberPipe2 = false;
        parsed[i].beNumberPiped = false;
        parsed[i].pipeOpen = false;
    }
    setenv("PATH","bin:.",1);
    linecnt = 0;
}

bool EmptyStr(string s){
    for(int i = 0;i < s.length();++i){
        if(s[i] != ' ') return false;
    }
    return true;
}

void childHandler(int signo){
    int st;
    int pid;
    while(pid = waitpid(-1,&st,WNOHANG) > 0){
        //do nothing
    }
}

void SeperateArg(string s){
    vector<string> args;
    string tmp = "";

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

    parsed[linecnt].instr.push_back(args);
}

void SeparateCommand(string s){
    // init
    string tmp = "";
    for(int i = 0;i < parsed[linecnt].instr.size();++i){
        parsed[linecnt].instr[i].clear();
    }
    parsed[linecnt].instr.clear();
    parsed[linecnt].isNumberPipe = false;
    parsed[linecnt].isNumberPipe2 = false;

    for(int i = 0;i < s.length();++i){
        // line -> groups of one command
        if((s[i] == '|' || s[i] == '!') && !EmptyStr(tmp)){

            //every string that is of the same command

            //cout << "33" << tmp << "33\n";
            SeperateArg(tmp);
            
            //check whether it is number pipe
            if(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){

                if(s[i] == '|') {
                    parsed[linecnt].isNumberPipe = true;
                } else if(s[i] == '!'){
                    parsed[linecnt].isNumberPipe2 = true;
                }

                int num = 0; // how many lines after should this command execute
                while(i + 1 < s.length() && s[i + 1] - '0' >= 0 && s[i + 1] - '0' <= 9){
                    num *= 10;
                    num += s[i + 1] - '0';
                    i++;
                }

                // push to future command
                parsed[linecnt].nextNline = (linecnt + num) % M;

                if(parsed[linecnt].isNumberPipe || parsed[linecnt].isNumberPipe2){
                    parsed[(linecnt + num) % M].beNumberPiped = true;
                }
            }

            tmp = "";
        } else {
            tmp += s[i];

            if(i == s.length() - 1 && !EmptyStr(tmp)){
                //cout << "55" << tmp << "55\n";
                SeperateArg(tmp);

                tmp = "";
            }
        }
    }

}

void partialCommand(vector<string> &singleLine,int pipeid,int type){ //single command without |
    string filename = "./";
    cstr arg[M];
    int argcnt = 0;
    int fileid = -1;

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
            if(parsed[linecnt].beNumberPiped){
                dup2(parsed[linecnt].fd[0],STDIN_FILENO);
            }
            // output part
            if(parsed[linecnt].instr.size() == 1){ // only one command
                if(parsed[linecnt].isNumberPipe){
                    dup2(parsed[parsed[linecnt].nextNline].fd[1],STDOUT_FILENO);
                    dup2(clientfd,STDERR_FILENO);
                    //cerr << parsed[linecnt].nextNline << "\n";
                } else if(parsed[linecnt].isNumberPipe2){
                    dup2(parsed[parsed[linecnt].nextNline].fd[1],STDOUT_FILENO);
                    dup2(parsed[parsed[linecnt].nextNline].fd[1],STDERR_FILENO);
                    //cerr << "no\n";
                } else {
                    dup2(clientfd,STDOUT_FILENO);
                    dup2(clientfd,STDERR_FILENO);
                }
            } else {
                dup2(fd[pipeid % N][1],STDOUT_FILENO);
                dup2(clientfd,STDERR_FILENO);
            }
            
        } else if(type == 1){
            dup2(fd[(pipeid - 1 + N) % N][0],STDIN_FILENO);
            dup2(fd[pipeid % N][1],STDOUT_FILENO);
        } else {
            // number pipe redirect output
            if(parsed[linecnt].isNumberPipe){
                dup2(parsed[parsed[linecnt].nextNline].fd[1],STDOUT_FILENO);
            } else if(parsed[linecnt].isNumberPipe2){
                dup2(parsed[parsed[linecnt].nextNline].fd[1],STDOUT_FILENO);
                dup2(parsed[parsed[linecnt].nextNline].fd[1],STDERR_FILENO);
            } else {
                dup2(clientfd,STDOUT_FILENO);
                dup2(clientfd,STDERR_FILENO);
            }

            dup2(fd[(pipeid - 1 + N) % N][0],STDIN_FILENO); 
        }
    } else {
        if(type == 0){ // begin
            // input part
            if(parsed[linecnt].beNumberPiped){
                dup2(parsed[linecnt].fd[0],STDIN_FILENO);
            }
            // output part

            dup2(fileid,STDOUT_FILENO);
            dup2(clientfd,STDERR_FILENO);
        } else if(type == 2){ // command such as ls | number > test.txt
            dup2(fd[(pipeid - 1 + N) % N][0],STDIN_FILENO);
            dup2(fileid,STDOUT_FILENO);
            dup2(clientfd,STDERR_FILENO);
        } else {
            //cerr << "jizz?\n";
        }
    }

    if(type == 0){
        // single command
        close(fd[pipeid % N][0]);
        close(fd[pipeid % N][1]);
    } else {
        close(fd[(pipeid - 1 + N) % N][0]);
        close(fd[(pipeid - 1 + N) % N][1]);
        close(fd[pipeid % N][0]);
        close(fd[pipeid % N][1]);
    }

    for(int i = 0;i < N;++i){
        if(parsed[i].pipeOpen){
            close(parsed[i].fd[0]);
            close(parsed[i].fd[1]);
        }
    }

    close(sockfd);
    close(clientfd);

    string tmpPath(getenv("PATH"));
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

void exeCommand(){
    // string without pipe 
    // belongs to the first command
    // the remainings are arguments
    
    int pipeId = 0;
    int instrLen = parsed[linecnt].instr.size();
    string filename = "./";

    // open future pipe
    if(parsed[linecnt].isNumberPipe || parsed[linecnt].isNumberPipe2){
        if(parsed[parsed[linecnt].nextNline].pipeOpen == false){
            parsed[parsed[linecnt].nextNline].pipeOpen = true;
            if(pipe(parsed[parsed[linecnt].nextNline].fd) == -1){
                //cerr << "error creating fpipe\n";
                parsed[parsed[linecnt].nextNline].pipeOpen = false;
            }
        }
    }

    vector<int> pid;
    signal(SIGCHLD,childHandler);

    for(int i = 0;i < instrLen;++i){
        int childpid;
        int st;

        if(pipe(fd[(i % N)]) == -1){
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
                close(fd[(i % N)][0]);
                close(fd[(i % N)][1]);
                --i;
                continue;
            } else {
                close(fd[(i % N)][0]);
                close(fd[(i % N)][1]);
                close(fd[((i - 1 + N) % N)][0]);
                close(fd[((i - 1 + N) % N)][1]);
                exit(0);
            }
        } else if(childpid == 0){
            // child process
            // fd[i] : pipe from i -> i + 1

            if(i == 0) partialCommand(parsed[linecnt].instr[i],(i % N),0);
            else if(i != instrLen - 1) partialCommand(parsed[linecnt].instr[i],(i % N),1);
            else partialCommand(parsed[linecnt].instr[i],(i % N),2);
        } else {
            pid.push_back(childpid);
        }

        if(i - 1 >= 0){
            close(fd[((i - 1 + N) % N)][0]);
            close(fd[((i - 1 + N) % N)][1]);
        }

    }
    
    if(instrLen >= 2){
        close(fd[(instrLen - 1 + N) % N][0]);
        close(fd[(instrLen - 1 + N) % N][1]);
    } else {
        close(fd[(instrLen - 1 + N) % N][0]);
        close(fd[(instrLen - 1 + N) % N][1]);
    }

    if(parsed[linecnt].pipeOpen){
        close(parsed[linecnt].fd[0]);
        close(parsed[linecnt].fd[1]);
    }

    int st;
    for(int i = 0;i < pid.size();++i){
        if(parsed[linecnt].isNumberPipe == false && parsed[linecnt].isNumberPipe2 == false)
            waitpid(pid[i],&st,0);
    }

    parsed[linecnt].beNumberPiped = false;
    parsed[linecnt].pipeOpen = false;
    
}

int portNumber(char* argv){
    int num = 0;
    for(int i = 0;i < strlen(argv);++i){
        num *= 10;
        num += argv[i] - '0';
    }
    return num;
}

int main(int argc, char *argv[]){
    // socket things
    int res;
    int flag = 1;
    struct sockaddr_in sa;
    char buffer[15005];

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0){
        // do nothing?
    }
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portNumber(argv[1]));
    sa.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr*)&sa, sizeof(struct sockaddr));
    listen(sockfd,10);

    struct sockaddr_in client_addr;

    socklen_t addrlen = sizeof(client_addr);
    printf("wait...\n");
    clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
    printf("---connection success---\n");

    init();

    //char line[15005];
    string line;
    char pa[3] = "% ";
    bool connected = true;

    do{
        line = "";
        if(!connected){
            init();

            printf("wait...\n");
            clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
            printf("---connection success---\n");

            connected = true;
        }
        
        write(clientfd,pa,strlen(pa));

        //getline(cin,line);
        memset(buffer,'\0',sizeof(buffer));
        read(clientfd,buffer,sizeof(buffer));
        
        //buffer[strlen(buffer)] = '\0';
        //int commandLen = strlen(buffer) - 2 < 0 ? 0 : strlen(buffer) - 2;
        for(int i = 0;i < strlen(buffer);++i){
            if(buffer[i] == '\r' || buffer[i] == '\n') continue;
            line += buffer[i];
        }
        cout << line << " line " << strlen(buffer) << "\n";

        if(EmptyStr(line)) continue;

        SeparateCommand(line);

        if(parsed[linecnt].instr[0][0] == "printenv"){
            cstr arg = strdup(parsed[linecnt].instr[0][1].c_str());
            cstr tmp;
            //cstr newLine = "\n";
            if(tmp = getenv(arg)){
                write(clientfd,tmp,strlen(tmp));
                write(clientfd,"\n",1);
            }

            continue;
        } else if(parsed[linecnt].instr[0][0] == "setenv"){
            cstr arg = strdup(parsed[linecnt].instr[0][1].c_str());
            cstr val = strdup(parsed[linecnt].instr[0][2].c_str());
            
            setenv(arg,val,1);
            continue;
        } else if(parsed[linecnt].instr[0][0] == "exit"){
            //finish = true;
            close(clientfd);
            connected = false;
            continue;
        }

        exeCommand();

        linecnt = (linecnt + 1) % N;
    }while(1);

    return 0;
}