AUTOMAKE_OPTIONS = foreign 

bin_PROGRAMS = skynet 

skynet_SOURCES = \
  src/skynet.h \
  src/skynet_atomic.h \
  src/skynet_config.c \
  src/skynet_config.h \
  src/skynet_logger.c \
  src/skynet_logger.h \
  src/skynet_main.c \
  src/skynet_mq.c \
  src/skynet_mq.h \
  src/skynet_server.c \
  src/skynet_server.h \
  src/skynet_service.c \
  src/skynet_service.h \
  src/skynet_socket.c \
  src/skynet_socket.h \
  src/skynet_spinlock.h \
  src/skynet_timer.c \
  src/skynet_timer.h \
  src/socket_epoll.h \
  src/socket_kqueue.h \
  src/socket_poll.h \
  src/socket_server.c \
  src/socket_server.h

skynet_includedir = $(prefix)/include
skynet_include_HEADERS = \
  src/skynet.h

skynet_CFLAGS = -g -ggdb -O2 -Wall -m64 -Wl,-E
skynet_LDFLAGS = -lrt -ldl -lm -lpthread

