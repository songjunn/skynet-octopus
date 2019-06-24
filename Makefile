BIN_PATH ?= ./bin
LIB_PATH ?= ./bin/lib

CC := gcc

CFLAGS := -std=gnu99 -g -ggdb -O0 -w -m64 -DDEBUG
EXPORT := -Wl,-E
LIBS := -lrt -ldl -lpthread
SHARED := -shared -fPIC

SKYNET_SRC = skynet_service.c skynet_mq.c \
	skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
	skynet_logger.c skynet_socket.c socket_server.c

SKYNET_EXE_SRC = skynet_main.c

all : \
	$(LIB_PATH) \
	$(BIN_PATH)/libskynet.so \
	$(BIN_PATH)/skynet \
	$(LIB_PATH)/libgate.so \
	$(LIB_PATH)/liblogger.so \
	$(LIB_PATH)/libharbor.so \
	$(LIB_PATH)/libpython.so \
	$(LIB_PATH)/libhttp.so \
	#$(LIB_PATH)/libmongo.so \

$(LIB_PATH) :
	mkdir $(LIB_PATH)

$(BIN_PATH)/libskynet.so : $(foreach v, $(SKYNET_SRC), skynet-src/$(v))
	$(CC) $(CFLAGS) $(SHARED) $(LIBS) $^ -o $@ -Iskynet-src

$(BIN_PATH)/skynet : $(foreach v, $(SKYNET_EXE_SRC), skynet-src/$(v))
	$(CC) $(CFLAGS) $(EXPORT) $(LIBS) $^ -o $@ -Iskynet-src -lskynet -L$(BIN_PATH)

$(LIB_PATH)/libgate.so : service-src/service_gate.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

$(LIB_PATH)/liblogger.so : service-src/service_logger.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

$(LIB_PATH)/libharbor.so : service-src/service_harbor.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

$(LIB_PATH)/libpython.so : service-src/service_python.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src -I/usr/local/python3/include/python3.7m -lpython3

$(LIB_PATH)/libhttp.so : service-src/service_http.c service-src/http/http_parser.c service-src/http/http_proxy.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src -Iservice-src/http

$(LIB_PATH)/libmongo.so : service-src/service_mongo.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src -I3rd/mongoc/include/libbson-1.0 -I3rd/mongoc/include/libmongoc-1.0 -L3rd/mongoc/lib -lbson-1.0 -lmongoc-1.0

clean :
	rm -f $(BIN_PATH)/skynet $(LIB_PATH)/*.so

cleanall:
	clean

default:
	$(MAKE) all
