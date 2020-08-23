BIN_PATH ?= ./bin
LIB_PATH ?= ./bin/lib

SKYNET_INC ?= skynet-src
SERVICE_INC ?= service-src/util

CC := gcc

CFLAGS := -std=gnu99 -g -ggdb -O0 -w -m64 -I$(SKYNET_INC) -I$(SERVICE_INC)
EXPORT := -Wl,-E
LIBS := -lrt -ldl -lpthread
SHARED := -shared -fPIC

SKYNET_SRC = skynet_service.c skynet_mq.c \
	skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
	skynet_logger.c skynet_socket.c socket_server.c skynet_malloc.c

SKYNET_EXE_SRC = skynet_main.c

all : \
	$(LIB_PATH) \
	$(BIN_PATH)/libskynet.so \
	$(BIN_PATH)/skynet \
	$(LIB_PATH)/liblogger.so \
	$(LIB_PATH)/libharbor.so \
	$(LIB_PATH)/libsimdb.so \
	$(LIB_PATH)/libhttp.so \
	$(LIB_PATH)/libgate.so \
	$(LIB_PATH)/libgatews.so \
	$(LIB_PATH)/libsnlua.so \
	$(LIB_PATH)/libmongo.so \
	#$(LIB_PATH)/libpython.so \

$(LIB_PATH) :
	mkdir $(LIB_PATH)

$(BIN_PATH)/libskynet.so : $(foreach v, $(SKYNET_SRC), skynet-src/$(v))
	$(CC) $(CFLAGS) $(SHARED) $(LIBS) $^ -o $@

$(BIN_PATH)/skynet : $(foreach v, $(SKYNET_EXE_SRC), skynet-src/$(v))
	$(CC) $(CFLAGS) $(EXPORT) $(LIBS) $^ -o $@ -lskynet -L$(BIN_PATH)

$(LIB_PATH)/liblogger.so : service-src/service_logger.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(LIB_PATH)/libharbor.so : service-src/service_harbor.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(LIB_PATH)/libsimdb.so : service-src/service_simdb.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(LIB_PATH)/libhttp.so : service-src/service_http.c service-src/http/http_parser.c service-src/http/http_proxy.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iservice-src/http

$(LIB_PATH)/libgate.so : service-src/service_gate.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(LIB_PATH)/libgatews.so : service-src/service_gatews.c service-src/websocket/websocket.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iservice-src/websocket

$(LIB_PATH)/libmongo.so : service-src/service_mongo.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iservice-src/3rd/mongoc/include/libbson-1.0 -Iservice-src/3rd/mongoc/include/libmongoc-1.0 -Lservice-src/3rd/mongoc/lib -lbson-1.0 -lmongoc-1.0

$(LIB_PATH)/libpython.so : service-src/service_python.c
	#$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iservice-src/3rd/pypy3/include -Lservice-src/3rd/pypy3/bin -lpypy3-c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -I/usr/include/python3.6m -lpython3.6m

$(LIB_PATH)/libsnlua.so : service-src/service_snlua.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iservice-src/3rd/lua/include -Lservice-src/3rd/lua/lib -llua

clean :
	rm -f $(BIN_PATH)/skynet  $(BIN_PATH)/libskynet.so $(LIB_PATH)/*.so

cleanall:
	clean

default:
	$(MAKE) all
