BUILD_PATH ?= .

CFLAGS := -g -ggdb -O0 -Wall -m64 -DDEBUG
LDFLAGS := -Wl,-E
LIBS := -lrt -ldl -lm -lpthread

SKYNET_SRC = skynet_main.c skynet_service.c skynet_mq.c \
  skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
  skynet_logger.c skynet_socket.c socket_server.c

all : \
  $(BUILD_PATH)/skynet 

$(BUILD_PATH)/skynet : $(foreach v, $(SKYNET_SRC), src/$(v)) 
	$(CC) $(CFLAGS) -o $@ $^ -Isrc $(LDFLAGS) $(LIBS)

clean :
	rm -f $(BUILD_PATH)/skynet

cleanall: clean

default:
	$(MAKE) all
