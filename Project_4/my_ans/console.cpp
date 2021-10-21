#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <map>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include<boost/format.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/array.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#define MAX_LENGTH 4096
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

boost::asio::io_service service;

vector<string> split_trim(string,const string &);
void http_encode(string &);

class Request {
 public:
  enum command_type { connect = 1, bind = 2};

  Request(command_type cmd, const boost::asio::ip::tcp::endpoint& endpoint)
      : version_(4), command_(cmd) {
    // Only IPv4 is supported by the SOCKS 4 protocol.
    if (endpoint.protocol() != boost::asio::ip::tcp::v4()) {
      throw boost::system::system_error(
          boost::asio::error::address_family_not_supported);
    }

    // Convert port number to network byte order.
    unsigned short port = endpoint.port();
    port_high_byte_ = (port >> 8) & 0xff;
    port_low_byte_ = port & 0xff;

    // Save IP address in network byte order.
    address_ = endpoint.address().to_v4().to_bytes();
  }

  std::array<boost::asio::const_buffer, 7> buffers() const {
    return {
        {boost::asio::buffer(&version_, 1), boost::asio::buffer(&command_, 1),
         boost::asio::buffer(&port_high_byte_, 1),
         boost::asio::buffer(&port_low_byte_, 1), boost::asio::buffer(address_),
         boost::asio::buffer(""), boost::asio::buffer(&null_byte_, 1)}};
  }

  unsigned char version_;
  unsigned char command_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_;
  unsigned char null_byte_;
};

class Reply {
 public:
  enum status_type {
    request_granted = 90,
    request_failed = 91,
    request_failed_no_identd = 92,
    request_failed_bad_user_id = 93
  };

  Reply() : null_byte_(0), status_() {}

  array<boost::asio::mutable_buffer, 5> buffers() {
    return {{boost::asio::buffer(&null_byte_, 1),
             boost::asio::buffer(&status_, 1),
             boost::asio::buffer(&port_high_byte_, 1),
             boost::asio::buffer(&port_low_byte_, 1),
             boost::asio::buffer(address_)}};
  }

  bool success() const { return null_byte_ == 0 && status_ == request_granted; }

  unsigned char status() const { return status_; }

 private:
  unsigned char null_byte_;
  unsigned char status_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_;
};




class Host{
    public:
    string h;
    string p;
    string f;
};

class Console {
    public:
    static Console &getInstance() {
        static Console instance;
        return instance;
    }
    array<Host,5> host; //max client
    void print_initial_html();
};

vector<string> split_trim(string line,const string &E_char="\t\n "){
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
/*
void Print_vector(vector<string> a){
    vector<string>::iterator it;
    for(it=a.begin();it!=a.end();it++){
        std::cout<<*it<<endl;
    }
}*/

void http_encode(string &d){
    boost::algorithm::replace_all(d,"&","&amp;");
    boost::algorithm::replace_all(d,">","&gt;");
    boost::algorithm::replace_all(d,"<","&lt;");
    boost::algorithm::replace_all(d,"\"","&quot;");
    boost::algorithm::replace_all(d,"\'","&apos;");

    boost::algorithm::replace_all(d,"\n","&#13;");
    boost::algorithm::replace_all(d,"\r","");
}


class Socks_Session:public std::enable_shared_from_this<Socks_Session>{
    public:
    string session;
    tcp::resolver::query _q;
    string file_name;
    fstream file;
    tcp::socket _socket{service};
    tcp::resolver resolver;
    array<char,MAX_LENGTH> _bytes;
    string http_s,http_p;
    Socks_Session(string session,string host,string port,string file_name,tcp::resolver::query q)
                            :session(std::move(session)),_q(std::move(q)),file_name(file_name),
                                resolver(service),http_s(host),http_p(port){
                                file.open("./test_case/"+file_name,std::ios::in);
                                if(!file.is_open()){
                                    std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<"File Not Open!"<<"&NewLine;</b>';</script>" << endl;
                                }
                            }

    void resolve(){
        auto end_p=resolver.resolve(_q);
        boost::asio::connect(_socket,end_p);
        auto http_end_p=*resolver.resolve(tcp::resolver::query(http_s,http_p));
        Request socks_request(Request::connect,http_end_p);
        boost::asio::write(_socket,socks_request.buffers(),boost::asio::transfer_all());

        Reply socks_reply;
        boost::asio::read(_socket,socks_reply.buffers(),boost::asio::transfer_all());
        if(socks_reply.success()==false){
            std::cout<<"Connection failed."<<std::endl;
            std::cout<<"Status="<<socks_reply.status();
            return;
        }
        read_handler();
    }

    void read_handler(){
        auto self(shared_from_this());
        _socket.async_receive(buffer(_bytes),
            [this,self](boost::system::error_code ec,size_t length){
                if(!ec){
                    string data(_bytes.begin(),_bytes.begin()+length);
                    http_encode(data);
                    std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML+=\""<<data<<"\";</script>" << std::endl;
                    std::cout.flush();

                    if(data.find("%")!=string::npos){
                        string cmd;
                        getline(file,cmd);
                        http_encode(cmd);
                        std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<cmd<<"&NewLine;</b>';</script>" << std::endl;
                        std::cout.flush();
                        boost::asio::write(_socket,buffer(cmd+'\n'));
                    }
                    read_handler();
                }
            }
        );
    }
};

class Session:public std::enable_shared_from_this<Session>{
    private:
        
    public:
    tcp::socket tcp_socket{service};
        tcp::resolver::query q;
        tcp::resolver resolver{service};
        array<char,MAX_LENGTH> _data;
        string file_name;
        string session;
        std::fstream file;
        void do_read(){
            auto self(shared_from_this());

            tcp_socket.async_receive(
                boost::asio::buffer(_data),
                [this,self](boost::system::error_code ec,std::size_t length){
                    if (!ec){
                        string data(_data.begin(),_data.begin()+length);
                        http_encode(data);
                        std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML+=\""<<data<<"\";</script>" << std::endl;
                        std::cout.flush();

                        if(data.find("% ")!=string::npos){
                            string cmd,tmp;
                            getline(file,cmd);
                            tmp=cmd;
                            http_encode(tmp);
                            std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<tmp<<"&NewLine;</b>';</script>" << std::endl;
                            std::cout.flush();
                            tcp_socket.write_some(buffer(cmd+'\n'));
                        }
                        do_read();
                    }else{
                        //std::cout<<"read ec:"<<ec<<endl;
                    }
                }
            );
        }
    Session(string session,string port,string file_name,tcp::resolver::query q)
                            :session(std::move(session)),q(std::move(q)),file_name(file_name){
        std::cout<<endl;
      file.open("./test_case/" + file_name, std::ios::in);
      if (!file.is_open()){
          std::cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<"File Not Open!"<<"&NewLine;</b>';</script>" << endl;
      }
    }
    void connect_handler() {
        do_read();
    }

    void resolve_handler(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        tcp_socket.async_connect(*it, [this, self](boost::system::error_code ec) {
            if (!ec)
                connect_handler();
        });
  }
    void resolve() {
        auto self(shared_from_this());
        resolver.async_resolve(q,
                           [this, self](const boost::system::error_code &ec,
                                        tcp::resolver::iterator it) {
                             if (!ec)
                               resolve_handler(it);
                           });
  }
};

int main() {
    /*string a="h0=nplinux1.cs.nctu.edu.tw&p0=10&f0=t1.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=";
    setenv("QUERY_STRING",a.c_str(),1);*/
    getenv("QUERY_STRING");

    string socks_server,socks_port;
    char *query_string;
    query_string=getenv("QUERY_STRING");
    vector<string> query=split_trim(query_string,"&");
    vector<string>::iterator it;
    for(it=query.begin();it!=query.end();it++){
        string key=(*it).substr(0,(*it).find('='));
        string value=(*it).substr(key.size()+1);

        if(key=="sh"){
            socks_server=value;
            continue;
        }else if(key=="sp"){
            socks_port=value;
            continue;
        }



        if(value.empty()==false){
            if(key[0]=='h'){
                Console::getInstance().host.at((int)(key[1]-'0')).h=value;
            }else if (key[0]=='p') {
                Console::getInstance().host.at((int)(key[1]-'0')).p=value;
            } else if(key[0]=='f') {
                Console::getInstance().host.at((int)(key[1]-'0')).f=value;
            }
        }
    }
    Console::getInstance().print_initial_html();

    const auto &host=Console::getInstance().host;
    for(int i=0;i<host.size();++i){
        if(!(host.at(i).f.empty()&&host.at(i).h.empty()&&host.at(i).p.empty())){
            if(!socks_server.empty() && !socks_port.empty()){
                tcp::resolver::query q{socks_server,socks_port};
                std::make_shared<Socks_Session>(to_string(i),host.at(i).h,host.at(i).p,host.at(i).f,std::move(q))->resolve();
            }else{
                tcp::resolver::query q{host.at(i).h,host.at(i).p};
                std::make_shared<Session>(to_string(i),host.at(i).p,host.at(i).f,std::move(q))->resolve();
            }
        }
    }

    service.run();
}

void Console::print_initial_html(){
    std::cout<<"Content-type: text/html\r\n\r\n";
    std::cout<<"<!DOCTYPE html>"<<endl;
    std::cout<<"<html lang=\"en\">"<<endl;
    std::cout<<"  <head>"<<endl;
    std::cout<<"    <meta charset=\"UTF-8\" />"<<endl;
    std::cout<<"    <title>NP Project 4 Console</title>"<<endl;
    std::cout<<"    <link"<<endl;
    std::cout<<"      rel=\"stylesheet\""<<endl;
    std::cout<<"      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""<<endl;
    std::cout<<"      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""<<endl;
    std::cout<<"      crossorigin=\"anonymous\""<<endl;
    std::cout<<"    />"<<endl;
    std::cout<<"    <link"<<endl;
    std::cout<<"      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""<<endl;
    std::cout<<"      rel=\"stylesheet\""<<endl;
    std::cout<<"    />"<<endl;
    std::cout<<"    <link"<<endl;
    std::cout<<"      rel=\"icon\""<<endl;
    std::cout<<"      type=\"image/png\""<<endl;
    std::cout<<"      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""<<endl;
    std::cout<<"    />"<<endl;
    std::cout<<"    <style>"<<endl;
    std::cout<<"      * {"<<endl;
    std::cout<<"        font-family: 'Source Code Pro', monospace;"<<endl;
    std::cout<<"        font-size: 1rem !important;"<<endl;
    std::cout<<"      }"<<endl;
    std::cout<<"      body {"<<endl;
    std::cout<<"        background-color: #212529;"<<endl;
    std::cout<<"      }"<<endl;
    std::cout<<"      pre {"<<endl;
    std::cout<<"        color: #cccccc;"<<endl;
    std::cout<<"      }"<<endl;
    std::cout<<"      b {"<<endl;
    std::cout<<"        color: #01b468;"<<endl;
    std::cout<<"      }"<<endl;
    std::cout<<"    </style>"<<endl;
    std::cout<<"  </head>"<<endl;
    std::cout<<"  <body>"<<endl;
    std::cout<<"    <table class=\"table table-dark table-bordered\">"<<endl;
    std::cout<<"      <thead>"<<endl;
    std::cout<<"        <tr>"<<endl;
     //     <th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>
      //    <th scope=\"col\">nplinux2.cs.nctu.edu.tw:5678</th>
    
    for(size_t i=0;i<host.size();i++){
        if(!(host.at(i).f.empty()&&host.at(i).h.empty()&&host.at(i).p.empty())){
            std::cout<<"          <th scope=\"col\">"+host.at(i).h+":"+host.at(i).p+"</th>"<<endl;
        }
    }
    std::cout<<"        </tr>"<<endl;
    std::cout<<"      </thead>"<<endl;
    std::cout<<"      <tbody>"<<endl;
    std::cout<<"        <tr>"<<endl;
    //std::cout<<"     <td><pre id=\"s0\" class=\"mb-0\"></pre></td>"endl;
    //std::cout<<"     <td><pre id=\"s1\" class=\"mb-0\"></pre></td>"endl;
    
    for(size_t i=0;i<host.size();i++){
        if(!(host.at(i).f.empty()&&host.at(i).h.empty()&&host.at(i).p.empty())){
            std::cout<<"          <td><pre id=\"s"+to_string(i)+"\" class=\"mb-0\"></pre></td>"<<endl;
        }
    }
    std::cout<<"        </tr>"<<endl;
    std::cout<<"      </tbody>"<<endl;
    std::cout<<"    </table>"<<endl;
    std::cout<<"  </body>"<<endl;
    std::cout<<"</html>"<<endl;
}