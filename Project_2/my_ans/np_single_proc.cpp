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
#include<iomanip>
#include<arpa/inet.h>
#include<signal.h>
//#include<cstdio>
#define MAX_CLIENT 30
#define BUF_SIZE 4096

using namespace std;

string command_line_prompt="% ";


class Env{
    public:
    string  name;
    string value;
};


class Pipe{
    public:
    int num_pipe[2];
    int count;
};

class User_Pipe{
    public:
    int send_id[MAX_CLIENT+1][MAX_CLIENT+1];
    int recv_id[MAX_CLIENT+1];
    int user_fd[MAX_CLIENT+1][MAX_CLIENT+1][2];
};

class Client{
    public:
    string name;
    string  ip;
    int fdNum;
    int user_id;
    unsigned short port;
    vector<Env> envTable;
    vector<Pipe> pipestatus;
};

class shell{
    public:
    string line;
    string path;
    vector <string> split_group;
    //vector<Pipe> pipestatus;
    public:
    shell(vector<Client>::iterator,string); //it_cli,line
    int Run(vector<Client>::iterator&);
    
};

fd_set afds;
int nfds;
int msock;
vector<Client> clientTable;
int uidTable[MAX_CLIENT+1];
User_Pipe userpipeTable;

vector<string>Spilt_space(const string);
vector<string>Spilt_symbol(const string,const char);
void RunCmd(vector<vector<string>>,int,string &,bool,string,vector<Pipe> &,bool &,int &,bool &,bool &,int &,bool &,int &,int &);
void ResetVector(vector<string>&);
vector<const char*> Trans_VecToCharArr(vector<string>);
bool LegalCmd(string);
void PrintWelcome(void);
void Broad(vector<Client>::iterator,int,string,int);
int Create_UserId(void);
vector<Client>::iterator GetClient(int,int); //data choose 1:fd 2:uid
string Get_Cli_Env(vector<Client>::iterator,string);
void Erase_Env(vector<Client>::iterator,string);
void Clear_UserPipe(int);
void Who(vector<Client>::iterator);
string Cmd_Handle_Msg(vector<string>,int);
void Yell(vector<Client>::iterator,string);
void Tell(vector<Client>::iterator,int,string);
void Name(vector<Client>::iterator,string);

void PrintWelcome(){
    ::cout << "****************************************" << endl;
    ::cout << "** Welcome to the information server. **" << endl;
    ::cout << "****************************************" << endl;  
}

void Broad(vector<Client>::iterator it_client,int action,string msg,int target_id){
    string tmp;
    vector<Client>::iterator it_target;
    for(int i=0;i<nfds;i++){
        tmp="";
        if(FD_ISSET(i,&afds)){
            if(i!=msock){
                switch(action){
                case 0://log in
                    tmp="*** User '(no name)' entered from "+(*it_client).ip+":"+to_string((*it_client).port)+". ***\n";
                    break;
                case 1://log out
                    if((*it_client).fdNum!=i)
                        tmp="*** User '"+(*it_client).name+"' left. ***\n";
                    break;
                case 2://name
                    tmp="*** User from "+(*it_client).ip+":"+to_string((*it_client).port)+" is named '"+(*it_client).name+"'. ***\n";
                    break;
                case 3://yell
                    tmp="*** "+(*it_client).name+" yelled ***: "+msg+"\n";
                    break;
                case 4://send
                    it_target=GetClient(target_id,2);
                    tmp="*** "+(*it_client).name+" (#"+to_string((*it_client).user_id)+") just piped '"+msg+"' to "+(*it_target).name+" (#"+to_string(target_id)+") ***\n";
                    break;
                case 5://recv
                    it_target=GetClient(target_id,2);
                    tmp="*** "+(*it_client).name+" (#"+to_string((*it_client).user_id)+") just received from "+(*it_target).name+" (#"+to_string((*it_target).user_id)+") by '"+msg+"' ***\n";
                    break;
                default:
                    break;
                }
                if(write(i,tmp.c_str(),tmp.length())==-1){
                    perror("send");
                }
            }
        }
    }
}

int Create_UserId(){
    for(int i=1;i<MAX_CLIENT+1;i++){
        if(uidTable[i]==0){
            uidTable[i]=1;
            userpipeTable.recv_id[i]=1;
            return i;
        }
    }
    return 0;
}

vector<Client>::iterator GetClient(int data,int choose){
    vector<Client>::iterator it;
    for(it=clientTable.begin();it!=clientTable.end();it++){
        if(choose==1){
            if((*it).fdNum==data)
                return it;
        }else if(choose==2){
            if((*it).user_id==data)
                return it;
        }
    }
    return it;
}

string Get_Cli_Env(vector<Client>::iterator it,string name){
    vector<Env>::iterator it_e;
    for(it_e=(*it).envTable.begin();it_e!=(*it).envTable.end();it_e++){
        if(name==(*it_e).name){
            return (*it_e).value;
        }
    }
    return NULL;
}

void Erase_Env(vector<Client>::iterator it,string name){
    vector<Env>::iterator it_e;
    for(it_e=(*it).envTable.begin();it_e!=(*it).envTable.end();it_e++){
        if(name==(*it_e).name){
            (*it).envTable.erase(it_e);
            return;
        }
    }
}

void Clear_UserPipe(int id){
    userpipeTable.recv_id[id]=0;
    for(int i=1;i!=MAX_CLIENT;i++){
        userpipeTable.send_id[i][id]=0;
        if(userpipeTable.user_fd[i][id][0]!=-1){
            close(userpipeTable.user_fd[i][id][0]);
        }
        if(userpipeTable.user_fd[i][id][1]!=-1){
            close(userpipeTable.user_fd[i][id][1]);
        }
    }
    
}

void Who(vector<Client>::iterator it){
    ::cout<<"<ID> "<<"<nickname>  "<<"<IP:port>   "<<"<indicate me>"<<endl;
    vector<Client>::iterator it_c;
    for(int i=1;i<MAX_CLIENT+1;i++){
        if(uidTable[i]!=0){
            it_c=GetClient(i,2);//user_id
            ::cout<<(*it_c).user_id<<"    "<<(*it_c).name<<"    "<<(*it_c).ip+":"+to_string((*it_c).port);
            //::fflush(stdout);
            if((*it_c).fdNum==(*it).fdNum){
                ::cout<<" "<<"<-me"<<endl;
            }else{
                ::cout<<endl;
            }
        }
    }
}

string Cmd_Handle_Msg(vector<string>cmd,int begin){
    string msg;
    for(int i=begin;i!=cmd.size()-1;i++){
        msg=msg+cmd[i]+" ";
    }
    msg=msg+cmd[cmd.size()-1];
    return msg;
}

void Yell(vector<Client>::iterator it,string msg){
    Broad(it,3,msg,0);
}

void Tell(vector<Client>::iterator it,int id,string msg){
    if(uidTable[id]!=0){
        msg="*** "+(*it).name+" told you ***: "+msg+"\n";
        vector<Client>::iterator it_target=GetClient(id,2);
        write((*it_target).fdNum,msg.c_str(),msg.length());
    }else{
        ::cout<<"*** Error: user #"<<id<<" does not exist yet. ***"<<endl;
    }
}

void Name(vector<Client>::iterator it,string name){
    vector<Client>::iterator it_c;
    bool check=false;
    for(it_c=clientTable.begin();it_c!=clientTable.end();it_c++){
        if((*it_c).name==name){
            check=true;
            break;
        }
    }
    if(check){
        ::cout<<"*** User '"<<name<<"' already exists. ***"<<endl;
    }else{
        (*it).name=name;
        Broad(it,2,"",0);
    }
}

int main(int argc,const char **argv){
    fd_set rfds;
    struct sockaddr_in cli_addr,serv_addr;
    socklen_t clilen;
    int PORT = atoi(argv[1]);
    
    vector<Client>::iterator it_client;

    if((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("server erro");
    }

    bzero((char*)&serv_addr, sizeof(serv_addr)); //clear
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serv_addr.sin_port = htons(PORT);

    //handle ctrl+c close server port
    const int opt = 1;
    if(setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0){
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    if(setsockopt(msock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))<0){
        perror("setsockopt(SO_REUSEPORT) failed");
    }
    //

    if(::bind(msock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <0){
        perror("bind error");
    }

    listen(msock, MAX_CLIENT); // listen client .
    signal(SIGCHLD,SIG_IGN);
    nfds=getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock,&afds);
    ::cout<<"Server Start."<<endl;
    for(;;)
    {
        memcpy(&rfds,&afds,sizeof(rfds));
        int ssock;
        if(select(nfds,&rfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0)<0){
            perror("select error");
        }
        for(int i=0;i<nfds;i++){
            if(FD_ISSET(i,&rfds)){
                if(i==msock){
                    //handle new connection
                    clilen=sizeof(cli_addr);
                    ssock=accept(msock,(struct sockaddr*)& cli_addr,&clilen);

                    if(ssock==-1){
                        perror("accept");
                    }else{
                        //New Client
                        Client client;
                        client.name="(no name)";
                        client.fdNum=ssock;
                        client.user_id=Create_UserId();
                        client.ip=inet_ntoa(cli_addr.sin_addr);
                        client.port=ntohs(cli_addr.sin_port);
                        Env env;
                        env.name="PATH";
                        //string path_init="bin:.";
                        env.value="bin:."; //string to char*
                        client.envTable.push_back(env);
                        clientTable.push_back(client);
                        FD_SET(ssock,&afds);
                        dup2(ssock,1);
                        PrintWelcome();
                        Broad(--clientTable.end(),0,"",0);//
                        send(ssock,"% ",2,0);

                    }
                }else{
                    //handle client
                    int nbytes;
                    char buf[BUF_SIZE];

                    if((nbytes=recv(i,buf,sizeof(buf),0))<=0){
                        if(nbytes==0){
                            ::cout<<"CLOSE:"<<i<<endl;
                        }else{
                            perror("recv");
                        }
                        //
                        Broad(it_client,1,"",0);
                        uidTable[(*it_client).user_id]=0;
                        Clear_UserPipe((*it_client).user_id);
                        clientTable.erase(it_client);
                        close(i);
                        close(0);
                        close(1);
                        close(2);
                        dup2(msock,0);
                        dup2(msock,1);
                        dup2(msock,2);
                        FD_CLR(i,&afds);
                    }else{
                        //get some data
                    
                        it_client=GetClient(i,1);
                        string line;
                        buf[nbytes-1]='\0';
                        line=buf;
                        //line=line.assign(line,0,line.size()-1);  //remove \r
                        /*
                        char* cli_path=Get_Cli_Env(it_client,"PATH");
                        setenv("PATH",cli_path,1);*/
                        shell np_shell(it_client,line);
                        dup2(i,0);
                        dup2(i,1);
                        dup2(i,2);
                        int status=np_shell.Run(it_client);
                        if(status==-1){
                            Broad(it_client,1,"",0);
                            uidTable[(*it_client).user_id]=0;
                            Clear_UserPipe((*it_client).user_id);
                            clientTable.erase(it_client);
                            close(i);
                            close(0);
                            close(1);
                            close(2);
                            dup2(msock,0);
                            dup2(msock,1);
                            dup2(msock,2);
                            
                            FD_CLR(i, &afds); // 從 master set 中移除
                        }
                       //

                    }
                }
            }
        }

    }

    return 0;
}

shell::shell(vector<Client>::iterator init_Cli,string init_line){
    /*setenv("PATH","bin:.",true);
    path=getenv("PATH");*/
    
    //Init
    path=Get_Cli_Env(init_Cli,"PATH");
    setenv("PATH",path.c_str(),1);
    line=init_line;
}

int shell::Run(vector<Client>::iterator &it_client){
    
    //path=getenv("PATH");
    int num=0,pipenum=0,send_id=0,recv_id=0;
    string out_file;
    string broad_msg;
    bool file_check=0;
    bool err_pipe=false;
    bool send_msg=false;
    bool recv_msg=false;
    bool has_send_msg; //send msg
    bool has_recv_msg;//recv msg
    bool has_num_pipe;

    if(!(line.rfind("yell")!=string::npos) && !(line.rfind("tell")!=string::npos)&&!(line.rfind("name")!=string::npos)&&!(line.rfind("who")!=string::npos)&&!(line.rfind("printenv")!=string::npos)&&!(line.rfind("setenv")!=string::npos)&&!(line.rfind("exit")!=string::npos)){
        has_recv_msg=regex_match(line, regex("^.*<[0-9].*"));//recv msg
        if(has_recv_msg==true){
            int count_num;
            int j=0;
            bool check_num=false;
            char recv_num[2];
            if(line.rfind("<")!=string::npos){
                count_num=line.rfind("<");
                for(size_t i=count_num+1;i<line.size();i++){
                    if(line[i]!=' '){
                        if(j==2){
                            check_num=true;
                            break;
                        }
                        recv_num[j]=line[i];
                        j++;
                        send_msg=true;
                    }else{
                        break;
                    }
                }
                if(check_num){
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
                }
            }
            recv_num[j]='\0';
            recv_id=atoi(recv_num);
            if(userpipeTable.recv_id[recv_id]==0){   //no the recv user
                    ::cout<<"*** Error: user #"<<recv_id<<" does not exist yet. ***"<<endl;
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
            }else{
                if(userpipeTable.send_id[recv_id][(*it_client).user_id]==0){   //already has send
                    ::cout<<"*** Error: the pipe #"<<recv_id<<"->#"<<(*it_client).user_id<<" does not exist yet. ***"<<endl;
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
                }else{
                    userpipeTable.send_id[recv_id][(*it_client).user_id]=0;
                    broad_msg=line;
                    Broad(it_client,5,line,recv_id);
                    int line_size=line.size();
                    string tmp_line;
                    tmp_line.assign(line,0,count_num-1);
                    if(count_num+j+1==line_size){
                        line=tmp_line;
                    }else{
                        line=tmp_line+line.assign(line,count_num+j+2,line_size);
                    }
                }
            }
        }
        has_send_msg=regex_match(line, regex("^.*>[0-9]*$")); //send msg
        if(has_send_msg==true){
            int count_num;
            int j=0;
            bool check_num=false;
            char send_num[2];
            if(line.rfind(">")!=string::npos){
                count_num=line.rfind(">");
                for(size_t i=count_num+1;i<line.size();i++){
                    if(line[i]!=' '){
                        if(j==2){
                            check_num=true;
                            break;
                        }
                        send_num[j]=line[i];
                        j++;
                        send_msg=true;
                    }
                }
                if(check_num){
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
                }
            }
            send_num[j]='\0';
            send_id=atoi(send_num);
            if(userpipeTable.recv_id[send_id]==0){   //no the recv user
                    ::cout<<"*** Error: user #"<<send_id<<" does not exist yet. ***"<<endl;
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
            }else{
                if(userpipeTable.send_id[(*it_client).user_id][send_id]==1){   //already has send
                    ::cout<<"*** Error: the pipe #"<<(*it_client).user_id<<"->#"<<send_id<<" already exists. ***"<<endl;
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
                }else{
                    userpipeTable.send_id[(*it_client).user_id][send_id]=1;
                    //Broad
                    if(has_recv_msg){
                        Broad(it_client,4,broad_msg,send_id);
                    }else{
                        Broad(it_client,4,line,send_id);
                    }
                    line=line.assign(line,0,count_num);
                }
            }
        }
        //pipenum
        has_num_pipe = regex_match(line, regex("^.*(\\||!)[0-9]*$"));
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
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
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
                    ::cout<<command_line_prompt;
                    ::fflush(stdout);
                    return 1;
                }
            }
            pipe_num[j]='\0';
            pipenum=atoi(pipe_num);
            line=line.assign(line,0,count_num);
        }
        if(line.find(">",0)!=string::npos){
            num=line.find(">",0);
            out_file=out_file.assign(line,num+2,line.size()-1);
            line=line.assign(line,0,num);
            FILE * outf;
            outf=fopen(out_file.c_str(),"w");
            if(outf==NULL){
                    perror("open file error");
                    return -2;
                }
            file_check=1;
        }
    }


    ResetVector(split_group);
    split_group=Spilt_symbol(line,'|');
    if(split_group.size()!=0){
        vector<vector<string>>cmd(split_group.size());
        for(size_t i=0;i<split_group.size();i++){
            cmd[i]=Spilt_space(split_group[i]);
        }
        if(LegalCmd(cmd[0][0])){
            if(cmd[0][0]=="printenv"){
                char* c_path;
                if(cmd[0].size()>=2){
                    c_path=getenv(cmd[0][1].c_str());
                }
                if(c_path!=NULL){
                    path=c_path;
                    ::cout<<path<<endl;
                }
            }else if(cmd[0][0]=="setenv"){
                Erase_Env(it_client,cmd[0][1]);
                Env tmp;
                tmp.name=cmd[0][1];
                tmp.value=cmd[0][2];
                (*it_client).envTable.push_back(tmp);
                setenv(cmd[0][1].c_str(),cmd[0][2].c_str(),1);
                path=getenv(cmd[0][1].c_str());
            }else if(cmd[0][0]=="who") { 
                Who(it_client);
            }else if(cmd[0][0]=="yell"){
                string msg=Cmd_Handle_Msg(cmd[0],1);
                Yell(it_client,msg);
            }else if(cmd[0][0]=="tell"){
                string msg=Cmd_Handle_Msg(cmd[0],2);
                Tell(it_client,atoi(cmd[0][1].c_str()),msg);
            }else if(cmd[0][0]=="name"){
                Name(it_client,cmd[0][1]);
            }else{
                return -1; //exit
            }
            ::cout<<command_line_prompt;
            ::fflush(stdout);
            return 1;
        }
        RunCmd(cmd,split_group.size(),path,file_check,out_file,(*it_client).pipestatus,has_num_pipe,pipenum,err_pipe,has_send_msg,send_id,has_recv_msg,recv_id,(*it_client).user_id);
        for(size_t i=0;i<split_group.size();i++){
            ResetVector(cmd[i]);
        }
        ResetVector(split_group);
    }
    //::cout<<pipestatus[0].count<<endl;
    ::cout<<command_line_prompt;
    ::fflush(stdout);
    return 1;
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

void RunCmd(vector<vector<string>>cmd,int index,string &path,bool file_check,string out_file,vector<Pipe> &pipestatus,bool &has_num_pipe,int &num,bool &err_pipe,bool &has_send_msg,int &send_id,bool &has_recv_msg,int &recv_id,int &own_id){
    int pipes[index-1][2];
    int fd_pip[2];
    bool number_zero=false;
    vector<Pipe>::iterator it;
    if(has_num_pipe){
        pipe(fd_pip);
    }
    if(has_send_msg && userpipeTable.user_fd[send_id]!=0){
        pipe(userpipeTable.user_fd[own_id][send_id]);
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
            if(has_recv_msg&&i==0){
                dup2(userpipeTable.user_fd[recv_id][own_id][0],fileno(stdin));
                close(userpipeTable.user_fd[recv_id][own_id][0]);
            }
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
            if(has_send_msg==true && i==(index-1)){
                dup2(userpipeTable.user_fd[own_id][send_id][1],fileno(stdout));
                close(userpipeTable.user_fd[own_id][send_id][1]);
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
                break;
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
            if(has_recv_msg && i==0){
                close(userpipeTable.user_fd[recv_id][own_id][0]);
                userpipeTable.user_fd[recv_id][own_id][0]=-1;
            }
            if(has_send_msg && i==(index-1)){
                close(userpipeTable.user_fd[own_id][send_id][1]);
                userpipeTable.user_fd[own_id][send_id][1]=-1;
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
    if(cmdstring=="printenv"||cmdstring=="setenv"||cmdstring=="exit"||cmdstring=="who"||cmdstring=="yell"||cmdstring=="tell"||cmdstring=="name"){
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