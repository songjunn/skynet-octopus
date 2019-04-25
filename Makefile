BIN_PATH ?= ./bin
LIB_PATH ?= ./bin/lib

CC := gcc

CFLAGS := -std=gnu99 -g -ggdb -O0 -w -m64 -DDEBUG
EXPORT := -Wl,-E
LIBS := -lrt -ldl -lpthread
SHARED := -shared -fPIC
SKYNET_DEFINES :=-DFD_SETSIZE=4096

SKYNET_SRC = skynet_main.c skynet_service.c skynet_mq.c \
	skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
	skynet_logger.c skynet_socket.c socket_server.c

all : \
	$(LIB_PATH) \
	$(BIN_PATH)/skynet \
	$(LIB_PATH)/libgate.so \
	$(LIB_PATH)/liblogger.so \
	$(LIB_PATH)/libharbor.so \

$(LIB_PATH) :
	mkdir $(LIB_PATH)

$(BIN_PATH)/skynet : $(foreach v, $(SKYNET_SRC), skynet-src/$(v))
	$(CC) $(CFLAGS) $(EXPORT) $(LIBS) $(SKYNET_DEFINES) $^ -o $@ -Iskynet-src

$(LIB_PATH)/libgate.so : service-src/service_gate.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

$(LIB_PATH)/liblogger.so : service-src/service_logger.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

$(LIB_PATH)/libharbor.so : service-src/service_harbor.c
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iskynet-src

clean :
	rm -f $(BIN_PATH)/skynet $(LIB_PATH)/*.so

cleanall:
	clean

default:
	$(MAKE) all
