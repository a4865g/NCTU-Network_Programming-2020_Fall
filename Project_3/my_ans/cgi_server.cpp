#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#define MAX_LENGTH 4096

using boost::asio::ip::tcp;

using namespace std;

boost::asio::io_service io_service;

class Host
{
public:
    string h;
    string p;
    string f;
};

vector<string> split_trim(string, const string &);
void handle_http_line(vector<string> &, string);
void http_encode(string &);

class Console
{
public:
    static Console &getInstance()
    {
        static Console instance;
        return instance;
    }
    array<Host, 5> host; //max client
    string print_initial_html();
};

vector<string> split_trim(string line, const string &E_char = "\t\n ")
{
    vector<string> handle_line;
    boost::trim(line); //DELETE Begin and End 'space'

    //can see https://kheresy.wordpress.com/2010/12/21/boost_string_algorithms_split/
    boost::split(handle_line, line, boost::is_any_of(E_char), boost::token_compress_on);

    //handle every element
    for (auto &i : handle_line)
    {
        boost::trim(i);
    }
    return handle_line;
}

class Parser
{
public:
    static Parser getInstance()
    {
        static Parser instance;
        return instance;
    }
    static vector<string> split_trim(string line, const string &E_char = "\t\n ")
    {
        vector<string> handle_line;
        boost::trim(line); //DELETE Begin and End 'space'

        //can see https://kheresy.wordpress.com/2010/12/21/boost_string_algorithms_split/
        boost::split(handle_line, line, boost::is_any_of(E_char), boost::token_compress_on);

        //handle every element
        for (auto &i : handle_line)
        {
            boost::trim(i);
        }
        return handle_line;
    }
    map<string, string> parse_http(string request);
};

class Session_C : public std::enable_shared_from_this<Session_C>
{
public:
    class Session : public std::enable_shared_from_this<Session>
    {
    private:
    public:
        shared_ptr<Session_C> _ptr;
        tcp::socket tcp_socket{io_service};
        tcp::resolver resolver{io_service};
        array<char, MAX_LENGTH> _data;
        string file_name;
        string session;
        std::fstream file;
        void do_read()
        {
            auto self(shared_from_this());

            tcp_socket.async_receive(
                boost::asio::buffer(_data),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec)
                    {
                        string data(_data.begin(), _data.begin() + length);
                        string out_shell;
                        http_encode(data);
                        out_shell = "<script>document.getElementById('s" + session + "').innerHTML+=\"" + data + "\";</script>\n";
                        boost::asio::write(
                            _ptr->_socket,
                            boost::asio::buffer(out_shell));

                        if (data.find("% ") != string::npos)
                        {
                            string cmd, tmp, cmd_in;
                            getline(file, cmd);
                            tmp = cmd;
                            http_encode(tmp);
                            cmd_in = cmd_in + "<script>document.getElementById('s" + session + "').innerHTML += '<b>" + tmp + "&NewLine;</b>';</script>\n";
                            boost::asio::write(_ptr->_socket,
                                               boost::asio::buffer(cmd_in));
                            tcp_socket.write_some(boost::asio::buffer(cmd + '\n'));
                        }
                        do_read();
                    }
                    else
                    {
                        //cout<<"read ec:"<<ec<<endl;
                    }
                });
        }
        Session(const shared_ptr<Session_C> &ptr, string session, string file_name)
            : _ptr(ptr), session(session), file("./test_case/" + file_name, std::ios::in)
        {
            //file.open("./test_case/" + file_name, std::ios::in);
            if (!file.is_open())
            {
                /* cout << "<script>document.getElementById('s" << session << "').innerHTML += '<b>"
                     << "File Not Open!"
                     << "&NewLine;</b>';</script>" << endl;*/
            }
        }
        void connect_handler()
        {
            do_read();
        }

        void resolve_handler(tcp::resolver::iterator it)
        {
            auto self(shared_from_this());
            tcp_socket.async_connect(*it, [this, self](boost::system::error_code ec) {
                if (!ec)
                    connect_handler();
            });
        }
        void resolve(string h, string p)
        {
            auto self(shared_from_this());
            tcp::resolver::query _query{h, p};
            resolver.async_resolve(_query,
                                   [this, self](const boost::system::error_code &ec,
                                                tcp::resolver::iterator it) {
                                       if (!ec)
                                       {
                                           resolve_handler(it);
                                       }
                                   });
        }
    };

    Session_C(tcp::socket socket) : _socket(std::move(socket)) {}
    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        _socket.async_read_some(
            boost::asio::buffer(_data, MAX_LENGTH),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec)
                {
                    auto http_result = Parser::getInstance().parse_http(string(_data, _data + length));
                    //cout << endl;
                    auto target = http_result["TARGET"];
                    string str;
                    if (target == "/console.cgi")
                    {
                        str = get_console(http_result["QUERY_STRING"]);
                        do_write(str);
                        const auto &host = Console::getInstance().host;
                        for (int i = 0; i < host.size(); ++i)
                        {
                            if (!(host.at(i).f.empty() && host.at(i).h.empty() && host.at(i).p.empty()))
                            {
                                std::make_shared<Session>(shared_from_this(), to_string(i), host.at(i).f)->resolve(host.at(i).h, host.at(i).p);
                            }
                        }
                    }
                    if (target == "/panel.cgi")
                    {
                        str = get_panel();
                        do_write(str);
                    }
                }
            });
    }

    void do_write(string s)
    {
        auto self(shared_from_this());
        boost::asio::async_write(
            _socket, boost::asio::buffer(s.c_str(), s.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec)
                {
                }
            });
    }

    string get_console(string s)
    {
        auto self(shared_from_this());
        vector<string> query = split_trim(s, "&");
        vector<string>::iterator it;
        for (it = query.begin(); it != query.end(); it++)
        {
            string key = (*it).substr(0, (*it).find('='));
            string value = (*it).substr(key.size() + 1);
            if (value.empty() == false)
            {
                if (key[0] == 'h')
                {
                    Console::getInstance().host.at((int)(key[1] - '0')).h = value;
                }
                else if (key[0] == 'p')
                {
                    Console::getInstance().host.at((int)(key[1] - '0')).p = value;
                }
                else if (key[0] == 'f')
                {
                    Console::getInstance().host.at((int)(key[1] - '0')).f = value;
                }
            }
        }
        string str = "HTTP/1.1 200 OK\r\n";
        str = str + Console::getInstance().print_initial_html();
        return str;
    }

    string get_panel()
    {
        auto self(shared_from_this());
        string s = "HTTP/1.1 200 OK\r\n"
                   "Content-type: text/html\r\n\r\n";
        string s1 =
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "  <head>\n"
            "    <title>NP Project 3 Panel</title>\n"
            "    <link\n"
            "      rel=\"stylesheet\"\n"
            "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
            "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
            "      crossorigin=\"anonymous\"\n"
            "    />\n"
            "    <link\n"
            "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
            "      rel=\"stylesheet\"\n"
            "    />\n"
            "    <link\n"
            "      rel=\"icon\"\n"
            "      type=\"image/png\"\n"
            "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n"
            "    />\n"
            "    <style>\n"
            "      * {\n"
            "        font-family: 'Source Code Pro', monospace;\n"
            "      }\n"
            "    </style>\n"
            "  </head>\n"
            "  <body class=\"bg-secondary pt-5\">"
            "<form action=\"console.cgi\" method=\"GET\">\n"
            "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n"
            "        <thead class=\"thead-dark\">\n"
            "          <tr>\n"
            "            <th scope=\"col\">#</th>\n"
            "            <th scope=\"col\">Host</th>\n"
            "            <th scope=\"col\">Port</th>\n"
            "            <th scope=\"col\">Input File</th>\n"
            "          </tr>\n"
            "        </thead>\n"
            "        <tbody>";
        boost::format fmt(
            "          <tr>\n"
            "            <th scope=\"row\" class=\"align-middle\">Session %1%</th>\n"
            "            <td>\n"
            "              <div class=\"input-group\">\n"
            "                <select name=\"h%2%\" class=\"custom-select\">\n"
            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option>\n"
            "                </select>\n"
            "                <div class=\"input-group-append\">\n"
            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
            "                </div>\n"
            "              </div>\n"
            "            </td>\n"
            "            <td>\n"
            "              <input name=\"p%2%\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
            "            </td>\n"
            "            <td>\n"
            "              <select name=\"f%2%\" class=\"custom-select\">\n"
            "                <option></option><option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>\n"
            "              </select>\n"
            "            </td>\n"
            "          </tr>");

        for (int i = 0; i < 5; i++)
        {
            s1 = s1 + (fmt % (i + 1) % i).str();
        }
        s1 = s1 +
             "          <tr>\n"
             "            <td colspan=\"3\"></td>\n"
             "            <td>\n"
             "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n"
             "            </td>\n"
             "          </tr>\n"
             "        </tbody>\n"
             "      </table>\n"
             "    </form>\n"
             "  </body>\n"
             "</html>";

        s = s + s1;
        return s;
    }
    tcp::socket _socket;
    char _data[MAX_LENGTH];
};

class server
{
private:
    void do_accept()
    {
        _acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec)
                {
                    std::make_shared<Session_C>(std::move(socket))->start();
                }
                do_accept();
            });
    }
    tcp::acceptor _acceptor;

public:
    server(unsigned short port) : _acceptor(io_service, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }
};

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        server s(atoi(argv[1]));
        io_service.run();
    }
    catch (exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

void http_encode(string &d)
{
    boost::algorithm::replace_all(d, "&", "&amp;");
    boost::algorithm::replace_all(d, ">", "&gt;");
    boost::algorithm::replace_all(d, "<", "&lt;");
    boost::algorithm::replace_all(d, "\"", "&quot;");
    boost::algorithm::replace_all(d, "\'", "&apos;");

    boost::algorithm::replace_all(d, "\n", "&#13;");
    boost::algorithm::replace_all(d, "\r", "");
}

string Console::print_initial_html()
{
    string s =
        "Content-type: text/html\r\n\r\n"
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <meta charset=\"UTF-8\" />\n"
        "    <title>NP Project 3 Console</title>\n"
        "    <link\n"
        "      rel=\"stylesheet\"\n"
        "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
        "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
        "      crossorigin=\"anonymous\"\n"
        "    />\n"
        "    <link\n"
        "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "      rel=\"stylesheet\"\n"
        "    />\n"
        "    <link\n"
        "      rel=\"icon\"\n"
        "      type=\"image/png\"\n"
        "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
        "    />\n"
        "    <style>\n"
        "      * {\n"
        "        font-family: 'Source Code Pro', monospace;\n"
        "        font-size: 1rem !important;\n"
        "      }\n"
        "      body {\n"
        "        background-color: #212529;\n"
        "      }\n"
        "      pre {\n"
        "        color: #cccccc;\n"
        "      }\n"
        "      b {\n"
        "        color: #01b468;\n"
        "      }\n"
        "    </style>\n"
        "  </head>\n"
        "  <body>\n"
        "    <table class=\"table table-dark table-bordered\">\n"
        "      <thead>\n"
        "        <tr>\n";
    //     <th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>
    //    <th scope=\"col\">nplinux2.cs.nctu.edu.tw:5678</th>

    for (size_t i = 0; i < host.size(); i++)
    {
        if (!(host.at(i).f.empty() && host.at(i).h.empty() && host.at(i).p.empty()))
        {
            s = s + "          <th scope=\"col\">" + host.at(i).h + ":" + host.at(i).p + "</th>\n";
        }
    }
    s = s +
        "        </tr>\n"
        "      </thead>\n"
        "      <tbody>\n"
        "        <tr>\n";
    //cout<<"     <td><pre id=\"s0\" class=\"mb-0\"></pre></td>"endl;
    //cout<<"     <td><pre id=\"s1\" class=\"mb-0\"></pre></td>"endl;

    for (size_t i = 0; i < host.size(); i++)
    {
        if (!(host.at(i).f.empty() && host.at(i).h.empty() && host.at(i).p.empty()))
        {
            s = s + "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
        }
    }
    s = s +
        "        </tr>\n"
        "      </tbody>\n"
        "    </table>\n"
        "  </body>\n"
        "</html>\n";
    return s;
}

map<string, string> Parser::parse_http(string request)
{
    vector<string> http;
    handle_http_line(http, request);
    map<string, string> map_http;

    for (auto line : http)
    {
        string key = line.substr(0, line.find(':')); // get ":" before all
        string value = line.substr(key.size());      //get ":" after all
        if (key != line)
        {
            value = value.substr(2); //delete space
            if (key.front() == '\n')
            {
                key = key.substr(1); // delete    ('\n')
            }
            map_http.insert({key, value});
        }
        else
        {
            //trin_if:https://my.oschina.net/zmlblog/blog/131104
            boost::trim_if(line, boost::is_any_of("\n\r"));
            if (!line.empty())
            {
                vector<string> header = split_trim(line);
                map_http.insert({"METHOD", header.at(0)});
                map_http.insert({"URI", header.at(1)});
                //map_http.insert({"SERVER_PROTOCOL", header.at(2)});
                string target = header.at(1).substr(0, header.at(1).find('?'));
                map_http.insert({"TARGET", target});
                if (header.at(1) != target)
                {
                    map_http.insert({"QUERY_STRING", header.at(1).substr(target.size() + 1)});
                }
                else
                {
                    map_http.insert({"QUERY_STRING", ""});
                }
            }
        }
    }
    /*
    try
    {
        map_http.insert({"HTTP_HOST", map_http.at("Host")});
    }
    catch (exception &e)
    {
        cerr << e.what() << endl;
    }*/
    return map_http;
}

void handle_http_line(vector<string> &http, string line)
{
    string::iterator it_l, it_r;
    for (it_l = line.begin(), it_r = line.begin(); it_r != line.end(); it_r++)
    {
        if ((*it_r) == '\n')
        {
            string handle_line(it_l, it_r);
            if (handle_line.back() == '\r')
            { //remove \r
                handle_line.pop_back();
            }
            http.push_back(handle_line);
            it_l = it_r;
        }
    }
}