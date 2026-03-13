/* See LICENSE file for copyright and license details. */

#ifndef DETECT_H
#define DETECT_H

/* Detected site/CMS types */
typedef enum {
	SITE_UNKNOWN,
	SITE_WORDPRESS,
	SITE_BLOGGER,
	SITE_HUGO,
	SITE_JEKYLL,
	SITE_GHOST,
	SITE_DRUPAL,
	SITE_MEDIAWIKI
} SiteType;

/* Detection result with hints for archiving */
typedef struct {
	SiteType type;
	const char *name;       /* human-readable CMS name */
	char *feed_url;         /* RSS/Atom feed URL if found */
	char *api_url;          /* REST API base if found */
	char *sitemap_url;      /* sitemap.xml URL if found */
	int has_json_api;       /* site has a JSON API */
} SiteInfo;

/* Detect CMS type from HTML and URL */
SiteInfo *detect_site(const char *html, const char *url);

/* Free detection result */
void siteinfo_free(SiteInfo *info);

/* Get sitemap URLs for a detected site */
char **detect_sitemap_urls(SiteInfo *info, const char *domain,
                           int *count);

/* Get additional seed URLs based on CMS type */
char **detect_seed_urls(SiteInfo *info, const char *domain,
                        int *count);

#endif /* DETECT_H */
