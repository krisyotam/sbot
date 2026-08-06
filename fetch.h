/* See LICENSE file for copyright and license details. */

#ifndef FETCH_H
#define FETCH_H

#include <stddef.h>

/* Retry settings */
#define FETCH_MAX_RETRIES  5   /* increased for old/slow archival targets */
#define FETCH_RETRY_BASE   2   /* base seconds for exponential backoff */

/* Response structure */
typedef struct {
	char *data;
	size_t size;
	char *content_type;
	long status_code;
	char *final_url;
} Response;

/* Initialize/cleanup curl globally */
void fetch_init(void);
void fetch_cleanup(void);

/* Fetch a URL with automatic retry on transient errors */
Response *fetch_url(const char *url);

/* Free a response */
void response_free(Response *resp);

#endif /* FETCH_H */
