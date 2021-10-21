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
        cout<<*it<<endl;
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
                        cout<<"<script>document.getElementById('s"<<session<<"').innerHTML+=\""<<data<<"\";</script>" << std::endl;
                        cout.flush();

                        if(data.find("% ")!=string::npos){
                            string cmd,tmp;
                            getline(file,cmd);
                            tmp=cmd;
                            http_encode(tmp);
                            cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<tmp<<"&NewLine;</b>';</script>" << std::endl;
                            cout.flush();
                            tcp_socket.write_some(buffer(cmd+'\n'));
                        }
                        do_read();
                    }else{
                        //cout<<"read ec:"<<ec<<endl;
                    }
                }
            );
        }
    Session(string session,string port,string file_name,tcp::resolver::query q)
                            :session(std::move(session)),q(std::move(q)),file_name(file_name){
        cout<<endl;
      file.open("./test_case/" + file_name, std::ios::in);
      if (!file.is_open()){
          cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<"File Not Open!"<<"&NewLine;</b>';</script>" << endl;
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
    try {
        char *query_string;
        query_string=getenv("QUERY_STRING");
        vector<string> query=split_trim(query_string,"&");
        vector<string>::iterator it;
        for(it=query.begin();it!=query.end();it++){
            string key=(*it).substr(0,(*it).find('='));
            string value=(*it).substr(key.size()+1);
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
                tcp::resolver::query q{host.at(i).h,host.at(i).p};
                std::make_shared<Session>(to_string(i),host.at(i).p,host.at(i).f,std::move(q))->resolve();
            }
        }

        service.run();

    } catch (exception& e) {
        cerr<<"Exception: " << e.what() << "\n";
    }
    return 0;
}

void Console::print_initial_html(){
    cout<<"Content-type: text/html\r\n\r\n";
    cout<<"<!DOCTYPE html>"<<endl;
    cout<<"<html lang=\"en\">"<<endl;
    cout<<"  <head>"<<endl;
    cout<<"    <meta charset=\"UTF-8\" />"<<endl;
    cout<<"    <title>NP Project 3 Console</title>"<<endl;
    cout<<"    <link"<<endl;
    cout<<"      rel=\"stylesheet\""<<endl;
    cout<<"      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""<<endl;
    cout<<"      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""<<endl;
    cout<<"      crossorigin=\"anonymous\""<<endl;
    cout<<"    />"<<endl;
    cout<<"    <link"<<endl;
    cout<<"      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""<<endl;
    cout<<"      rel=\"stylesheet\""<<endl;
    cout<<"    />"<<endl;
    cout<<"    <link"<<endl;
    cout<<"      rel=\"icon\""<<endl;
    cout<<"      type=\"image/png\""<<endl;
    cout<<"      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""<<endl;
    cout<<"    />"<<endl;
    cout<<"    <style>"<<endl;
    cout<<"      * {"<<endl;
    cout<<"        font-family: 'Source Code Pro', monospace;"<<endl;
    cout<<"        font-size: 1rem !important;"<<endl;
    cout<<"      }"<<endl;
    cout<<"      body {"<<endl;
    cout<<"        background-color: #212529;"<<endl;
    cout<<"      }"<<endl;
    cout<<"      pre {"<<endl;
    cout<<"        color: #cccccc;"<<endl;
    cout<<"      }"<<endl;
    cout<<"      b {"<<endl;
    cout<<"        color: #01b468;"<<endl;
    cout<<"      }"<<endl;
    cout<<"    </style>"<<endl;
    cout<<"  </head>"<<endl;
    cout<<"  <body>"<<endl;
    cout<<"    <table class=\"table table-dark table-bordered\">"<<endl;
    cout<<"      <thead>"<<endl;
    cout<<"        <tr>"<<endl;
     //     <th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>
      //    <th scope=\"col\">nplinux2.cs.nctu.edu.tw:5678</th>
    
    for(size_t i=0;i<host.size();i++){
        if(!(host.at(i).f.empty()&&host.at(i).h.empty()&&host.at(i).p.empty())){
            cout<<"          <th scope=\"col\">"+host.at(i).h+":"+host.at(i).p+"</th>"<<endl;
        }
    }
    cout<<"        </tr>"<<endl;
    cout<<"      </thead>"<<endl;
    cout<<"      <tbody>"<<endl;
    cout<<"        <tr>"<<endl;
    //cout<<"     <td><pre id=\"s0\" class=\"mb-0\"></pre></td>"endl;
    //cout<<"     <td><pre id=\"s1\" class=\"mb-0\"></pre></td>"endl;
    
    for(size_t i=0;i<host.size();i++){
        if(!(host.at(i).f.empty()&&host.at(i).h.empty()&&host.at(i).p.empty())){
            cout<<"          <td><pre id=\"s"+to_string(i)+"\" class=\"mb-0\"></pre></td>"<<endl;
        }
    }
    cout<<"        </tr>"<<endl;
    cout<<"      </tbody>"<<endl;
    cout<<"    </table>"<<endl;
    cout<<"  </body>"<<endl;
    cout<<"</html>"<<endl;
}