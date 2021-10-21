#include <cstdlib>
#include <iostream>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <netdb.h>
#include <arpa/inet.h>
#define MAX_LENGTH 4000
#define MAX_LENGTH_6 30000
using namespace std;
using namespace boost::asio;

io_service ioservice;

class BindSession:public enable_shared_from_this<BindSession>{
    private:
    ip::tcp::socket client_socket,server_socket;
    string remote_h,remote_p;
    vector<unsigned char> to_client,to_server;
    ip::tcp::acceptor _acceptor;

    public:
    BindSession(ip::tcp::socket socket):
    _acceptor(ioservice,ip::tcp::endpoint(ip::tcp::v4(),0)),
    client_socket(move(socket)),
    server_socket(ioservice),to_client(MAX_LENGTH_6),to_server(MAX_LENGTH_6){

    }

    void start(){
        result_to_client();
    }

    void result_to_client(){
        auto self(shared_from_this());
        to_client[0]=0;
        to_client[1]=90;
        to_client[2]=_acceptor.local_endpoint().port()>>8;
        to_client[3]=_acceptor.local_endpoint().port()%(1<<8);
        to_client[4]=0;
        to_client[5]=0;
        to_client[6]=0;
        to_client[7]=0;
    
        client_socket.async_write_some(buffer(to_client,8),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_accept();
                }
            }
            );
    
    }

    void do_accept(){
        auto self(shared_from_this());
        _acceptor.async_accept(server_socket,
            [this,self](boost::system::error_code ec){
                if(!ec){
                   // client_socket.async_write_some(buffer(to_client,8),
                   // [this,self](boost::system::error_code ec,size_t length){
                     //   if(!ec){
                    do_read_from_client();
                    do_read_from_server();
                    //    }
                    //}
                    //);
                   
                }else{
                    do_accept();
                    //cout<<"bind accept error"<<endl;
                }
            }
        );
    }

    void do_read_from_client(){
        auto self(shared_from_this());
        client_socket.async_read_some(boost::asio::buffer(to_server,MAX_LENGTH_6),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_write_to_server(length);
                }else{

                    client_socket.close();
                    server_socket.close();
                    //std::cout<<"READ,CLE"<<ec.message()<<endl;
                }
            }
        );
    }

    void do_read_from_server(){
        auto self(shared_from_this());
        server_socket.async_read_some(boost::asio::buffer(to_client,MAX_LENGTH_6),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_write_to_client(length);
                }else{

                    client_socket.close();
                    server_socket.close();
                    //std::cout<<ec.message()<<endl;
                }
            }
        );
    }

    void do_write_to_server(size_t length){
        auto self(shared_from_this());
        server_socket.async_write_some(boost::asio::buffer(to_server,length),
            [this,self](boost::system::error_code ec,size_t length){
                //if(length==0){
                   // server_socket.close();
                 //   client_socket.close();
               // }else 
                if(!ec){
                    do_read_from_client();
                }else{

                    client_socket.close();
                    server_socket.close();
                }
            }
        );
    }

    void do_write_to_client(size_t length){
        auto self(shared_from_this());
        client_socket.async_write_some(boost::asio::buffer(to_client,length),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_read_from_server();
                }else{
                    client_socket.close();
                    server_socket.close();
                }
            }
        );
    }
};

class Session:public enable_shared_from_this<Session>{
    private:
    ip::tcp::resolver _resolver;
    ip::tcp::socket destination_socket;
    string remote_h,remote_p;

    vector<unsigned char> source_buf;
    vector<unsigned char> destination_buf;

    void read_socks_head(){
        auto self(shared_from_this());
        //cout<<"start read sock head"<<endl;

        source_socket.async_read_some(boost::asio::buffer(source_buf,MAX_LENGTH),
        [this,self](boost::system::error_code ec,size_t length){
            if(!ec){
                unsigned char CD = (unsigned char)source_buf[1];
                unsigned int D_PORT= (((unsigned int)source_buf[2])<<8) + ((unsigned int)source_buf[3]);
                string S_IP=source_socket.remote_endpoint().address().to_string();
                unsigned short S_PORT = source_socket.remote_endpoint().port();
                char ip[20];
                sprintf(ip,"%u.%u.%u.%u",(unsigned int)source_buf[4],(unsigned int)source_buf[5],(unsigned int)source_buf[6],(unsigned int)source_buf[7]);
                int end_i,start_i;
                bool a4=false;

                cout<<"<S_IP>:"<<S_IP<<endl;
                cout<<"<S_PORT>:"<<S_PORT<<endl;
                if(ip[0]=='0'&& ip[1]=='.' && ip[2]=='0'&& ip[3]=='.'&& ip[4]=='0'&& ip[5]=='.'){
                    a4=true;
                    int tmp=source_buf.size()+1;
                    for(int i=source_buf.size();i>0;i--){
                        if(source_buf[i]==NULL){
                            if(tmp!=(i+1)){
                                end_i=tmp;
                                start_i=i;
                                break;
                            }
                            tmp=i;
                        }
                    }
                    char a[end_i-start_i];
                    int j=0;

                    memset(ip,0,20);
                    for(int i=start_i+1;i<=end_i-1;i++){
                        a[j]=source_buf[i];
                        ip[j]=source_buf[i];
                        j++;
                    }
                    a[j]='\0';
                    struct hostent *he=gethostbyname(a);
                    char *ipa=inet_ntoa(*(struct in_addr*)he->h_addr_list[0]);
                    cout<<"<D_IP>:"<<ipa<<endl;
                }else{
                    cout<<"<D_IP>:"<<(unsigned int)source_buf[4]<<"."<<(unsigned int)source_buf[5]<<"."<<(unsigned int)source_buf[6]<<"."<<(unsigned int)source_buf[7]<<endl;
                }
                //cout<<"+-----------------------------------------+"<<endl;
                
                cout<<"<D_PORT>:"<<D_PORT<<endl;
                if(CD==1){
                    cout<<"<Command>:CONNECT"<<endl;
                }else if(CD==2){
                    cout<<"<Command>:BIND"<<endl;
                }
                

                if (CD==2){
                    //Bind
                    fstream f;
                    f.open("socks.conf",ios::in);
                    char buf[200];
                    bool pass=false;
                    memset(buf,0,200);
                    while(f.getline(buf,sizeof(buf))){
                        int index=0;
                        while(true){
                            if(buf[index]==0){
                                memset(buf,0,200);
                                break;
                            }else{
                                if(buf[index]=='b'){
                                    index=index+2;
                                    int t_index=0;
                                    while(true){
                                        if(buf[index]!=0 && ip[t_index]==0 || buf[index]==0 && ip[t_index]!=0 ){
                                            break;
                                        }else if(buf[index]=='*'){
                                            pass=true;
                                            break;
                                        }else if(buf[index]==ip[t_index]&&buf[index+1]==0){
                                            pass=true;
                                            break;
                                        }else if(buf[index]!=ip[t_index]){
                                            break;
                                        }
                                        index++;
                                        t_index++;
                                    }
                                }
                            }

                            index++;

                        }
                    }

                    f.close();

                    if(pass){
                        cout<<"<Reply>:Accept"<<endl;
                        make_shared<BindSession>(move(source_socket))->start();
                    }else{
                        cout<<"<Reply>:Reject"<<endl;
                        source_buf[0]=0;
                        source_buf[1]=91;
                        source_socket.async_write_some(boost::asio::buffer(source_buf,8),
                            [this,self](boost::system::error_code ec,size_t length){
                                if(!ec){
                                    //after write
                                }
                            }
                        );
                    }
                }else{
                    //Connect
                    fstream f;
                    f.open("socks.conf",ios::in);
                    char buf[200];
                    bool pass=false;
                    memset(buf,0,200);
                    while(f.getline(buf,sizeof(buf))){
                        int index=0;
                        while(true){
                            if(buf[index]==0){
                                memset(buf,0,200);
                                break;
                            }else{
                                if(buf[index]=='c'){
                                    index=index+2;
                                    int t_index=0;
                                    while(true){
                                        if(buf[index]!=0 && ip[t_index]==0 || buf[index]==0 && ip[t_index]!=0 ){
                                            break;
                                        }else if(buf[index]=='*'){
                                            pass=true;
                                            break;
                                        }else if(buf[index]==ip[t_index]&&buf[index+1]==0){
                                            pass=true;
                                            break;
                                        }else if(buf[index]!=ip[t_index]){
                                            break;
                                        } 
                                        index++;
                                        t_index++;
                                    }
                                }
                            }

                            index++;

                        }
                    }

                    f.close();

                    if(pass){
                        cout<<"<Reply>:Accept"<<endl;
                        if (a4==true){
                            remote_h=ip;
                        }else{
                            remote_h=boost::asio::ip::address_v4(ntohl(*((uint32_t*)&source_buf[4]))).to_string();
                        }
                        
                        remote_p=to_string(ntohs(*((uint16_t*)&source_buf[2])));
                        //cout<<remote_h<<endl;
                        //cout<<remote_p<<endl;

                        do_resolve();
                        

                    }else{
                        cout<<"<Reply>:Reject"<<endl;
                        source_buf[0]=0;
                        source_buf[1]=91;

                        source_socket.async_write_some(boost::asio::buffer(source_buf,8),
                            [this,self](boost::system::error_code ec,size_t length){
                                if(!ec){
                                    //after write
                                }
                            }
                        );
                    }

                }
            }
        }
        
        );
    }



    void do_resolve(){
        auto self(shared_from_this());
        //cout<<"do resolve"<<endl;

        _resolver.async_resolve(ip::tcp::resolver::query(remote_h,remote_p),
            [this,self](const boost::system::error_code& ec,ip::tcp::resolver::iterator it){
                if(!ec){
                    do_connect(it);
                }
            }
            );
    }

    void do_connect(ip::tcp::resolver::iterator& it){
        auto self(shared_from_this());

        destination_socket.async_connect(*it,
            [this,self](const boost::system::error_code& ec){
                if(!ec){
                    write_socks_head();
                }
            }
        );
    }

    void write_socks_head(){
        auto self(shared_from_this());

        source_buf[0]=0;
        source_buf[1]=90;

        source_socket.async_write_some(boost::asio::buffer(source_buf,8),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_read_from_client();
                    do_read_from_server();
                }
            }
        );
    }

    void do_read_from_client(){
        auto self(shared_from_this());

        source_socket.async_read_some(boost::asio::buffer(source_buf,MAX_LENGTH),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_write_to_server(length);
                }
            }
        );
    }

    void do_read_from_server(){
        auto self(shared_from_this());

        destination_socket.async_read_some(boost::asio::buffer(destination_buf,MAX_LENGTH),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    do_write_to_client(length);
                }
            }
        );
    }

    void do_write_to_server(size_t length){
        auto self(shared_from_this());

        destination_socket.async_write_some(boost::asio::buffer(source_buf,length),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                   // cout<<"write to server:"<<length<<endl;
                    do_read_from_client();
                }
            }
        );
    }

    void do_write_to_client(size_t length){
        auto self(shared_from_this());

        source_socket.async_write_some(boost::asio::buffer(destination_buf,length),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    //cout<<"write to client:"<<length<<endl;
                    do_read_from_server();
                }
            }
        );
    }
    public:
    ip::tcp::socket source_socket;
    Session(ip::tcp::socket socket): source_socket(move(socket)),
                                                                    _resolver(ioservice),
                                                                    destination_socket(ioservice),
                                                                    source_buf(MAX_LENGTH),
                                                                    destination_buf(MAX_LENGTH){
        //cout<<"New session"<<endl;
    }

    void start(){
        read_socks_head();
    }
};

class Server{
    private:
    ip::tcp::socket _socket;
    ip::tcp::acceptor _acceptor;
    void do_accept(){
        _acceptor.async_accept(_socket,[this](boost::system::error_code ec){
            if(!ec){
                ioservice.notify_fork(boost::asio::io_context::fork_prepare);
                if(fork()==0){
                    ioservice.notify_fork(boost::asio::io_context::fork_child);
                    _acceptor.close();
                    
                    make_shared<Session>(move(_socket))->start();
                }else{
                    ioservice.notify_fork(boost::asio::io_context::fork_parent);
                    _socket.close();
                    do_accept();
                }
            
            
            }else{
                do_accept();
            }

            //error
        });
    }
    public:
    Server(unsigned port)
    :_acceptor(ioservice,ip::tcp::endpoint(ip::tcp::v4(),port)),_socket(ioservice){
        do_accept();
    }

};

int main(int argc,char* const argv[]){
    try{
        if(argc!=2){
            std::cerr << "Usage: ./socks_server <port>\n";
            return 1;
        }
        Server server(atoi(argv[1]));
        ioservice.run();
    }catch(exception &e){
        std::cerr << "Exception: " << e.what() << "\n";
    }
}