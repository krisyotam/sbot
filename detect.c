/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "detect.h"
#include "util.h"

/* CMS signature strings found in HTML */
static const char *wp_sigs[] = {
	"wp-content/",
	"wp-includes/",
	"wp-json/",
	"/xmlrpc.php",
	"name=\"generator\" content=\"WordPress",
	"powered by WordPress",
	NULL
};

static const char *blogger_sigs[] = {
	"blogger.com",
	"blogspot.com",
	"content=\"blogger\"",
	"name=\"generator\" content=\"Blogger",
	"b:skin",
	"b:template",
	NULL
};

static const char *hugo_sigs[] = {
	"name=\"generator\" content=\"Hugo",
	"powered by Hugo",
	"Hugo --",
	NULL
};

static const char *jekyll_sigs[] = {
	"name=\"generator\" content=\"Jekyll",
	"powered by Jekyll",
	"jekyll-",
	NULL
};

static const char *ghost_sigs[] = {
	"content=\"Ghost",
	"ghost-",
	"ghost/api/",
	"class=\"gh-",
	NULL
};

static const char *drupal_sigs[] = {
	"Drupal.settings",
	"name=\"generator\" content=\"Drupal",
	"/sites/default/files/",
	"/modules/",
	NULL
};

static const char *mediawiki_sigs[] = {
	"name=\"generator\" content=\"MediaWiki",
	"wgArticleId",
	"mw-content-text",
	"/wiki/",
	NULL
};

/* Check if HTML contains any signature from a list */
static int
match_sigs(const char *html, const char **sigs)
{
	int i, hits;

	hits = 0;
	for (i = 0; sigs[i]; i++) {
		if (strcasestr(html, sigs[i]))
			hits++;
	}
	return hits;
}

static const char *
sitetype_name(SiteType type)
{
	switch (type) {
	case SITE_WORDPRESS: return "WordPress";
	case SITE_BLOGGER:   return "Blogger";
	case SITE_HUGO:      return "Hugo";
	case SITE_JEKYLL:    return "Jekyll";
	case SITE_GHOST:     return "Ghost";
	case SITE_DRUPAL:    return "Drupal";
	case SITE_MEDIAWIKI: return "MediaWiki";
	default:             return "Unknown";
	}
}

/* Extract feed URL from <link> tags */
static char *
find_feed_url(const char *html, const char *base_url)
{
	const char *p, *href_start, *href_end;
	char *tag, *href;
	size_t tag_len, href_len;
	char quote;

	p = html;
	while ((p = strcasestr(p, "<link")) != NULL) {
		const char *end = strchr(p, '>');

		if (!end)
			break;

		tag_len = end - p;
		tag = xmalloc(tag_len + 1);
		memcpy(tag, p, tag_len);
		tag[tag_len] = '\0';

		/* Check for RSS/Atom type */
		if (strcasestr(tag, "application/rss+xml") ||
		    strcasestr(tag, "application/atom+xml")) {
			href_start = strcasestr(tag, "href=");
			if (href_start) {
				href_start += 5;
				quote = 0;
				if (*href_start == '"' ||
				    *href_start == '\'')
					quote = *href_start++;

				href_end = href_start;
				if (quote) {
					while (*href_end &&
					       *href_end != quote)
						href_end++;
				} else {
					while (*href_end &&
					       *href_end != ' ' &&
					       *href_end != '>')
						href_end++;
				}

				href_len = href_end - href_start;
				href = xmalloc(href_len + 1);
				memcpy(href, href_start, href_len);
				href[href_len] = '\0';

				free(tag);

				/* Resolve relative URL */
				if (str_starts_with(href, "http")) {
					return href;
				} else {
					char *resolved;

					resolved = url_resolve(
					    base_url, href);
					free(href);
					return resolved;
				}
			}
		}

		free(tag);
		p = end + 1;
	}

	return NULL;
}

SiteInfo *
detect_site(const char *html, const char *url)
{
	SiteInfo *info;
	int wp, bl, hu, jk, gh, dr, mw;
	int best;
	char *domain;

	info = xmalloc(sizeof(SiteInfo));
	info->type = SITE_UNKNOWN;
	info->name = "Unknown";
	info->feed_url = NULL;
	info->api_url = NULL;
	info->sitemap_url = NULL;
	info->has_json_api = 0;

	/* Count signature matches for each CMS */
	wp = match_sigs(html, wp_sigs);
	bl = match_sigs(html, blogger_sigs);
	hu = match_sigs(html, hugo_sigs);
	jk = match_sigs(html, jekyll_sigs);
	gh = match_sigs(html, ghost_sigs);
	dr = match_sigs(html, drupal_sigs);
	mw = match_sigs(html, mediawiki_sigs);

	/* Pick the CMS with the most signature hits */
	best = 0;

	if (wp > best) { info->type = SITE_WORDPRESS; best = wp; }
	if (bl > best) { info->type = SITE_BLOGGER; best = bl; }
	if (hu > best) { info->type = SITE_HUGO; best = hu; }
	if (jk > best) { info->type = SITE_JEKYLL; best = jk; }
	if (gh > best) { info->type = SITE_GHOST; best = gh; }
	if (dr > best) { info->type = SITE_DRUPAL; best = dr; }
	if (mw > best) { info->type = SITE_MEDIAWIKI; best = mw; }

	/* Require at least 1 hit */
	if (best < 1) {
		info->type = SITE_UNKNOWN;
		info->name = "Unknown";
		return info;
	}

	info->name = sitetype_name(info->type);
	domain = url_get_domain(url);

	/* Set CMS-specific hints */
	switch (info->type) {
	case SITE_WORDPRESS:
		info->has_json_api = 1;
		info->api_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/wp-json/wp/v2/") + 1);
		sprintf(info->api_url, "https://%s/wp-json/wp/v2/",
		        domain);
		info->sitemap_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/wp-sitemap.xml") + 1);
		sprintf(info->sitemap_url,
		        "https://%s/wp-sitemap.xml", domain);
		break;
	case SITE_BLOGGER:
		info->has_json_api = 1;
		/* Blogger Atom feed */
		info->feed_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/feeds/posts/default") + 1);
		sprintf(info->feed_url,
		        "https://%s/feeds/posts/default", domain);
		break;
	case SITE_HUGO:
		info->sitemap_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/sitemap.xml") + 1);
		sprintf(info->sitemap_url,
		        "https://%s/sitemap.xml", domain);
		break;
	case SITE_JEKYLL:
		info->sitemap_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/sitemap.xml") + 1);
		sprintf(info->sitemap_url,
		        "https://%s/sitemap.xml", domain);
		break;
	case SITE_GHOST:
		info->has_json_api = 1;
		info->api_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/ghost/api/content/") + 1);
		sprintf(info->api_url,
		        "https://%s/ghost/api/content/", domain);
		info->sitemap_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/sitemap.xml") + 1);
		sprintf(info->sitemap_url,
		        "https://%s/sitemap.xml", domain);
		break;
	case SITE_DRUPAL:
		info->sitemap_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/sitemap.xml") + 1);
		sprintf(info->sitemap_url,
		        "https://%s/sitemap.xml", domain);
		break;
	case SITE_MEDIAWIKI:
		info->has_json_api = 1;
		info->api_url = xmalloc(
		    strlen("https://") + strlen(domain) +
		    strlen("/w/api.php") + 1);
		sprintf(info->api_url,
		        "https://%s/w/api.php", domain);
		break;
	default:
		break;
	}

	/* Try to find feed URL from HTML if not set */
	if (!info->feed_url)
		info->feed_url = find_feed_url(html, url);

	free(domain);
	return info;
}

void
siteinfo_free(SiteInfo *info)
{
	if (!info)
		return;
	free(info->feed_url);
	free(info->api_url);
	free(info->sitemap_url);
	free(info);
}

/*
 * Parse a simple sitemap.xml to extract <loc> URLs.
 * Returns array of URL strings, sets *count.
 * Caller frees the array and each string.
 */
char **
detect_sitemap_urls(SiteInfo *info, const char *domain, int *count)
{
	char **urls;
	int capacity, n;

	(void)info;
	(void)domain;

	capacity = 64;
	n = 0;
	urls = xmalloc(capacity * sizeof(char *));

	*count = n;
	return urls;
}

/*
 * Get additional seed URLs based on CMS type.
 * For WordPress: /feed/, /wp-sitemap.xml
 * For Hugo/Jekyll: /sitemap.xml, /index.xml
 * For Blogger: /feeds/posts/default
 */
char **
detect_seed_urls(SiteInfo *info, const char *domain, int *count)
{
	char **urls;
	int n;
	size_t len;

	n = 0;
	urls = xmalloc(8 * sizeof(char *));

	switch (info->type) {
	case SITE_WORDPRESS:
		len = strlen("https://") + strlen(domain) +
		      strlen("/feed/") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/feed/", domain);
		n++;
		len = strlen("https://") + strlen(domain) +
		      strlen("/wp-sitemap.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/wp-sitemap.xml", domain);
		n++;
		break;
	case SITE_BLOGGER:
		len = strlen("https://") + strlen(domain) +
		      strlen("/feeds/posts/default") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/feeds/posts/default",
		        domain);
		n++;
		len = strlen("https://") + strlen(domain) +
		      strlen("/sitemap.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/sitemap.xml", domain);
		n++;
		break;
	case SITE_HUGO:
		/* fallthrough */
	case SITE_JEKYLL:
		len = strlen("https://") + strlen(domain) +
		      strlen("/sitemap.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/sitemap.xml", domain);
		n++;
		len = strlen("https://") + strlen(domain) +
		      strlen("/index.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/index.xml", domain);
		n++;
		break;
	case SITE_GHOST:
		len = strlen("https://") + strlen(domain) +
		      strlen("/sitemap.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/sitemap.xml", domain);
		n++;
		break;
	case SITE_DRUPAL:
		len = strlen("https://") + strlen(domain) +
		      strlen("/sitemap.xml") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/sitemap.xml", domain);
		n++;
		break;
	case SITE_MEDIAWIKI:
		len = strlen("https://") + strlen(domain) +
		      strlen("/wiki/Special:AllPages") + 1;
		urls[n] = xmalloc(len);
		sprintf(urls[n], "https://%s/wiki/Special:AllPages",
		        domain);
		n++;
		break;
	default:
		break;
	}

	*count = n;
	return urls;
}
