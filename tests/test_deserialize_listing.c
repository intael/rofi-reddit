
#include "memory.h"
#include "reddit.h"
#include "unity.h"
#include <jansson.h>

static void assert_listing_equal(const struct listing* expected, const struct listing* actual) {
    TEST_ASSERT_EQUAL_STRING(expected->title, actual->title);
    TEST_ASSERT_EQUAL_STRING(expected->selftext, actual->selftext);
    TEST_ASSERT_EQUAL_UINT32(expected->ups, actual->ups);
    TEST_ASSERT_EQUAL_STRING(expected->url, actual->url);
}

static void assert_listing_not_initialized(const struct listing* listing) {
    TEST_ASSERT_NULL(listing->title);
    TEST_ASSERT_NULL(listing->selftext);
    TEST_ASSERT_EQUAL_UINT32(0, listing->ups);
    TEST_ASSERT_NULL(listing->url);
}

static struct listing new_expected_listing(void) {
    struct listing l = {.title = "Test Title",
                        .selftext = "Test selftext",
                        .ups = 42,
                        .url = "https://www.reddit.com/r/test/comments/12345/test_title/"};
    return l;
}

static json_t* new_json(void) {
    json_t* data = json_object();
    json_object_set_new(data, "title", json_string("Test Title"));
    json_object_set_new(data, "selftext", json_string("Test selftext"));
    json_object_set_new(data, "ups", json_integer(42));
    json_object_set_new(data, "permalink", json_string("/r/test/comments/12345/test_title/"));

    json_t* listing = json_object();
    json_object_set_new(listing, "data", data);
    return listing;
}

void setUp(void) {
}

void tearDown(void) {
}

void test_happy_path(void) {
    struct listing* listing = LOG_ERR_MALLOC(struct listing, 1);
    json_t* json = new_json();
    deserialize_listing(json, listing, 0);
    struct listing expected = new_expected_listing();
    assert_listing_equal(&expected, listing);
    free_listing(listing);
    json_decref(json);
    ;
}

void test_no_data_key(void) {
    struct listing* listing = calloc(1, sizeof(struct listing));
    json_t* json = new_json();
    json_object_del(json, "data");
    deserialize_listing(json, listing, 0);
    assert_listing_not_initialized(listing);
    free_listing(listing);
    json_decref(json);
}

void test_no_title_key(void) {
    struct listing* listing = calloc(1, sizeof(struct listing));
    json_t* json = new_json();
    json_object_del(json_object_get(json, "data"), "title");
    deserialize_listing(json, listing, 0);
    assert_listing_not_initialized(listing);
    free_listing(listing);
    json_decref(json);
}

void test_nullable_keys_are_missing(void) {
    struct listing* listing = calloc(1, sizeof(struct listing));
    json_t* json = new_json();
    json_object_del(json_object_get(json, "data"), "selftext");
    json_object_del(json_object_get(json, "data"), "ups");
    deserialize_listing(json, listing, 0);
    struct listing expected = new_expected_listing();
    expected.selftext = NULL;
    expected.ups = 0;
    assert_listing_equal(&expected, listing);
    free_listing(listing);
    json_decref(json);
}

void test_permalink_fallsback_to_url(void) {
    struct listing* listing = calloc(1, sizeof(struct listing));
    json_t* json = new_json();
    json_object_del(json_object_get(json, "data"), "permalink");
    json_object_set_new(json_object_get(json, "data"), "url", json_string("/fallback_url"));
    deserialize_listing(json, listing, 0);
    struct listing expected = new_expected_listing();
    expected.url = "https://www.reddit.com/fallback_url";
    assert_listing_equal(&expected, listing);
    free_listing(listing);
    json_decref(json);
}

void test_permalink_and_url_missing(void) {
    struct listing* listing = calloc(1, sizeof(struct listing));
    json_t* json = new_json();
    json_object_del(json_object_get(json, "data"), "permalink");
    deserialize_listing(json, listing, 0);
    struct listing expected = new_expected_listing();
    expected.url = NULL;
    assert_listing_equal(&expected, listing);
    free_listing(listing);
    json_decref(json);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path);
    RUN_TEST(test_no_data_key);
    RUN_TEST(test_no_title_key);
    RUN_TEST(test_nullable_keys_are_missing);
    RUN_TEST(test_permalink_fallsback_to_url);
    return UNITY_END();
}
