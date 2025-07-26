#include "curl/mock_easy.h"
#include "fixtures.h"
#include "mock_curl_wrappers.h"
#include "reddit.h"
#include "unity.h"

static RedditApp* app;

void setUp(void) {
    app = fake_app();
}

void tearDown(void) {
    free(app);
}

void test_fetch_reddit_access_token_happy_path(void) {
    struct response* const VALID_JSON_RESPONSE = fake_response("{\"access_token\":\"LOLTOKEN\"}");

    new_response_ExpectAndReturn(VALID_JSON_RESPONSE);

    curl_easy_reset_Expect(app->http_client);
    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = malloc(sizeof(long));
    *fake_response_code = 200;
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_OK);
    free_response_Expect(VALID_JSON_RESPONSE);

    const struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    TEST_ASSERT_EQUAL_STRING(((RedditAccessToken*)response->data)->token, "LOLTOKEN");
    free(VALID_JSON_RESPONSE);
}

void test_fetch_reddit_access_token_malformed_response_json_payload(void) {
    struct response* malformed_response_json_payload =
        fake_response("{access_token\":\"LOLTOKEN\"}"); // misses a double quote

    new_response_ExpectAndReturn(malformed_response_json_payload);

    curl_easy_reset_Expect(app->http_client);
    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = malloc(sizeof(long));
    *fake_response_code = 200;
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_OK);
    free_response_Expect(malformed_response_json_payload);

    TEST_ASSERT_NULL(fetch_reddit_access_token_from_api(app)->data);
    free(malformed_response_json_payload);
}

void test_fetch_reddit_access_token_non_200_response_status(void) {
    struct response* valid_json_response = fake_response("{\"access_token\":\"LOLTOKEN\"}");

    curl_easy_reset_Expect(app->http_client);
    new_response_ExpectAndReturn(valid_json_response);

    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = malloc(sizeof(long));
    *fake_response_code = 400;
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_BAD_REQUEST);
    free_response_Expect(valid_json_response);

    TEST_ASSERT_NULL(fetch_reddit_access_token_from_api(app)->data);
    free(valid_json_response);
}

void test_fetch_reddit_access_token_bad_curl_response_status(void) {
    struct response* valid_json_response = fake_response("{\"access_token\":\"LOLTOKEN\"}");

    curl_easy_reset_Expect(app->http_client);
    new_response_ExpectAndReturn(valid_json_response);

    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_RECV_ERROR);

    long* fake_response_code = malloc(sizeof(long));
    *fake_response_code = 200;
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_OK);
    free_response_Expect(valid_json_response);

    TEST_ASSERT_NULL(fetch_reddit_access_token_from_api(app)->data);
    free(valid_json_response);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fetch_reddit_access_token_happy_path);
    RUN_TEST(test_fetch_reddit_access_token_malformed_response_json_payload);
    RUN_TEST(test_fetch_reddit_access_token_non_200_response_status);
    RUN_TEST(test_fetch_reddit_access_token_bad_curl_response_status);
    return UNITY_END();
}
