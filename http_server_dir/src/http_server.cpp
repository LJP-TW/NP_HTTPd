//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>
#include <sys/wait.h>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/signal_set.hpp>

using boost::asio::ip::tcp;
using namespace std;

// Currently only supports GET method
enum HTTP_METHOD {
  GET = 0,
  UNKNOWN,
};

static unordered_map<string, HTTP_METHOD> const str_http_method_table = { 
  {"GET", HTTP_METHOD::GET} 
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
      [this, self](boost::system::error_code ec, std::size_t length)
      {
        if (!ec) {
          vector<string> lines;
          vector<string> firstline;
          HTTP_METHOD method;
          string http_version;

          boost::split(lines, data_, boost::is_any_of("\r\n"), boost::token_compress_on);

          if (lines.size() < 1) {
            cerr << "[x] Warning: Weird request" << endl;
            return;
          }

          // Parse "<Method> <URI> <HTTP_VERSION>"
          boost::split(firstline, lines[0], boost::is_any_of(" "), boost::token_compress_on);
          lines.erase(lines.begin());

          if (firstline.size() != 3) {
            cerr << "[x] Warning: Parsing error" << endl;
            for (auto word : firstline) {
              cerr << "\t[x] " << word << endl;
            }
            return;
          }

          {
            auto it = str_http_method_table.find(firstline[0]);
            if (it != str_http_method_table.end()) {
              method = it->second;
              method_ = firstline[0];
            } else { 
              method = HTTP_METHOD::UNKNOWN;
            }
          }

          uri_ = firstline[1];
          http_version = firstline[2];

          cout << "[>] Method: " << method << endl;
          cout << "[>] URI   : " << uri_ << endl;
          cout << "[>] Ver   : " << http_version << endl;

          // Parse Headers
          headers_.clear();

          for (auto line : lines) {
            // Parse "<Header>: <Value>"
            auto split_idx = line.find(": ");
            
            if (std::string::npos != split_idx) {
              string key = line.substr(0, split_idx);
              string value = line.substr(split_idx + 2);
              if (key == "Host") {
                headers_["HTTP_HOST"] = value;
              }
            }
          }

          headers_["REQUEST_METHOD"]  = method_;
          headers_["REQUEST_URI"]     = uri_;
          headers_["SERVER_PROTOCOL"] = http_version;
          headers_["SERVER_ADDR"]     = socket_.local_endpoint().address().to_string();
          headers_["SERVER_PORT"]     = to_string(socket_.local_endpoint().port());
          headers_["REMOTE_ADDR"]     = socket_.remote_endpoint().address().to_string();
          headers_["REMOTE_PORT"]     = to_string(socket_.remote_endpoint().port());

          {
            // Parse "URI?QUERY_STRING"
            auto split_idx = uri_.find("?");
            
            if (std::string::npos != split_idx) {
              string query = uri_.substr(split_idx + 1);
              uri_ = uri_.substr(0, split_idx);
              headers_["QUERY_STRING"] = query;
            } else {
              headers_["QUERY_STRING"] = "";
            }
          }

          // Process URI
          if (uri_[0] == '/') {
            uri_.erase(0, 1);
          }
          
          // Execute CGI
          if (uri_.length() >= 5 && uri_.substr(uri_.length() - 4) == ".cgi") {
            do_execute_cgi();
          }
        }
      });
  }

  void do_execute_cgi()
  {
    auto self(shared_from_this());
    char status_200[] = "HTTP/1.1 200 OK\r\n";
    boost::asio::async_write(socket_, boost::asio::buffer(status_200, strlen(status_200)),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
          string cgi = "./" + uri_;
          cout << "[!] CGI: " << cgi.c_str() << endl;

          if (fork() != 0) {
            socket_.close();
          } else {
            int sock = socket_.native_handle();

            clearenv();

            for (auto& h : headers_) {
              setenv(h.first.c_str(), h.second.c_str(), 1);
            }

            dup2(sock, STDIN_FILENO);
            dup2(sock, STDOUT_FILENO);

            socket_.close();

            if (execlp(cgi.c_str(), cgi.c_str(), NULL) < 0) {
              std::cout << "Content-type:text/html\r\n\r\n<h1>FAIL</h1>";
            }
            exit(0);
          }

          do_read();
        }
      });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  string uri_;
  string method_;
  unordered_map<string, string> headers_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      signal_(io_context, SIGCHLD)
  {
    wait_for_signal();
    do_accept();
  }

private:
  void wait_for_signal()
  {
    signal_.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        if (acceptor_.is_open()) {
          int status = 0;
          while (waitpid(-1, &status, WNOHANG) > 0) {}

          wait_for_signal();
        }
      });
  }

  void do_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (!ec) {
          std::make_shared<session>(std::move(socket))->start();
        }

        do_accept();
      });
  }

  tcp::acceptor acceptor_;
  boost::asio::signal_set signal_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2) {
      std::cerr << "Usage: http_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}