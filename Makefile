BUILD_PATH ?= .
SERVICE_PATH ?= service-libs

CFLAGS := -g -ggdb -O2 -Wall -m64 -DDEBUG -fPIC
LDFLAGS := -Wl,-E
LIBS := -lrt -ldl -lm -lpthread
SHARED := -shared

SKYNET_SRC = skynet_main.c skynet_service.c skynet_mq.c \
  skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
  skynet_logger.c skynet_socket.c socket_server.c

all : \
  $(SERVICE_PATH) \
  $(BUILD_PATH)/skynet \
  $(SERVICE_PATH)/harbor.so 

$(SERVICE_PATH) :
	mkdir $(SERVICE_PATH)

$(BUILD_PATH)/skynet : $(foreach v, $(SKYNET_SRC), src/$(v)) 
	$(CC) $(CFLAGS) -o $@ $^ -Isrc $(LDFLAGS) $(LIBS)

$(SERVICE_PATH)/harbor.so : service-src/service_harbor.c 
	$(CC) $(CFLAGS) $(LDFLAGS) $(SHARED) $^ -o $@ -Isrc

clean :
	rm -f $(BUILD_PATH)/skynet $(SERVICE_PATH)/*.so

cleanall: clean

default:
	$(MAKE) all
