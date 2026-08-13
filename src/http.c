#include "http.h"

#include <curl/curl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void http_global_init(void) { curl_global_init(CURL_GLOBAL_DEFAULT); }

void http_global_cleanup(void) { curl_global_cleanup(); }

void http_response_free(http_response *resp) {
  if (resp == nullptr) return;
  free(resp->body);
  resp->body = nullptr;
  resp->len = 0;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *ud) {
  size_t realsize = size * nmemb;
  http_response *resp = (http_response *)ud;
  char *p = realloc(resp->body, resp->len + realsize + 1);
  if (p == nullptr) return 0;
  resp->body = p;
  memcpy(resp->body + resp->len, ptr, realsize);
  resp->len += realsize;
  resp->body[resp->len] = '\0';
  return realsize;
}

static bool perform(CURL *curl, http_response *resp) {
  resp->body = nullptr;
  resp->len = 0;
  resp->code = 0;

  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    free(resp->body);
    resp->body = nullptr;
    return false;
  }
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  resp->code = code;
  return true;
}

bool http_post_form(const char *url, const http_field *fields, size_t nfields,
                   const char *bearer, http_response *resp) {
  CURL *curl = curl_easy_init();
  if (curl == nullptr) return false;

  curl_mime *form = curl_mime_init(curl);
  for (size_t i = 0; i < nfields; i++) {
    curl_mimepart *part = curl_mime_addpart(form);
    curl_mime_name(part, fields[i].key);
    curl_mime_data(part, fields[i].value, CURL_ZERO_TERMINATED);
  }

  struct curl_slist *hdrs = nullptr;
  if (bearer != nullptr) {
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
    hdrs = curl_slist_append(hdrs, auth);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "cli-toot/0.1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  bool ok = perform(curl, resp);

  curl_mime_free(form);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  return ok;
}

bool http_post_json(const char *url, const char *body, const char *bearer,
                    http_response *resp) {
  CURL *curl = curl_easy_init();
  if (curl == nullptr) return false;

  struct curl_slist *hdrs = nullptr;
  hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
  if (bearer != nullptr) {
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
    hdrs = curl_slist_append(hdrs, auth);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "cli-toot/0.1.0");

  bool ok = perform(curl, resp);

  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  return ok;
}

bool http_get(const char *url, const char *bearer, http_response *resp) {
  CURL *curl = curl_easy_init();
  if (curl == nullptr) return false;

  struct curl_slist *hdrs = nullptr;
  if (bearer != nullptr) {
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
    hdrs = curl_slist_append(hdrs, auth);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "cli-toot/0.1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  bool ok = perform(curl, resp);

  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  return ok;
}
