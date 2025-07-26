#include <curl/curl.h>

#ifndef _CURLWRAPPERS__H
#define _CURLWRAPPERS_H

struct response {
  char *buffer;
  size_t size;
};

struct response *new_response();

void free_response(struct response *resp);

long *get_response_status(CURL *client);

enum http_status_code {
  HTTP_OK = 200,
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED = 401,
  HTTP_FORBIDDEN = 403,
  HTTP_NOT_FOUND = 404
};

enum http_status_code http_status_code_from(long code);

#endif
