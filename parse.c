/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parse.h"
#include "util.h"

ResourceList *
reslist_new(void)
{
    ResourceList *list = xmalloc(sizeof(ResourceList));
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    return list;
}

void
reslist_free(ResourceList *list)
{
    if (!list)
        return;
    Resource *r = list->head;
    while (r) {
        Resource *next = r->next;
        free(r->url);
        free(r);
        r = next;
    }
    free(list);
}

int
reslist_contains(ResourceList *list, const char *url)
{
    for (Resource *r = list->head; r; r = r->next)
        if (strcmp(r->url, url) == 0)
            return 1;
    return 0;
}

void
reslist_add(ResourceList *list, const char *url, ResourceType type)
{
    if (!url || !*url || reslist_contains(list, url))
        return;

    /* Skip data: URLs */
    if (str_starts_with(url, "data:"))
        return;

    Resource *r = xmalloc(sizeof(Resource));
    r->url = xstrdup(url);
    r->type = type;
    r->next = NULL;

    if (list->tail) {
        list->tail->next = r;
        list->tail = r;
    } else {
        list->head = r;
        list->tail = r;
    }
    list->count++;
}

/* Extract attribute value from tag */
static char *
get_attr(const char *tag, const char *attr)
{
    size_t attr_len = strlen(attr);
    const char *p = tag;

    while (*p) {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p))
            p++;

        /* Check for attribute name */
        if (strncasecmp(p, attr, attr_len) == 0) {
            p += attr_len;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (*p == '=') {
                p++;
                while (*p && isspace((unsigned char)*p))
                    p++;

                char quote = 0;
                if (*p == '"' || *p == '\'') {
                    quote = *p++;
                }

                const char *start = p;
                if (quote) {
                    while (*p && *p != quote)
                        p++;
                } else {
                    while (*p && !isspace((unsigned char)*p) && *p != '>')
                        p++;
                }

                size_t len = p - start;
                char *value = xmalloc(len + 1);
                memcpy(value, start, len);
                value[len] = '\0';
                return value;
            }
        }

        /* Skip to next attribute */
        while (*p && !isspace((unsigned char)*p) && *p != '>')
            p++;
    }

    return NULL;
}

/* Determine resource type from URL/tag */
static ResourceType
guess_resource_type(const char *url, const char *tag_name)
{
    if (!tag_name)
        return RES_OTHER;

    if (strcasecmp(tag_name, "img") == 0)
        return RES_IMAGE;

    if (strcasecmp(tag_name, "link") == 0) {
        if (strstr(url, ".css") || strstr(url, "stylesheet"))
            return RES_CSS;
        if (strstr(url, ".woff") || strstr(url, ".ttf") || strstr(url, ".otf"))
            return RES_FONT;
        return RES_OTHER;
    }

    if (strcasecmp(tag_name, "a") == 0) {
        /* <a> links: page by default, but assets (pdf, binaries, extra images) get RES_ASSET */
        char *lower = xstrdup(url);
        str_tolower(lower);
        int is_asset = strstr(lower, ".pdf") || strstr(lower, ".ps") ||
                       strstr(lower, ".zip") || strstr(lower, ".tar") ||
                       strstr(lower, ".gz") || strstr(lower, ".doc") ||
                       strstr(lower, ".rtf") || strstr(lower, ".txt") ||
                       strstr(lower, ".csv") || strstr(lower, ".xls");
        if (!is_asset) {
            /* also treat linked images via <a> as assets so they get saved locally */
            if (strstr(lower, ".jpg") || strstr(lower, ".jpeg") ||
                strstr(lower, ".png") || strstr(lower, ".gif") ||
                strstr(lower, ".webp") || strstr(lower, ".svg") ||
                strstr(lower, ".ico"))
                is_asset = 1;
        }
        free(lower);
        return is_asset ? RES_ASSET : RES_PAGE;
    }

    if (strcasecmp(tag_name, "script") == 0)
        return RES_OTHER;

    /* Check by extension (for img/link etc that weren't caught above) */
    char *lower = xstrdup(url);
    str_tolower(lower);

    ResourceType type = RES_OTHER;
    if (strstr(lower, ".jpg") || strstr(lower, ".jpeg") ||
        strstr(lower, ".png") || strstr(lower, ".gif") ||
        strstr(lower, ".webp") || strstr(lower, ".svg") ||
        strstr(lower, ".ico"))
        type = RES_IMAGE;
    else if (strstr(lower, ".css"))
        type = RES_CSS;
    else if (strstr(lower, ".woff") || strstr(lower, ".woff2") ||
             strstr(lower, ".ttf") || strstr(lower, ".otf") ||
             strstr(lower, ".eot"))
        type = RES_FONT;
    else if (strstr(lower, ".pdf"))
        type = RES_ASSET;

    free(lower);
    return type;
}

ResourceList *
parse_html(const char *html, const char *base_url)
{
    ResourceList *list = reslist_new();
    const char *p = html;

    while (*p) {
        /* Find tag start */
        if (*p != '<') {
            p++;
            continue;
        }
        p++;

        /* Skip comments */
        if (str_starts_with(p, "!--")) {
            p = strstr(p, "-->");
            if (p)
                p += 3;
            else
                break;
            continue;
        }

        /* Get tag name */
        const char *tag_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/')
            p++;

        size_t tag_len = p - tag_start;
        if (tag_len == 0 || tag_len > 20)
            continue;

        char tag_name[21];
        memcpy(tag_name, tag_start, tag_len);
        tag_name[tag_len] = '\0';

        /* Find tag end */
        const char *tag_end = strchr(p, '>');
        if (!tag_end)
            break;

        /* Extract tag content for attribute parsing */
        size_t content_len = tag_end - tag_start;
        char *tag_content = xmalloc(content_len + 1);
        memcpy(tag_content, tag_start, content_len);
        tag_content[content_len] = '\0';

        /* Check for relevant attributes based on tag */
        char *url = NULL;

        if (strcasecmp(tag_name, "img") == 0) {
            url = get_attr(tag_content, "src");
            if (!url)
                url = get_attr(tag_content, "data-src");
        } else if (strcasecmp(tag_name, "link") == 0) {
            url = get_attr(tag_content, "href");
        } else if (strcasecmp(tag_name, "script") == 0) {
            url = get_attr(tag_content, "src");
        } else if (strcasecmp(tag_name, "a") == 0) {
            url = get_attr(tag_content, "href");
        } else if (strcasecmp(tag_name, "source") == 0) {
            url = get_attr(tag_content, "srcset");
            if (!url)
                url = get_attr(tag_content, "src");
        }

        if (url && *url) {
            char *resolved = url_resolve(base_url, url);
            ResourceType type = guess_resource_type(resolved, tag_name);
            reslist_add(list, resolved, type);
            free(resolved);
        }

        free(url);
        free(tag_content);
        p = tag_end + 1;
    }

    return list;
}

char *
parse_title(const char *html)
{
    const char *start = strcasestr(html, "<title");
    if (!start)
        return xstrdup("Untitled");

    start = strchr(start, '>');
    if (!start)
        return xstrdup("Untitled");
    start++;

    const char *end = strcasestr(start, "</title>");
    if (!end)
        return xstrdup("Untitled");

    size_t len = end - start;
    char *title = xmalloc(len + 1);
    memcpy(title, start, len);
    title[len] = '\0';

    return str_trim(title);
}

/* Helper to find and replace in string, returns new allocated string */
static char *
str_replace_first(const char *str, const char *old, size_t old_len, const char *new, size_t new_len)
{
    const char *pos = strstr(str, old);
    if (!pos)
        return xstrdup(str);

    size_t before_len = pos - str;
    size_t after_len = strlen(pos + old_len);
    size_t result_len = before_len + new_len + after_len;

    char *result = xmalloc(result_len + 1);
    memcpy(result, str, before_len);
    memcpy(result + before_len, new, new_len);
    memcpy(result + before_len + new_len, pos + old_len, after_len + 1);

    return result;
}

char *
inline_resources(const char *html, const char *base_url,
                 char *(*fetch_and_encode)(const char *url, const char *base_url))
{
    char *result = xstrdup(html);
    size_t search_offset = 0;

    /* Process img tags */
    while (1) {
        const char *p = strcasestr(result + search_offset, "<img");
        if (!p)
            break;

        const char *tag_end = strchr(p, '>');
        if (!tag_end)
            break;

        /* Calculate offset for this tag */
        size_t tag_offset = p - result;

        /* Find src attribute */
        const char *src_start = strcasestr(p, "src=");
        if (!src_start || src_start > tag_end) {
            /* No src, skip this img */
            search_offset = (tag_end - result) + 1;
            continue;
        }

        src_start += 4;
        char quote = 0;
        if (*src_start == '"' || *src_start == '\'')
            quote = *src_start++;

        const char *src_end = src_start;
        if (quote) {
            while (*src_end && *src_end != quote)
                src_end++;
        } else {
            while (*src_end && !isspace((unsigned char)*src_end) && *src_end != '>')
                src_end++;
        }

        /* Extract URL */
        size_t url_len = src_end - src_start;
        char *url = xmalloc(url_len + 1);
        memcpy(url, src_start, url_len);
        url[url_len] = '\0';

        /* Skip if already data URI */
        if (str_starts_with(url, "data:")) {
            free(url);
            search_offset = (tag_end - result) + 1;
            continue;
        }

        /* Fetch and encode */
        char *data_uri = fetch_and_encode(url, base_url);
        if (data_uri) {
            /* Build old and new strings for replacement */
            char *old_attr = xmalloc(url_len + 8);
            snprintf(old_attr, url_len + 8, "src=%c%s%c",
                     quote ? quote : '"', url, quote ? quote : '"');

            size_t new_attr_len = 5 + strlen(data_uri) + 2;
            char *new_attr = xmalloc(new_attr_len + 1);
            snprintf(new_attr, new_attr_len + 1, "src=\"%s\"", data_uri);

            char *new_result = str_replace_first(result, old_attr, strlen(old_attr),
                                                  new_attr, strlen(new_attr));
            /* Continue searching after the new data URI */
            search_offset = tag_offset + strlen(new_attr);

            free(result);
            result = new_result;

            free(old_attr);
            free(new_attr);
            free(data_uri);
        } else {
            /* Fetch failed, skip this img tag */
            search_offset = (tag_end - result) + 1;
        }

        free(url);
    }

    return result;
}
