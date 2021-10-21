#include<stdlib.h>
#include<stdio.h>
#include<iostream>
#include<string>
#include<string.h>
#include<vector>
#include <sstream>
#include<iterator>
#include<unistd.h> //fork
#include<wait.h> //waitpid
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<regex>
#include<sys/socket.h>
#include<netinet/in.h>


using namespace std;

string command_line_prompt="% ";

class shell{
    public:
    string line;
    char* path;
    vector <string> split_group;
    public:
    shell();
    void Run(int newsockfd);
    
};
class Pipe{
    public:
    int num_pipe[2];
    int count;
};
vector<string>Spilt_space(const string);
vector<string>Spilt_symbol(const string,const char);
void RunCmd(vector<vector<string>>,int,char* &,bool,string,vector<Pipe> &,bool &,int &,bool &);
void ResetVector(vector<string>&);
vector<const char*> Trans_VecToCharArr(vector<string>);
bool LegalCmd(string);

shell::shell(){
    setenv("PATH","bin:.",true);
    path=getenv("PATH");
}

void shell::Run(int newsockfd){
    vector<Pipe> pipestatus;
    do{
        cout<<command_line_prompt;
        getline(cin,line);
        //line=line.assign(line,0,line.size()-1);  //remove \r
        int num=0,pipenum=0;
        string out_file;
        bool file_check=0;
        bool err_pipe=false;
        //pipenum
        bool has_num_pipe = regex_match(line, regex("^.*(\\||!)[0-9]*$"));
        if(has_num_pipe==true){
            int count_num;
            int j=0;
            bool check_num=false;
            char pipe_num[4];
            if(line.rfind("!")!=string::npos){
                count_num=line.rfind("!");
                for(size_t i=count_num+1;i<line.size();i++){
                    if(line[i]!=' '){
                        if(j==4){
                            check_num=true;
                            break;
                        }
                        pipe_num[j]=line[i];
                        j++;
                        err_pipe=true;
                    }
                }
                if(check_num){
                    continue;
                }
            }else{
                count_num=line.rfind("|");
                for(size_t i=count_num+1;i<line.size();i++){
                    if(line[i]!=' '){
                        if(j==4){
                            check_num=true;
                            break;
                        }
                        pipe_num[j]=line[i];
                        j++;
                    }
                }
                if(check_num){
                    continue;
                }
            }
            //cout<<check_num<<endl;
            pipe_num[j]='\0';
            pipenum=atoi(pipe_num);
            line=line.assign(line,0,count_num);
        }
        //cout<<pipenum<<endl;
        //cout<<line<<endl;
        if(line.find(">",0)!=string::npos){
            num=line.find(">",0);
            out_file=out_file.assign(line,num+2,line.size()-1);
            line=line.assign(line,0,num);
            FILE * outf;
            outf=fopen(out_file.c_str(),"w");
            if(outf==NULL){
                    perror("open file error");
                    continue;
                }
            file_check=1;
        }

        split_group=Spilt_symbol(line,'|');
        if(split_group.size()!=0){
            vector<vector<string>>cmd(split_group.size());
            for(size_t i=0;i<split_group.size();i++){
                cmd[i]=Spilt_space(split_group[i]);
            }
            if(LegalCmd(cmd[0][0])){
                if(cmd[0][0]=="printenv"){
                    if(cmd[0].size()>=2){
                        path=getenv(cmd[0][1].c_str());
                    }
                    if(path!=NULL)
                        cout<<path<<endl;
                }else if(cmd[0][0]=="setenv"){
                    setenv(cmd[0][1].c_str(),cmd[0][2].c_str(),1);
                    path=getenv(cmd[0][1].c_str());
                }else{
                    break;
                }
                continue;
            }
           RunCmd(cmd,split_group.size(),path,file_check,out_file,pipestatus,has_num_pipe,pipenum,err_pipe);
            for(size_t i=0;i<split_group.size();i++){
                ResetVector(cmd[i]);
            }
            ResetVector(split_group);
        }
    }while (1);
    
}

vector<string>Spilt_space(const string line){
    istringstream ss(line);
    vector<string> vec((istream_iterator<string>(ss)),istream_iterator<string>());
    return vec;
}

vector<string>Spilt_symbol(const string line,const char symbol){
    stringstream ss(line);
    string item;
    vector<string>vec;
    while(getline(ss,item,symbol)){
        vec.push_back(item);
    }
    return vec;
}

void RunCmd(vector<vector<string>>cmd,int index,char* &path,bool file_check,string out_file,vector<Pipe> &pipestatus,bool &has_num_pipe,int &num,bool &err_pipe){
    int pipes[index-1][2];
    int fd_pip[2];
    bool number_zero=false;
    vector<Pipe>::iterator it;
    if(has_num_pipe){
        pipe(fd_pip);
    }
    for(it=pipestatus.begin();it!=pipestatus.end();it++){
        (*it).count--;
    }

    Pipe p;
    bool same_c=false;
    for(it=pipestatus.begin();it!=pipestatus.end();it++){
        if((*it).count==num){
            p.num_pipe[0]=(*it).num_pipe[0];
            p.num_pipe[1]=(*it).num_pipe[1];
            same_c=true;
            break;
        }
    }
    if (same_c!=true){
        p.num_pipe[0]=fd_pip[0];
        p.num_pipe[1]=fd_pip[1];
    }
    if(has_num_pipe){
        p.count=num;
        pipestatus.push_back(p);
    }
    
    for(it=pipestatus.begin();it!=pipestatus.end();it++){
        if((*it).count==0){
           number_zero=true;
           break;
        }
    }
    vector<pid_t> pid_table;
    for(size_t i=0;i<index;i++){
        vector<vector<const char*>> arr(index);
        arr[i]=Trans_VecToCharArr(cmd[i]);
        int status=0;
        if(index!=1){
            if(i<=index-2){
                pipe(pipes[i]);
            }
        }

        pid_t child_pid=fork();
        switch (child_pid){
        case -1:
            ::perror("fork error");
            ::exit(1);
            break;
        case 0: //child
            if(number_zero&&i==0){
                dup2((*it).num_pipe[0],fileno(stdin));
                close((*it).num_pipe[0]);
                close((*it).num_pipe[1]);
            }

            if(index!=1){  
                if(i==0){
                    dup2(pipes[0][1],fileno(stdout));
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                }else if(i<index-1 && i>0){
                    dup2(pipes[i-1][0],fileno(stdin));
                    close(pipes[i-1][0]);
                    close(pipes[i-1][1]);

                    dup2(pipes[i][1],fileno(stdout));
                    close(pipes[i][0]);
                    close(pipes[i][1]);
                }else if (i==(index-1)){
                    dup2(pipes[i-1][0],fileno(stdin));
                    close(pipes[i-1][0]);
                    close(pipes[i-1][1]);
                }
            }
            if(file_check==1 && i==(index-1)){
                int fd=open(out_file.c_str(),O_RDWR|O_APPEND|O_CREAT|O_TRUNC,0600);
                dup2(fd,fileno(stdout));
                close(fd);
            }
            if(has_num_pipe==true && i==(index-1)){
                if(err_pipe==true){
                    dup2(pipestatus.back().num_pipe[1],fileno(stderr));
                }
                dup2(pipestatus.back().num_pipe[1],fileno(stdout));
                close(pipestatus.back().num_pipe[1]);
            }
            if(execvp(arr[i][0],(char**)(arr[i].data()))==-1){
                //::perror("exec error");
                cerr << "Unknown command: [" << arr[i][0] << "]." << endl;
                ::exit(1);
            }
            break;
        default: //parent
            pid_table.push_back(child_pid);
            if(i<index-1 && i>0){
                close(pipes[i-1][0]);
                close(pipes[i-1][1]);
            }else if(i==index-1&&index!=1){
                close(pipes[i-1][0]);
                close(pipes[i-1][1]);
            }
            vector<Pipe>::iterator it_e=pipestatus.begin();
            while(it_e!=pipestatus.end()){
                if((*it_e).count==0){
                    close((*it_e).num_pipe[0]);
                    close((*it_e).num_pipe[1]);
                    if(it_e==pipestatus.begin()){
                        pipestatus.erase(it_e);
                        it_e=pipestatus.begin();
                        continue;
                    }
                    pipestatus.erase(it_e);
                }
                it_e++;
            }

            /*for(it=pipestatus.begin();it!=pipestatus.end();it++){
                if((*it).count==0){
                    close((*it).num_pipe[0]);
                    close((*it).num_pipe[1]);
                    pipestatus.erase(it);
                    it--;
                }
            }*/
            waitpid(-1,&status,WNOHANG);
            vector<pid_t>::iterator pid_it;
            for(pid_it=pid_table.begin();pid_it<pid_table.end();pid_it++){
                waitpid(*pid_it,&status,0);
            }
            //wait(&status);
            // waitpid(child_pid, &status, 0);
            break;
        }
    }
    /*for(size_t i=0;i<index;i++){
        wait(NULL);
    }*/
}

bool LegalCmd(string cmdstring){
    if(cmdstring=="printenv"||cmdstring=="setenv"||cmdstring=="exit"){
        return true;
    }
    return false;
}

void ResetVector(vector<string> &arg){
    arg.clear();
    vector<string>().swap(arg); //free mem
}

vector<const char*> Trans_VecToCharArr(vector<string> arg){
    vector<const char*> strings;
    for (int i = 0; i < arg.size(); ++i)
        strings.push_back(arg[i].data());
    strings.push_back(NULL);
    return strings;
}


int main(int argc,const char **argv){
    int sockfd,newsockfd,childpid;
    struct sockaddr_in cli_addr,serv_addr;
    socklen_t clilen;
    int PORT = atoi(argv[1]);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("server erro");

    bzero((char*)&serv_addr, sizeof(serv_addr)); //clear
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serv_addr.sin_port = htons(PORT);
    
    if(::bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <0)
        perror("bind error");
    
    listen(sockfd, 5); // listen client .
    signal(SIGCHLD,SIG_IGN);
    for(;;)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen); 
        
        if (newsockfd < 0)
        {
            perror("server cannot accept");
            return -1;
        }
        if ((childpid = fork()) < 0)
        {
            perror("server fork error");
            return -1;
        }
        else if (childpid == 0)
        {
            
            shell np_shell;
            dup2(newsockfd, 0);
            dup2(newsockfd, 1);
            dup2(newsockfd , 2);
            close(sockfd);
            np_shell.Run(newsockfd);
            
            close(newsockfd);
            exit(0);
        }
        else
        {
            close(newsockfd);
        }
    }

    return 0;
}