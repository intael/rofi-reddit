#include "curl_wrappers.h"
#include "memory.h"
#include <curl/curl.h>
#include <stdlib.h>

static const int INITIAL_RESPONSE_BUFFER_SIZE = (256 * 1024);

struct response* new_response() {
    struct response* resp = (struct response*)LOG_ERR_MALLOC(struct response, 1);
    resp->buffer = LOG_ERR_MALLOC(char, INITIAL_RESPONSE_BUFFER_SIZE);
    resp->size = 0;
    return resp;
}

void free_response(struct response* resp) {
    free(resp->buffer);
    free(resp);
}

long* get_response_status(CURL* client) {
    long* http_code = (long*)malloc(sizeof(long));
    curl_easy_getinfo(client, CURLINFO_RESPONSE_CODE, http_code);
    return http_code;
}

enum http_status_code http_status_code_from(long code) {
    switch (code) {
    case 200L:
        return HTTP_OK;
    case 400L:
        return HTTP_BAD_REQUEST;
    case 401L:
        return HTTP_UNAUTHORIZED;
    case 403L:
        return HTTP_FORBIDDEN;
    case 404L:
        return HTTP_NOT_FOUND;
    default:
        return (enum http_status_code)code;
    }
}
