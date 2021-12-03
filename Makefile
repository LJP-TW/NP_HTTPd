CXX=g++
CXXFLAGS=-std=c++11 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

HTTP_SERVER = http_server
HTTP_SERVER_SRC = ./http_server_dir/src

CONSOLE_CGI = console.cgi

all: $(HTTP_SERVER) $(CONSOLE_CGI)
	
$(HTTP_SERVER):
	@echo "Compiling" $@ "..."
	$(CXX) $(HTTP_SERVER_SRC)/http_server.cpp -o $@ $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

$(CONSOLE_CGI):
	@echo "Compiling" $@ "..."

clean:
	rm -f $(HTTP_SERVER)
	rm -f $(CONSOLE_CGI)