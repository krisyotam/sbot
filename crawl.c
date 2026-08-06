/* See LICENSE file for copyright and license details. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crawl.h"
#include "util.h"

/* FNV-1a hash - fast, good distribution for URL strings */
static unsigned long
fnv1a(const char *s)
{
	unsigned long h = 2166136261UL;

	for (; *s; s++) {
		h ^= (unsigned char)*s;
		h *= 16777619UL;
	}
	return h;
}

UrlQueue *
queue_new(void)
{
	UrlQueue *q = xmalloc(sizeof(UrlQueue));

	q->head = NULL;
	q->tail = NULL;
	q->count = 0;
	return q;
}

void
queue_free(UrlQueue *q)
{
	QueueNode *n, *next;

	if (!q)
		return;
	for (n = q->head; n; n = next) {
		next = n->next;
		free(n->url);
		free(n);
	}
	free(q);
}

void
queue_push(UrlQueue *q, const char *url, int depth)
{
	QueueNode *n = xmalloc(sizeof(QueueNode));

	n->url = xstrdup(url);
	n->depth = depth;
	n->next = NULL;
	if (q->tail) {
		q->tail->next = n;
		q->tail = n;
	} else {
		q->head = n;
		q->tail = n;
	}
	q->count++;
}

QueueNode *
queue_pop(UrlQueue *q)
{
	QueueNode *n;

	if (!q->head)
		return NULL;
	n = q->head;
	q->head = n->next;
	if (!q->head)
		q->tail = NULL;
	q->count--;
	return n;
}

int
queue_empty(UrlQueue *q)
{
	return q->head == NULL;
}

size_t
queue_size(UrlQueue *q)
{
	return q->count;
}

VisitedSet *
visited_new(void)
{
	VisitedSet *v = xmalloc(sizeof(VisitedSet));

	memset(v->buckets, 0, sizeof(v->buckets));
	v->count = 0;
	return v;
}

void
visited_free(VisitedSet *v)
{
	HashNode *n, *next;
	size_t i;

	if (!v)
		return;
	for (i = 0; i < HT_SIZE; i++) {
		for (n = v->buckets[i]; n; n = next) {
			next = n->next;
			free(n->url);
			free(n);
		}
	}
	free(v);
}

void
visited_add(VisitedSet *v, const char *url)
{
	unsigned long h = fnv1a(url) % HT_SIZE;
	HashNode *n;

	/* Check for duplicate first */
	for (n = v->buckets[h]; n; n = n->next) {
		if (strcmp(n->url, url) == 0)
			return;
	}
	n = xmalloc(sizeof(HashNode));
	n->url = xstrdup(url);
	n->next = v->buckets[h];
	v->buckets[h] = n;
	v->count++;
}

int
visited_contains(VisitedSet *v, const char *url)
{
	unsigned long h = fnv1a(url) % HT_SIZE;
	HashNode *n;

	for (n = v->buckets[h]; n; n = n->next) {
		if (strcmp(n->url, url) == 0)
			return 1;
	}
	return 0;
}

size_t
visited_count(VisitedSet *v)
{
	return v->count;
}

char *
url_normalize(const char *url)
{
	char *norm, *p, *hash, *query;
	size_t len;

	norm = xstrdup(url);

	/* Remove fragment */
	hash = strchr(norm, '#');
	if (hash)
		*hash = '\0';

	/* Remove query string */
	query = strchr(norm, '?');
	if (query)
		*query = '\0';

	/* Remove trailing slash (but not bare domain slash) */
	len = strlen(norm);
	if (len > 1 && norm[len - 1] == '/') {
		/* Keep slash if it's just protocol://domain/ */
		p = norm;
		if (str_starts_with(p, "https://"))
			p += 8;
		else if (str_starts_with(p, "http://"))
			p += 7;
		/* Skip domain */
		while (*p && *p != '/')
			p++;
		/* Only strip if there's path beyond domain */
		if (p < norm + len - 1)
			norm[len - 1] = '\0';
	}

	/* Lowercase the domain part (use url_path to find where domain ends) */
	{
		const char *path = url_path(norm);
		char *dom_end = (char *)path;
		for (char *q = norm; q < dom_end; q++)
			*q = tolower((unsigned char)*q);
	}

	/* Remove default port :80 or :443 */
	p = norm;
	if (str_starts_with(p, "https://"))
		p += 8;
	else if (str_starts_with(p, "http://"))
		p += 7;
	{
		char *colon = NULL;
		char *slash = NULL;
		char *scan;
		int is_https;

		is_https = str_starts_with(norm, "https://");
		for (scan = p; *scan && *scan != '/'; scan++) {
			if (*scan == ':')
				colon = scan;
		}
		slash = scan;
		if (colon) {
			char port[8];
			size_t plen = slash - colon - 1;

			if (plen < sizeof(port)) {
				memcpy(port, colon + 1, plen);
				port[plen] = '\0';
				if ((is_https && strcmp(port, "443") == 0) ||
				    (!is_https && strcmp(port, "80") == 0)) {
					memmove(colon, slash,
						strlen(slash) + 1);
				}
			}
		}
	}

	return norm;
}

char *
url_to_path(const char *url, const char *base_domain)
{
	const char *p;
	char *path, *query, *hash, *new_path;
	size_t len, new_len;

	(void)base_domain;

	/* Use the new central helper — removes 8 lines of duplication */
	p = url_path(url);

	/* No path or just "/" -> index.html */
	if (!*p || strcmp(p, "/") == 0)
		return xstrdup("index.html");

	/* Skip the leading slash for the stored path */
	p++;
	if (*p == '\0')
		return xstrdup("index.html");

	/* Copy path, strip query/fragment */
	path = xstrdup(p);
	query = strchr(path, '?');
	if (query)
		*query = '\0';
	hash = strchr(path, '#');
	if (hash)
		*hash = '\0';

	/* Remove trailing slash */
	len = strlen(path);
	if (len > 0 && path[len - 1] == '/') {
		path[len - 1] = '\0';
		len--;
	}

	/* Basic sanitization for messy legacy filenames (vintage dumps, etc.) */
	{
		char *s;
		for (s = path; *s; s++) {
			/* Replace dangerous or annoying characters */
			if (*s == '\\' || *s == ':' || *s == '*' || *s == '?' ||
			    *s == '"' || *s == '<' || *s == '>' || *s == '|') {
				*s = '_';
			}
		}
	}

	/* If last path segment has no extension (no '.'), treat as directory */
	{
		const char *last = strrchr(path, '/');
		last = last ? last + 1 : path;
		if (len > 0 && strchr(last, '.') == NULL) {
			new_len = len + 12;
			new_path = xmalloc(new_len);
			snprintf(new_path, new_len, "%s/index.html", path);
			free(path);
			path = new_path;
		}
	}

	return path;
}
