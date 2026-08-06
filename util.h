/* See LICENSE file for copyright and license details. */

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Memory allocation with error handling */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Base64 encoding */
char *base64_encode(const unsigned char *data, size_t input_len,
                    size_t *output_len);

/* String utilities */
int str_starts_with(const char *str, const char *prefix);
int str_ends_with(const char *str, const char *suffix);
char *str_tolower(char *str);
char *str_trim(char *str);

/* URL utilities */
char *url_resolve(const char *base, const char *relative);
char *url_get_domain(const char *url);
int url_same_domain(const char *url1, const char *url2);

/* Return pointer into url at the start of the path component (after domain).
 * Handles http:// https:// // and bare paths. Returns "/" if no path.
 * Never returns NULL. The returned pointer is inside the original string. */
const char *url_path(const char *url);

/* Compute a relative path from 'from' (current file path) to 'to' (target file path).
 * Both are paths relative to site root, e.g. "dir/page.htm" and "other/file.pdf".
 * Returns e.g. "../other/file.pdf" or "file.pdf" as needed for <a href> rewriting.
 * Caller must free the result. */
char *make_relative_path(const char *from, const char *to);

/* File utilities */
char *get_mime_type(const char *url);
char *sanitize_filename(const char *url);

/* Time utilities */
char *get_iso_date(void);

/* Error handling */
void die(const char *fmt, ...);
void warn(const char *fmt, ...);

#endif /* UTIL_H */
