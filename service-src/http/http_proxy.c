#include "http_proxy.h"
#include "http_encode.h"
#include "skynet.h"
#include <stdio.h>

int on_message_begin(http_parser* parser) {
  skynet_logger_debug(NULL, "***MESSAGE BEGIN***");
  return 0;
}

int on_headers_complete(http_parser* parser) {
  skynet_logger_debug(NULL, "***HEADERS COMPLETE***");
  return 0;
}

int on_message_complete(http_parser* parser) {
  skynet_logger_debug(NULL, "***MESSAGE COMPLETE***");
  http_proxy * proxy = (http_proxy *)parser->data;
  proxy->complete = 1;
  return 0;
}

int on_header_field(http_parser* parser, const char* at, size_t length) {
  skynet_logger_debug(NULL, "Header field: %.*s", (int)length, at);
  return 0;
}

int on_header_value(http_parser* parser, const char* at, size_t length) {
  skynet_logger_debug(NULL, "Header value: %.*s", (int)length, at);
  return 0;
}

int on_url(http_parser* parser, const char* at, size_t length) {
  http_proxy * proxy = (http_proxy *)parser->data;
  proxy->url = (char *) malloc(sizeof(char) * length);
  //proxy->url_ptr = length;
  //memcpy(proxy->url, at, length);
  proxy->url_ptr = urldecode(at, length, proxy->url);
  skynet_logger_debug(NULL, "Url: %.*s", (int)proxy->url_ptr, proxy->url);
  return 0;
}

int on_body(http_parser* parser, const char* at, size_t length) {
  http_proxy * proxy = (http_proxy *)parser->data;
  proxy->body = (char *) malloc(sizeof(char) * length);
  proxy->body_ptr = length;
  memcpy(proxy->body, at, length);
  skynet_logger_debug(NULL, "Body: %.*s", (int)proxy->body_ptr, proxy->body);
  return 0;
}

static http_parser_settings settings_null = 
{.on_message_begin = on_message_begin
  ,.on_header_field = on_header_field
  ,.on_header_value = on_header_value
  ,.on_url = on_url
  ,.on_status = 0
  ,.on_body = on_body
  ,.on_headers_complete = on_headers_complete
  ,.on_message_complete = on_message_complete
};

http_proxy * proxy_create(int type, int id, const char * addr, size_t sz) {
  http_proxy * proxy = (http_proxy *)malloc(sizeof(http_proxy));
  proxy->id = id;
  proxy->type = type;
  proxy->complete = 0;
  
  proxy->chunk = (char *) malloc(sizeof(char) * 20480);
  proxy->chunk_ptr = 0;

  proxy->url = NULL;
  proxy->url_ptr = 0;

  proxy->body = NULL;
  proxy->body_ptr = 0;

  //skynet_logger_debug(NULL, "[HTTP] create proxy, id=%d", proxy->id);
  return proxy;
}

void proxy_destroy(http_proxy * proxy) {
  //skynet_logger_debug(NULL, "[HTTP] destroy proxy, id=%d", proxy->id);
  if (proxy->body) free(proxy->body);
  if (proxy->url) free(proxy->url);
  free(proxy->chunk);
  free(proxy);
}

void proxy_reset(http_proxy * proxy) {
  //skynet_logger_debug(NULL, "[HTTP] reset proxy, id=%d", proxy->id);
  if (proxy->url) {
    free(proxy->url);
    proxy->url = NULL;
    proxy->url_ptr = 0;
  }
  if (proxy->body) {
    free(proxy->body);
    proxy->body = NULL;
    proxy->body_ptr = 0;
  }
  proxy->chunk_ptr = 0;
  proxy->complete = 0;
}

int proxy_parse(http_proxy * proxy, const char * message, size_t sz) {
  //skynet_logger_debug(NULL, "[HTTP] parse size:%d", sz);
  //skynet_logger_debug(NULL, "[HTTP] parse recv:%s", message);

  if (proxy->chunk_ptr + sz > 20480) {
    skynet_logger_error(NULL, "[HTTP] parse ptr:%d size:%d", proxy->chunk_ptr, sz);
    return -1;
  }

  memcpy(proxy->chunk + proxy->chunk_ptr, message, sz);
  proxy->chunk_ptr += sz;

  http_parser parser;
  parser.data = proxy;
  http_parser_init(&parser, (enum http_parser_type)proxy->type);

  int hparsed = http_parser_execute(&parser, &settings_null, proxy->chunk, proxy->chunk_ptr);
  if (proxy->complete) {
    return hparsed;
  }

  return 0;
}
