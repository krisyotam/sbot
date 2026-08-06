/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#include "util.h"

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
        fputc(' ', stderr);
        perror(NULL);
    } else {
        fputc('\n', stderr);
    }
    exit(1);
}

void
warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "warning: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p)
        die("malloc:");
    return p;
}

void *
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p)
        die("realloc:");
    return p;
}

char *
xstrdup(const char *s)
{
    char *p = strdup(s);
    if (!p)
        die("strdup:");
    return p;
}

char *
base64_encode(const unsigned char *data, size_t input_len, size_t *output_len)
{
    size_t olen = 4 * ((input_len + 2) / 3);
    char *encoded = xmalloc(olen + 1);

    size_t i, j;
    for (i = 0, j = 0; i < input_len;) {
        unsigned int a = i < input_len ? data[i++] : 0;
        unsigned int b = i < input_len ? data[i++] : 0;
        unsigned int c = i < input_len ? data[i++] : 0;
        unsigned int triple = (a << 16) | (b << 8) | c;

        encoded[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded[j++] = base64_table[triple & 0x3F];
    }

    /* Add padding */
    size_t mod = input_len % 3;
    if (mod) {
        encoded[olen - 1] = '=';
        if (mod == 1)
            encoded[olen - 2] = '=';
    }

    encoded[olen] = '\0';
    if (output_len)
        *output_len = olen;
    return encoded;
}

int
str_starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int
str_ends_with(const char *str, const char *suffix)
{
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (suflen > slen)
        return 0;
    return strcmp(str + slen - suflen, suffix) == 0;
}

char *
str_tolower(char *str)
{
    for (char *p = str; *p; p++)
        *p = tolower((unsigned char)*p);
    return str;
}

char *
str_trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == '\0')
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

char *
url_get_domain(const char *url)
{
    const char *start, *end;
    char *domain;

    /* Skip protocol */
    if (str_starts_with(url, "https://"))
        start = url + 8;
    else if (str_starts_with(url, "http://"))
        start = url + 7;
    else if (str_starts_with(url, "//"))
        start = url + 2;
    else
        start = url;

    /* Find end of domain */
    end = start;
    while (*end && *end != '/' && *end != ':' && *end != '?')
        end++;

    size_t len = end - start;
    domain = xmalloc(len + 1);
    memcpy(domain, start, len);
    domain[len] = '\0';

    /* Strip leading www. (and similar) so www.example.com and example.com
     * are treated as the same domain. Critical for sites with mixed links. */
    if (strncasecmp(domain, "www.", 4) == 0) {
        memmove(domain, domain + 4, strlen(domain) - 3);
    }

    return domain;
}

int
url_same_domain(const char *url1, const char *url2)
{
    char *d1 = url_get_domain(url1);
    char *d2 = url_get_domain(url2);
    int same = strcasecmp(d1, d2) == 0;
    free(d1);
    free(d2);
    return same;
}

char *
url_resolve(const char *base, const char *relative)
{
    char *result;

    /* Already absolute */
    if (str_starts_with(relative, "http://") ||
        str_starts_with(relative, "https://") ||
        str_starts_with(relative, "data:")) {
        return xstrdup(relative);
    }

    /* Protocol-relative */
    if (str_starts_with(relative, "//")) {
        size_t len = 6 + strlen(relative);
        result = xmalloc(len + 1);
        snprintf(result, len + 1, "https:%s", relative);
        return result;
    }

    char *domain = url_get_domain(base);
    const char *proto = str_starts_with(base, "https://") ? "https://" : "http://";

    /* Root-relative */
    if (relative[0] == '/') {
        size_t len = strlen(proto) + strlen(domain) + strlen(relative);
        result = xmalloc(len + 1);
        snprintf(result, len + 1, "%s%s%s", proto, domain, relative);
        free(domain);
        return result;
    }

    /* Find base path */
    const char *path_start;
    if (str_starts_with(base, "https://"))
        path_start = base + 8;
    else if (str_starts_with(base, "http://"))
        path_start = base + 7;
    else
        path_start = base;

    /* Skip domain */
    while (*path_start && *path_start != '/')
        path_start++;

    /* Find last slash in path */
    const char *last_slash = strrchr(path_start, '/');
    if (!last_slash)
        last_slash = path_start;

    size_t base_len = last_slash - path_start + 1;
    size_t len = strlen(proto) + strlen(domain) + base_len + strlen(relative);
    result = xmalloc(len + 1);
    snprintf(result, len + 1, "%s%s%.*s%s", proto, domain, (int)base_len, path_start, relative);

    free(domain);
    return result;
}

char *
get_mime_type(const char *url)
{
    /* Strip query string */
    char *copy = xstrdup(url);
    char *query = strchr(copy, '?');
    if (query)
        *query = '\0';

    str_tolower(copy);

    const char *mime = "application/octet-stream";

    if (str_ends_with(copy, ".jpg") || str_ends_with(copy, ".jpeg"))
        mime = "image/jpeg";
    else if (str_ends_with(copy, ".png"))
        mime = "image/png";
    else if (str_ends_with(copy, ".gif"))
        mime = "image/gif";
    else if (str_ends_with(copy, ".webp"))
        mime = "image/webp";
    else if (str_ends_with(copy, ".svg"))
        mime = "image/svg+xml";
    else if (str_ends_with(copy, ".ico"))
        mime = "image/x-icon";
    else if (str_ends_with(copy, ".css"))
        mime = "text/css";
    else if (str_ends_with(copy, ".js"))
        mime = "application/javascript";
    else if (str_ends_with(copy, ".woff"))
        mime = "font/woff";
    else if (str_ends_with(copy, ".woff2"))
        mime = "font/woff2";
    else if (str_ends_with(copy, ".ttf"))
        mime = "font/ttf";
    else if (str_ends_with(copy, ".otf"))
        mime = "font/otf";
    else if (str_ends_with(copy, ".eot"))
        mime = "application/vnd.ms-fontobject";

    free(copy);
    return xstrdup(mime);
}

char *
sanitize_filename(const char *url)
{
    char *domain = url_get_domain(url);
    size_t len = strlen(domain) + 32;
    char *filename = xmalloc(len);

    /* Replace dots with underscores */
    for (char *p = domain; *p; p++)
        if (*p == '.')
            *p = '_';

    snprintf(filename, len, "%s", domain);
    free(domain);
    return filename;
}

char *
get_iso_date(void)
{
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    char *buf = xmalloc(32);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

/* make_relative_path: given two paths from site root (like url_to_path results),
 * compute minimal relative reference from 'from' (current) to 'to' (target).
 * Handles going up with ../ and down. Assumes no .. or . in inputs.
 */
char *
make_relative_path(const char *from, const char *to)
{
    if (!from || !to)
        return xstrdup(to ? to : "");

    /* If identical (after norm), empty relative (or just filename if same) */
    if (strcmp(from, to) == 0) {
        const char *last = strrchr(to, '/');
        return xstrdup(last ? last + 1 : to);
    }

    /* Work on dir of 'from' only (current document's directory) */
    char *from_dir = xstrdup(from);
    char *slash = strrchr(from_dir, '/');
    if (slash)
        *slash = '\0';
    else
        from_dir[0] = '\0';

    /* Find common prefix length in from_dir and to */
    const char *f = from_dir;
    const char *t = to;
    const char *last_match = NULL;
    while (*f && *t && *f == *t) {
        if (*f == '/')
            last_match = f;
        f++;
        t++;
    }
    if (*f == '\0' && (*t == '\0' || *t == '/'))
        last_match = f; /* full match or to under from_dir */

    /* Count how many dirs up from from_dir's remaining */
    int up = 0;
    const char *f_rem = last_match ? last_match + 1 : from_dir;
    if (*f_rem == '/')
        f_rem++;
    for (const char *p = f_rem; *p; p++)
        if (*p == '/')
            up++;

    /* from_dir may have had trailing after last_match, count its / */
    /* Build result: ups * "../" + suffix of to after last_match */
    const char *to_rem = last_match ? last_match + 1 : to;
    if (*to_rem == '/')
        to_rem++;

    size_t up_len = (size_t)up * 3;
    size_t rem_len = strlen(to_rem);
    char *result = xmalloc(up_len + rem_len + 1);
    result[0] = '\0';
    for (int i = 0; i < up; i++)
        strcat(result, "../");
    strcat(result, to_rem);

    /* If result empty, use the basename of to */
    if (result[0] == '\0') {
        const char *b = strrchr(to, '/');
        free(result);
        result = xstrdup(b ? b + 1 : to);
    }

    free(from_dir);
    return result;
}

/*
 * url_path - suckless helper: return pointer to the path part of a URL.
 * Used everywhere we used to manually skip protocol + domain.
 * Examples:
 *   "https://example.com/foo/bar?x=1" -> "/foo/bar?x=1"
 *   "http://www.example.com"          -> "/"
 *   "/already/relative"               -> "/already/relative"
 */
const char *
url_path(const char *url)
{
    const char *p;

    if (!url)
        return "/";

    p = url;

    if (str_starts_with(p, "https://"))
        p += 8;
    else if (str_starts_with(p, "http://"))
        p += 7;
    else if (str_starts_with(p, "//"))
        p += 2;

    /* Skip domain (until /, ?, #, or end) */
    while (*p && *p != '/' && *p != '?' && *p != '#')
        p++;

    if (*p == '\0' || *p == '?' || *p == '#')
        return "/";

    return p;   /* points at the leading '/' of the path */
}
