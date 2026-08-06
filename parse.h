/* See LICENSE file for copyright and license details. */

#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

/* Resource types */
typedef enum {
    RES_IMAGE,
    RES_CSS,
    RES_FONT,
    RES_PAGE,
    RES_ASSET,   /* downloadable non-HTML: pdf, images via <a>, etc. */
    RES_OTHER
} ResourceType;

/* Extracted resource */
typedef struct Resource {
    char *url;
    ResourceType type;
    struct Resource *next;
} Resource;

/* Resource list */
typedef struct {
    Resource *head;
    Resource *tail;
    size_t count;
} ResourceList;

/* Create/destroy resource list */
ResourceList *reslist_new(void);
void reslist_free(ResourceList *list);

/* Add a resource (url is copied) */
void reslist_add(ResourceList *list, const char *url, ResourceType type);

/* Check if URL already in list */
int reslist_contains(ResourceList *list, const char *url);

/* Extract resources from HTML */
ResourceList *parse_html(const char *html, const char *base_url);

/* Extract title from HTML */
char *parse_title(const char *html);

/* Find and inline resources in HTML */
char *inline_resources(const char *html, const char *base_url,
                       char *(*fetch_and_encode)(const char *url, const char *base_url));

#endif /* PARSE_H */
