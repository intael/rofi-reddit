#include "curl/mock_easy.h"
#include "fixtures.h"
#include "mock_curl_wrappers.h"
#include "reddit.h"
#include "unity.h"

static RedditApp* app;
static struct response_buffer* some_response;

void setUp(void) {
    app = fake_app();
    some_response = fake_response("{\"access_token\":\"the reddit app token\"}");
}

void tearDown(void) {
    free(app);
    free(some_response);
}

// TODO: It would be interesting to test here that curl_easy_setopt is called with the right stuff,
// but CMock seems unable to mock it. So in the meanwhile we're marking that as non-mockable to CMock (see strippables
// in CMock's curl config). Would be good to find another way.

void test_happy_path(void) {
    new_response_buffer_ExpectAndReturn(some_response);

    curl_easy_reset_Expect(app->http_client);
    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long fake_response_code = (long)HTTP_OK;
    get_response_status_ExpectAndReturn(app->http_client, &fake_response_code);
    http_status_code_from_ExpectAndReturn(fake_response_code, HTTP_OK);
    free_response_buffer_Expect(some_response);

    const struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    TEST_ASSERT_EQUAL(some_response, response->response_buffer);
}

void test_fetch_reddit_access_token_non_200_response_status(void) {
    new_response_buffer_ExpectAndReturn(some_response);
    curl_easy_reset_Expect(app->http_client);

    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long fake_response_code = (long)HTTP_BAD_REQUEST;
    get_response_status_ExpectAndReturn(app->http_client, &fake_response_code);
    http_status_code_from_ExpectAndReturn(fake_response_code, HTTP_BAD_REQUEST);
    free_response_buffer_Expect(some_response);

    const struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    TEST_ASSERT_EQUAL(some_response, response->response_buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path);
    RUN_TEST(test_fetch_reddit_access_token_non_200_response_status);
    return UNITY_END();
}
