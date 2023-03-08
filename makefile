INCLUDE = -I./src
# FLAGS = -O2 -DDBG_MACRO_DISABLE
# FLAGS = -g -DDBG_MACRO_DISABLE
LIBS = -lpthread #-pg
BUILD = ./build
SRC = ./src
INSTALLDIR = /usr/local/bin

all: $(BUILD)/webserver

$(BUILD)/webserver: $(BUILD)/main.o $(BUILD)/webserver.o $(BUILD)/epoller.o \
  $(BUILD)/http_conn.o $(BUILD)/http_request.o $(BUILD)/http_response.o $(BUILD)/logger.o \
  $(BUILD)/thread_pool.o $(BUILD)/scalable_buffer.o $(BUILD)/useful.o
	c++ $^ $(LIBS) -o $@

$(BUILD)/main.o: $(SRC)/main.cc $(SRC)/webserver/webserver.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/webserver.o: $(SRC)/webserver/webserver.cc $(SRC)/webserver/webserver.hh \
  $(SRC)/epoller/epoller.hh $(SRC)/expirer/expirer.hh $(SRC)/http_conn/http_conn.hh $(SRC)/http_request/http_request.hh \
  $(SRC)/http_response/http_response.hh $(SRC)/logger/logger.hh $(SRC)/scalable_buffer/scalable_buffer.hh \
  $(SRC)/thread_pool/thread_pool.hh $(SRC)/useful.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/epoller.o: $(SRC)/epoller/epoller.cc $(SRC)/epoller/epoller.hh \
  $(SRC)/logger/logger.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/http_conn.o: $(SRC)/http_conn/http_conn.cc $(SRC)/http_conn/http_conn.hh \
  $(SRC)/http_request/http_request.hh $(SRC)/http_response/http_response.hh \
  $(SRC)/logger/logger.hh $(SRC)/scalable_buffer/scalable_buffer.hh $(SRC)/useful.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/http_request.o: $(SRC)/http_request/http_request.cc $(SRC)/http_request/http_request.hh \
  $(SRC)/scalable_buffer/scalable_buffer.hh $(SRC)/logger/logger.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/http_response.o: $(SRC)/http_response/http_response.cc $(SRC)/http_response/http_response.hh \
  $(SRC)/http_request/http_request.hh $(SRC)/scalable_buffer/scalable_buffer.hh $(SRC)/logger/logger.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/logger.o: $(SRC)/logger/logger.cc $(SRC)/logger/logger.hh \
  $(SRC)/scalable_buffer/scalable_buffer.hh $(SRC)/thread_pool/thread_pool.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/thread_pool.o: $(SRC)/thread_pool/thread_pool.cc $(SRC)/thread_pool/thread_pool.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/scalable_buffer.o: $(SRC)/scalable_buffer/scalable_buffer.cc $(SRC)/scalable_buffer/scalable_buffer.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

$(BUILD)/useful.o: $(SRC)/useful.cc $(SRC)/useful.hh
	c++ $(INCLUDE) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)/*.o $(BUILD)/webserver

install:
	cp $(BUILD)/webserver $(INSTALLDIR)

uninstall:
	rm $(INSTALLDIR)/webserver
