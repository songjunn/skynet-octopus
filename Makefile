BIN_PATH ?= bin
LIB_PATH ?= bin/lib

CC := gcc -std=gnu99

CFLAGS := -g -ggdb -O0 -w -m64 -DDEBUG
LDFLAGS := -Wl,-E
LIBS := -lrt -ldl -lm -lpthread
SHARED := -shared -fPIC
SKYNET_DEFINES :=-DFD_SETSIZE=4096

SKYNET_SRC = skynet_main.c skynet_service.c skynet_mq.c \
  skynet_server.c skynet_timer.c skynet_config.c skynet_harbor.c \
  skynet_logger.c skynet_socket.c socket_server.c

SERVICE = logger gate harbor

all : \
  $(LIB_PATH) \
  $(BIN_PATH)/skynet \
  $(foreach v, $(SERVICE), $(LIB_PATH)/lib$(v).so) \

$(LIB_PATH) :
	mkdir $(LIB_PATH)

$(BIN_PATH)/skynet : $(foreach v, $(SKYNET_SRC), skynet-src/$(v)) 
	$(CC) $(CFLAGS) -o $@ $^ -Iskynet-src $(LDFLAGS) $(LIBS) $(SKYNET_DEFINES)

define CSERVICE_TEMP
  $$(LIB_PATH)/lib$(1).so : service-src/service_$(1).c | $$(LIB_PATH)
	$$(CC) $$(CFLAGS) $$(SHARED) $$< -o $$@ -Iskynet-src
endef
$(foreach v, $(SERVICE), $(eval $(call CSERVICE_TEMP,$(v))))

clean :
	rm -f $(BIN_PATH)/skynet $(LIB_PATH)/*.so

cleanall: clean

default:
	$(MAKE) all
