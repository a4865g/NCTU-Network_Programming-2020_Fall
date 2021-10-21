#include <cstdlib>
#include <iostream>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include<boost/format.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
using boost::asio::ip::tcp;
using namespace std;
//https://www.boost.org/doc/libs/1_47_0/doc/html/boost_asio/example/fork/process_per_connection.cpp

/*
       *REQUEST_METHOD:GET
       *REQUEST_URI:/console.cgi?tid=222
       *QUERY_STRING:tid=222&dd=111
       *SERVER_PROTOCOL:HTTP/1.1
       *HTTP_HOST:npbsd3.cs.nctu.edu.tw:55555
       *SERVER_ADDR:140.113.235.223
       *SERVER_PORT:55555
       *REMOTE_ADDR:client端的host名稱
       *REMOTE_PORT:client端的port
*/



void handle_http_line(vector <string>&,string);
void setenv_http(map<string,string>);

class Parser{
    public:
    static Parser getInstance(){
        static Parser instance;
        return instance;
    }
    static vector<string> split_trim(string line,const string &E_char="\t\n "){
    vector <string> handle_line;
    boost::trim(line); //DELETE Begin and End 'space'

    //can see https://kheresy.wordpress.com/2010/12/21/boost_string_algorithms_split/
    boost::split(handle_line,line,boost::is_any_of(E_char),boost::token_compress_on);
    
    //handle every element
    for (auto &i:handle_line) {
        boost::trim(i);
    }
    return handle_line;
}
    map<string,string> parse_http(string request);
};

class server {
public:
    server(boost::asio::io_service &io_service,unsigned short port)
        :io_service_(io_service),
        signal_set_(io_service,SIGCHLD),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
        socket_(io_service) {
            start_signal_wait();
            start_accept();
        }


private:
    void start_signal_wait() {
    signal_set_.async_wait(boost::bind(&server::handle_signal_wait, this));
    }
    void handle_signal_wait() {
        
        if (acceptor_.is_open()) {
           
            int status = 0;
            while (waitpid(-1, &status, WNOHANG) > 0) {}
             start_signal_wait();
        }
    }
    void start_accept() {
        acceptor_.async_accept(socket_,boost::bind(&server::handle_accept,this,_1));
    }
    void handle_accept(const boost::system::error_code& ec) {

        if (!ec) {
            io_service_.notify_fork(boost::asio::io_service::fork_prepare);

            if (fork() == 0) {
                io_service_.notify_fork(boost::asio::io_service::fork_child);

                acceptor_.close();
                signal_set_.cancel();

                start_read();
            } else  {
                io_service_.notify_fork(boost::asio::io_service::fork_parent);

                socket_.close();
                start_accept();
            }
        } else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            start_accept();
        }
    }

    void start_read() {
        socket_.async_read_some(boost::asio::buffer(buffer_),boost::bind(&server::handle_read, this, _1, _2));
    }

  void handle_read(const boost::system::error_code& ec, std::size_t length) {
        if (!ec) {
            map<string,string> http_result=Parser::getInstance().parse_http(string(buffer_.begin(),buffer_.begin()+length));
            
            setenv_http(http_result);
            setenv("SERVER_ADDR",socket_.local_endpoint().address().to_string().c_str(),1);
            setenv("SERVER_PORT",to_string(socket_.local_endpoint().port()).c_str(),1);
            setenv("REMOTE_ADDR",socket_.remote_endpoint().address().to_string().c_str(),1);
            setenv("REMOTE_PORT",to_string(socket_.remote_endpoint().port()).c_str(),1);

            cout<<"SERVER_ADDR:"<<socket_.local_endpoint().address().to_string().c_str()<<endl;
            cout<<"SERVER_PORT:"<<to_string(socket_.local_endpoint().port()).c_str()<<endl;
            cout<<"REMOTE_ADDR:"<<socket_.remote_endpoint().address().to_string().c_str()<<endl;
            cout<<"REMOTE_PORT:"<<to_string(socket_.remote_endpoint().port()).c_str()<<endl;
            cout<<endl<<endl;
            dup2(socket_.native_handle(),0);
            dup2(socket_.native_handle(),1);
            dup2(socket_.native_handle(),2);

            cout<<"HTTP/1.1"<<"200 OK\r\n";
            cout.flush();
            string target=http_result["TARGET"];
            target=target.substr(1); //del "/"
            char *argv[] = {nullptr};
            if (execv(target.c_str(), argv)==-1) {
                perror("execv error");
                exit(-1);
            }

        }
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    boost::asio::io_service &io_service_;
    boost::asio::signal_set signal_set_;
    std::array<char,1024> buffer_;
};

map<string,string> Parser::parse_http(string request){
    vector<string> http;
    handle_http_line(http,request);
    map<string,string> map_http;

    for(auto line:http){
        string key=line.substr(0,line.find(':')); // get ":" before all
        string value=line.substr(key.size()); //get ":" after all
        if(key!=line) {
            value=value.substr(2); //delete space
            if(key.front()=='\n') {
                key=key.substr(1); // delete    ('\n')
            }
            map_http.insert({key,value});
        } else {
            //trin_if:https://my.oschina.net/zmlblog/blog/131104
            boost::trim_if(line,boost::is_any_of("\n\r"));
            if(!line.empty()){
                vector<string> header=split_trim(line);
                map_http.insert({"REQUEST_METHOD",header.at(0)});
                map_http.insert({"REQUEST_URI",header.at(1)});
                map_http.insert({"SERVER_PROTOCOL",header.at(2)});
                string target=header.at(1).substr(0,header.at(1).find('?'));
                map_http.insert({"TARGET",target});
                if(header.at(1)!=target) {
                    map_http.insert({"QUERY_STRING",header.at(1).substr(target.size()+1)});
                } else {
                    map_http.insert({"QUERY_STRING",""});
                }
            }
        }
    }

    try {
        map_http.insert({"HTTP_HOST",map_http.at("Host")});
    } catch(exception &e) {
        cerr<<e.what()<<endl;
    }
    return map_http;
}

void handle_http_line(vector <string>& http,string line) {
    string::iterator it_l,it_r;
    for(it_l=line.begin(),it_r=line.begin();it_r!=line.end();it_r++){
        if((*it_r)=='\n') {
            string handle_line(it_l,it_r);
            if(handle_line.back()=='\r') {   //remove \r
                handle_line.pop_back(); 
            }
            http.push_back(handle_line);
            it_l=it_r;
        }
    }
}

void setenv_http(map<string,string> map_http){
    setenv("REQUEST_METHOD",map_http["REQUEST_METHOD"].c_str(),1);
    setenv("REQUEST_URI",map_http["REQUEST_URI"].c_str(),1);
    setenv("QUERY_STRING",map_http["QUERY_STRING"].c_str(),1);
    setenv("SERVER_PROTOCOL",map_http["SERVER_PROTOCOL"].c_str(),1);
    setenv("HTTP_HOST",map_http["HTTP_HOST"].c_str(),1);

    cout<<"REQUEST_METHOD:"<<map_http["REQUEST_METHOD"].c_str()<<endl;
    cout<<"REQUEST_URI:"<<map_http["REQUEST_URI"].c_str()<<endl;
    cout<<"QUERY_STRING:"<<map_http["QUERY_STRING"].c_str()<<endl;
    cout<<"SERVER_PROTOCOL:"<<map_http["SERVER_PROTOCOL"].c_str()<<endl;
    cout<<"HTTP_HOST:"<<map_http["HTTP_HOST"].c_str()<<endl;
}

int main(int argc,char  *argv[]) {
    signal(SIGCHLD,SIG_IGN);
    try {
        if(argc!=2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        server s(io_service, std::atoi(argv[1]));
        io_service.run();
    } catch (std::exception &e){
        std::cerr << "Exception: " << e.what() << "\n";
    }


    return 0;
}