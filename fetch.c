/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#include "config.h"
#include "fetch.h"
#include "util.h"

static CURL *curl_handle = NULL;

static size_t
write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	Response *resp = (Response *)userp;
	char *ptr;

	ptr = xrealloc(resp->data, resp->size + realsize + 1);
	resp->data = ptr;
	memcpy(&(resp->data[resp->size]), contents, realsize);
	resp->size += realsize;
	resp->data[resp->size] = '\0';
	return realsize;
}

/*
 * Check if an HTTP status code is transient (worth retrying).
 * 429 = rate limited, 5xx = server errors
 */
static int
is_transient(long code)
{
	return code == 429 || code == 500 || code == 502 ||
	       code == 503 || code == 504;
}

void
fetch_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	if (!curl_handle)
		die("curl_easy_init failed");
}

void
fetch_cleanup(void)
{
	if (curl_handle) {
		curl_easy_cleanup(curl_handle);
		curl_handle = NULL;
	}
	curl_global_cleanup();
}

Response *
fetch_url(const char *url)
{
	Response *resp;
	CURLcode res;
	char *ct, *effective_url;
	int attempt;

	for (attempt = 0; attempt < FETCH_MAX_RETRIES; attempt++) {
		if (attempt > 0) {
			unsigned int delay;

			delay = FETCH_RETRY_BASE * (1 << (attempt - 1));
			warn("retry %d/%d for %s (waiting %us)",
			     attempt, FETCH_MAX_RETRIES - 1, url, delay);
			sleep(delay);
		}

		resp = xmalloc(sizeof(Response));
		resp->data = xmalloc(1);
		resp->data[0] = '\0';
		resp->size = 0;
		resp->content_type = NULL;
		resp->status_code = 0;
		resp->final_url = NULL;

		curl_easy_reset(curl_handle);
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
		                 write_cb);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA,
		                 (void *)resp);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT,
		                 USER_AGENT);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS,
		                 MAX_REDIRECTS);
		curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT,
		                 CONNECT_TIMEOUT);
		curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,
		                 REQUEST_TIMEOUT);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, IGNORE_SSL_ERRORS ? 0L : 1L);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, IGNORE_SSL_ERRORS ? 0L : 2L);
		curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING,
		                 "");

		/* Enforce size limit to prevent pathological bloat (see config.h) */
		curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, MAX_FILE_SIZE);

		res = curl_easy_perform(curl_handle);

		if (res != CURLE_OK) {
			/* Network-level failure */
			if (res == CURLE_OPERATION_TIMEDOUT ||
			    res == CURLE_COULDNT_CONNECT ||
			    res == CURLE_GOT_NOTHING) {
				warn("fetch: %s: %s",
				     url, curl_easy_strerror(res));
				response_free(resp);
				resp = NULL;
				continue;
			}
			/* Non-transient curl error */
			warn("fetch: %s: %s",
			     url, curl_easy_strerror(res));
			response_free(resp);
			return NULL;
		}

		curl_easy_getinfo(curl_handle,
		                  CURLINFO_RESPONSE_CODE,
		                  &resp->status_code);

		ct = NULL;
		if (curl_easy_getinfo(curl_handle,
		                      CURLINFO_CONTENT_TYPE,
		                      &ct) == CURLE_OK && ct)
			resp->content_type = xstrdup(ct);

		effective_url = NULL;
		if (curl_easy_getinfo(curl_handle,
		                      CURLINFO_EFFECTIVE_URL,
		                      &effective_url) == CURLE_OK &&
		    effective_url)
			resp->final_url = xstrdup(effective_url);

		/* Retry on transient HTTP errors */
		if (is_transient(resp->status_code)) {
			response_free(resp);
			resp = NULL;
			continue;
		}

		/* Enforce MAX_FILE_SIZE after download (some servers ignore CURLOPT_MAXFILESIZE) */
		if (resp->size > MAX_FILE_SIZE) {
			warn("fetch: %s: exceeded MAX_FILE_SIZE (%zu > %d), skipping",
			     url, resp->size, MAX_FILE_SIZE);
			response_free(resp);
			return NULL;
		}

		return resp;
	}

	/* All retries exhausted */
	warn("fetch: gave up on %s after %d attempts",
	     url, FETCH_MAX_RETRIES);
	return resp;
}

void
response_free(Response *resp)
{
	if (!resp)
		return;
	free(resp->data);
	free(resp->content_type);
	free(resp->final_url);
	free(resp);
}
