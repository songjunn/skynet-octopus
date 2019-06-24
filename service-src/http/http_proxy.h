#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include "http_parser.h"

typedef struct http_proxy http_proxy;

struct http_proxy {
  int id;
  int type;
  int complete;

  char * chunk; //recvd data
  size_t chunk_ptr; 

  char * url; //parsed data
  size_t url_ptr;

  char * body; //parsed data
  size_t body_ptr;
};

http_proxy * proxy_create(int type, int id, const char * addr, size_t sz);
void proxy_destroy(http_proxy * proxy);
void proxy_reset(http_proxy * proxy);
int proxy_parse(http_proxy * proxy, const char * message, size_t sz);

#endif //HTTP_PROXY_H
