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
