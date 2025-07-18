#include "curl_wrappers.h"
#include "reddit.h"
#include <curl/easy.h>
#include <string.h>

RedditApp* fake_app() {
    RedditApp* app = malloc(sizeof(RedditApp));
    struct app_auth* auth = malloc(sizeof(struct app_auth));
    auth->client_name = "lol";
    auth->client_id = "id";
    auth->client_secret = "sicrit";
    app->auth = auth;
    app->http_client = curl_easy_init();
    return app;
}

struct response* fake_response(char* buffer) {
    struct response* fake_resp = malloc(sizeof(struct response));
    fake_resp->buffer = buffer;
    fake_resp->size = strlen(buffer);
    return fake_resp;
}
