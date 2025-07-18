#include "reddit.h"
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

#endif
