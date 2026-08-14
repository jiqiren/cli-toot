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

char *urlencode(const char *s) {
  if (s == nullptr) return nullptr;
  static const char hex[] = "0123456789ABCDEF";
  size_t n = strlen(s);
  char *out = malloc(n * 3 + 1);
  if (out == nullptr) return nullptr;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[j++] = (char)c;
    } else {
      out[j++] = '%';
      out[j++] = hex[c >> 4];
      out[j++] = hex[c & 0x0F];
    }
  }
  out[j] = '\0';
  return out;
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
  /* OAuth token endpoints (and most REST APIs) require
   * application/x-www-form-urlencoded. curl_mime would emit
   * multipart/form-data, which Doorkeeper/Mastodon's /oauth/token
   * rejects (RFC 6749 4.1.3). Build the urlencoded body by hand. */
  size_t cap = 256;
  char *body = malloc(cap);
  if (body == nullptr) return false;
  size_t len = 0;
  bool first = true;
  for (size_t i = 0; i < nfields; i++) {
    char *e_val = urlencode(fields[i].value);
    if (e_val == nullptr) {
      free(body);
      return false;
    }
    size_t need = len + (first ? 0 : 1) + strlen(fields[i].key) + 1 +
                  strlen(e_val) + 1;
    if (need > cap) {
      while (cap < need) cap *= 2;
      char *p = realloc(body, cap);
      if (p == nullptr) {
        free(e_val);
        free(body);
        return false;
      }
      body = p;
    }
    len += (size_t)snprintf(body + len, cap - len, "%s%s=%s",
                            first ? "" : "&", fields[i].key, e_val);
    first = false;
    free(e_val);
  }

  struct curl_slist *hdrs = nullptr;
  hdrs = curl_slist_append(hdrs,
                           "Content-Type: application/x-www-form-urlencoded");
  if (bearer != nullptr) {
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
    hdrs = curl_slist_append(hdrs, auth);
  }

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    curl_slist_free_all(hdrs);
    free(body);
    return false;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "cli-toot/0.1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  bool ok = perform(curl, resp);

  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  free(body);
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
