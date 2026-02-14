/* See LICENSE file for copyright and license details.
 * archiver-bot - suckless web archiver
 * configuration header
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Archiver metadata */
#define ARCHIVER_NAME    "Kris Yotam"
#define ARCHIVER_SITE    "krisyotam.com"
#define ARCHIVER_VERSION "0.2.0"

/* Network settings */
#define USER_AGENT       "archiver-bot/0.2 (+https://krisyotam.com)"
#define CONNECT_TIMEOUT  30L
#define REQUEST_TIMEOUT  60L
#define MAX_REDIRECTS    10L

/* Crawl settings */
#define MAX_DEPTH        5
#define RATE_LIMIT_MS    1000  /* milliseconds between requests */
#define MAX_FILE_SIZE    (50 * 1024 * 1024)  /* 50 MB max per resource */

/* Output settings */
#define OUTPUT_EXT       ".gwtar.html"

/* Progress reporting interval (pages between status lines) */
#define PROGRESS_INTERVAL 10

#endif /* CONFIG_H */
