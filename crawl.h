/* See LICENSE file for copyright and license details. */

#ifndef CRAWL_H
#define CRAWL_H

#include <stddef.h>

/* Hash table size (prime, ~64k buckets) */
#define HT_SIZE 65521

/* URL queue for BFS crawling */
typedef struct QueueNode {
	char *url;
	int depth;
	struct QueueNode *next;
} QueueNode;

typedef struct {
	QueueNode *head;
	QueueNode *tail;
	size_t count;
} UrlQueue;

/* Hash table node for visited URLs */
typedef struct HashNode {
	char *url;
	struct HashNode *next;
} HashNode;

/* Hash table based visited set - O(1) lookup */
typedef struct {
	HashNode *buckets[HT_SIZE];
	size_t count;
} VisitedSet;

/* Queue operations */
UrlQueue *queue_new(void);
void queue_free(UrlQueue *q);
void queue_push(UrlQueue *q, const char *url, int depth);
QueueNode *queue_pop(UrlQueue *q);
int queue_empty(UrlQueue *q);
size_t queue_size(UrlQueue *q);

/* Visited set operations (hash table) */
VisitedSet *visited_new(void);
void visited_free(VisitedSet *v);
void visited_add(VisitedSet *v, const char *url);
int visited_contains(VisitedSet *v, const char *url);
size_t visited_count(VisitedSet *v);

/* URL normalization for comparison */
char *url_normalize(const char *url);

/* Get path component from URL for directory structure */
char *url_to_path(const char *url, const char *base_domain);

#endif /* CRAWL_H */
