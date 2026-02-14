/* See LICENSE file for copyright and license details.
 *
 * archiver-bot - suckless web archiver
 *
 * Creates self-contained archives of websites with all
 * resources inlined as data URIs.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "crawl.h"
#include "fetch.h"
#include "parse.h"
#include "robots.h"
#include "util.h"

/* Global options */
static int verbose = 0;
static int recursive = 0;
static int max_depth = MAX_DEPTH;
static int respect_robots = 1;
static const char *author = "Unknown";
static const char *output_dir = NULL;
static char *base_domain = NULL;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: archiver-bot [-vrR] [-d depth] [-o dir]"
	    " [-a author] url\n"
	    "\n"
	    "  -v          verbose output\n"
	    "  -r          recursive (crawl entire site)\n"
	    "  -R          ignore robots.txt\n"
	    "  -d depth    max crawl depth (default: %d)\n"
	    "  -o dir      output directory\n"
	    "  -a author   site author name\n",
	    MAX_DEPTH);
	exit(1);
}

static int
mkdirp(const char *path)
{
	char *p, *sep;

	p = xstrdup(path);
	for (sep = p + 1; *sep; sep++) {
		if (*sep == '/') {
			*sep = '\0';
			if (mkdir(p, 0755) != 0 && errno != EEXIST) {
				free(p);
				return -1;
			}
			*sep = '/';
		}
	}
	free(p);
	return 0;
}

static char *
fetch_and_encode(const char *url, const char *base)
{
	char *resolved, *mime, *b64, *data_uri, *semi;
	Response *resp;
	size_t b64_len, uri_len;

	resolved = url_resolve(base, url);
	if (verbose)
		fprintf(stderr, "    resource: %s\n", resolved);

	resp = fetch_url(resolved);
	if (!resp || resp->status_code >= 400 || resp->size == 0) {
		free(resolved);
		response_free(resp);
		return NULL;
	}

	if (resp->content_type) {
		mime = xstrdup(resp->content_type);
		semi = strchr(mime, ';');
		if (semi)
			*semi = '\0';
		str_trim(mime);
	} else {
		mime = get_mime_type(resolved);
	}

	b64 = base64_encode((unsigned char *)resp->data,
	                     resp->size, &b64_len);

	uri_len = 5 + strlen(mime) + 8 + b64_len + 1;
	data_uri = xmalloc(uri_len);
	snprintf(data_uri, uri_len, "data:%s;base64,%s", mime, b64);

	free(mime);
	free(b64);
	free(resolved);
	response_free(resp);
	return data_uri;
}

static char *
generate_header(const char *title, const char *source_url)
{
	char *date, *domain, *header;
	size_t len;

	date = get_iso_date();
	domain = url_get_domain(source_url);

	len = 2048 + strlen(title) + strlen(source_url) +
	      strlen(author) + strlen(domain);
	header = xmalloc(len);

	snprintf(header, len,
	    "<!--\n"
	    "========================================"
	    "========================================\n"
	    "  GWTAR ARCHIVE\n"
	    "========================================"
	    "========================================\n"
	    "\n"
	    "  Title:        %s\n"
	    "  Source URL:   %s\n"
	    "  Domain:       %s\n"
	    "  Author:       %s\n"
	    "\n"
	    "  Archived by:  %s\n"
	    "  Archived on:  %s\n"
	    "  Archive date: %s\n"
	    "\n"
	    "  Generator:    archiver-bot/%s\n"
	    "  Format:       GWTAR (Gwern Web Tar Archive)\n"
	    "\n"
	    "========================================"
	    "========================================\n"
	    "-->\n",
	    title, source_url, domain, author,
	    ARCHIVER_NAME, ARCHIVER_SITE, date,
	    ARCHIVER_VERSION);

	free(date);
	free(domain);
	return header;
}

static int
path_depth(const char *path)
{
	int depth = 0;
	const char *p;

	for (p = path; *p; p++)
		if (*p == '/')
			depth++;
	return depth;
}

static char *
make_relative_prefix(int depth)
{
	char *prefix;
	size_t len;
	int i;

	if (depth == 0)
		return xstrdup("");
	len = depth * 3 + 1;
	prefix = xmalloc(len);
	prefix[0] = '\0';
	for (i = 0; i < depth; i++)
		strcat(prefix, "../");
	return prefix;
}

static char *
rewrite_links(char *html, const char *rel_path)
{
	int depth;
	char *prefix, *result, *new_result;
	size_t prefix_len, search_offset, pos, old_len, new_len;
	const char *p;

	depth = path_depth(rel_path);
	prefix = make_relative_prefix(depth);
	prefix_len = strlen(prefix);
	result = html;

	search_offset = 0;
	while (1) {
		p = strstr(result + search_offset, "href=\"/");
		if (!p)
			break;
		if (p[7] == '/') {
			search_offset = (p - result) + 8;
			continue;
		}
		pos = p - result + 6;
		old_len = strlen(result);
		new_len = old_len - 1 + prefix_len;
		new_result = xmalloc(new_len + 1);
		memcpy(new_result, result, pos);
		memcpy(new_result + pos, prefix, prefix_len);
		memcpy(new_result + pos + prefix_len,
		       result + pos + 1, old_len - pos);
		free(result);
		result = new_result;
		search_offset = pos + prefix_len;
	}

	search_offset = 0;
	while (1) {
		p = strstr(result + search_offset, "src=\"/");
		if (!p)
			break;
		if (p[6] == '/') {
			search_offset = (p - result) + 7;
			continue;
		}
		pos = p - result + 5;
		old_len = strlen(result);
		new_len = old_len - 1 + prefix_len;
		new_result = xmalloc(new_len + 1);
		memcpy(new_result, result, pos);
		memcpy(new_result + pos, prefix, prefix_len);
		memcpy(new_result + pos + prefix_len,
		       result + pos + 1, old_len - pos);
		free(result);
		result = new_result;
		search_offset = pos + prefix_len;
	}

	free(prefix);
	return result;
}

static char *
inline_css(char *html, const char *base)
{
	char *result, *tag, *href, *resolved;
	char *new_tag, *new_result;
	const char *link_start, *link_end;
	const char *href_start, *href_end;
	size_t search_offset, tag_offset, tag_len, href_len;
	size_t new_tag_len, old_len, new_len, result_len;
	Response *resp;
	char quote;

	result = html;
	search_offset = 0;

	while (1) {
		link_start = strcasestr(result + search_offset,
		                        "<link");
		if (!link_start)
			break;
		link_end = strchr(link_start, '>');
		if (!link_end)
			break;

		tag_offset = link_start - result;
		tag_len = link_end - link_start;
		tag = xmalloc(tag_len + 1);
		memcpy(tag, link_start, tag_len);
		tag[tag_len] = '\0';

		if (!strcasestr(tag, "stylesheet")) {
			free(tag);
			search_offset = (link_end - result) + 1;
			continue;
		}

		href_start = strcasestr(tag, "href=");
		if (!href_start) {
			free(tag);
			search_offset = (link_end - result) + 1;
			continue;
		}
		href_start += 5;
		quote = 0;
		if (*href_start == '"' || *href_start == '\'')
			quote = *href_start++;

		href_end = href_start;
		if (quote) {
			while (*href_end && *href_end != quote)
				href_end++;
		} else {
			while (*href_end && *href_end != ' ' &&
			       *href_end != '>')
				href_end++;
		}

		href_len = href_end - href_start;
		href = xmalloc(href_len + 1);
		memcpy(href, href_start, href_len);
		href[href_len] = '\0';

		resolved = url_resolve(base, href);
		if (verbose)
			fprintf(stderr, "    css: %s\n", resolved);

		resp = fetch_url(resolved);
		free(resolved);

		if (resp && resp->status_code < 400 &&
		    resp->size > 0) {
			new_tag_len = 7 + resp->size + 8 + 1;
			new_tag = xmalloc(new_tag_len);
			snprintf(new_tag, new_tag_len,
			         "<style>%s</style>", resp->data);

			old_len = (link_end + 1) - link_start;
			new_len = strlen(new_tag);
			result_len = strlen(result);

			new_result = xmalloc(
			    result_len - old_len + new_len + 1);
			memcpy(new_result, result, tag_offset);
			memcpy(new_result + tag_offset,
			       new_tag, new_len);
			memcpy(new_result + tag_offset + new_len,
			       link_end + 1,
			       result_len - tag_offset - old_len + 1);

			free(result);
			result = new_result;
			search_offset = tag_offset + new_len;
			free(new_tag);
		} else {
			search_offset = (link_end - result) + 1;
		}

		response_free(resp);
		free(href);
		free(tag);
	}

	return result;
}

static void
extract_links(const char *html, const char *base_url,
              UrlQueue *queue, VisitedSet *visited,
              int current_depth, Robots *robots)
{
	ResourceList *resources;
	Resource *r;
	char *norm, *path;
	const char *pstart;

	if (current_depth >= max_depth)
		return;

	resources = parse_html(html, base_url);

	for (r = resources->head; r; r = r->next) {
		if (r->type != RES_PAGE)
			continue;

		if (str_starts_with(r->url, "mailto:") ||
		    str_starts_with(r->url, "tel:") ||
		    str_starts_with(r->url, "javascript:") ||
		    str_starts_with(r->url, "#"))
			continue;

		if (!url_same_domain(r->url, base_url))
			continue;

		/* Check robots.txt */
		if (robots) {
			pstart = r->url;
			if (str_starts_with(pstart, "https://"))
				pstart += 8;
			else if (str_starts_with(pstart, "http://"))
				pstart += 7;
			while (*pstart && *pstart != '/')
				pstart++;
			path = xstrdup(pstart[0] ? pstart : "/");
			if (!robots_allowed(robots, path)) {
				if (verbose)
					fprintf(stderr,
					    "    robots: blocked"
					    " %s\n", r->url);
				free(path);
				continue;
			}
			free(path);
		}

		norm = url_normalize(r->url);
		if (!visited_contains(visited, norm)) {
			visited_add(visited, norm);
			queue_push(queue, r->url,
			           current_depth + 1);
			if (verbose)
				fprintf(stderr,
				    "    queued: %s\n", r->url);
		}
		free(norm);
	}

	reslist_free(resources);
}

static int
save_page(const char *url, const char *final_url,
          const char *data, const char *rel_path)
{
	char *html, *title, *header;
	char *full_path, *dir, *last_slash;
	size_t full_path_len;
	FILE *fp;

	html = xstrdup(data);
	title = parse_title(html);

	if (verbose)
		fprintf(stderr, "  title: %s\n", title);

	html = inline_css(html, final_url);
	html = inline_resources(html, final_url,
	                        fetch_and_encode);
	html = rewrite_links(html, rel_path);

	header = generate_header(title, url);

	full_path_len = strlen(output_dir) + 1 +
	                strlen(rel_path) + 1;
	full_path = xmalloc(full_path_len);
	snprintf(full_path, full_path_len, "%s/%s",
	         output_dir, rel_path);

	dir = xstrdup(full_path);
	last_slash = strrchr(dir, '/');
	if (last_slash) {
		*last_slash = '\0';
		mkdirp(dir);
		mkdir(dir, 0755);
	}
	free(dir);

	fp = fopen(full_path, "w");
	if (!fp) {
		warn("cannot write: %s", full_path);
		free(full_path);
		free(header);
		free(html);
		free(title);
		return -1;
	}

	fputs(header, fp);
	fputs(html, fp);
	fclose(fp);

	fprintf(stderr, "  saved: %s\n", full_path);

	free(full_path);
	free(header);
	free(html);
	free(title);
	return 0;
}

static int
archive_page(const char *url)
{
	Response *resp;
	const char *final_url;
	char *rel_path;
	int ret;

	if (verbose)
		fprintf(stderr, "[0] %s\n", url);

	resp = fetch_url(url);
	if (!resp) {
		warn("failed to fetch: %s", url);
		return -1;
	}
	if (resp->status_code >= 400) {
		warn("HTTP %ld: %s", resp->status_code, url);
		response_free(resp);
		return -1;
	}
	if (resp->content_type &&
	    !strstr(resp->content_type, "text/html")) {
		if (verbose)
			fprintf(stderr, "  skip non-HTML: %s\n",
			        resp->content_type);
		response_free(resp);
		return 0;
	}

	final_url = resp->final_url ? resp->final_url : url;
	rel_path = url_to_path(url, base_domain);

	ret = save_page(url, final_url, resp->data, rel_path);

	free(rel_path);
	response_free(resp);
	return ret;
}

static void
crawl_site(const char *start_url)
{
	UrlQueue *queue;
	VisitedSet *visited;
	Robots *robots = NULL;
	QueueNode *node;
	Response *resp;
	const char *final_url;
	char *norm, *url, *rel_path;
	int depth, pages_archived, rate_ms;
	time_t start_time, now;

	queue = queue_new();
	visited = visited_new();

	if (respect_robots) {
		fprintf(stderr, "Fetching robots.txt for %s...\n",
		        base_domain);
		robots = robots_fetch(base_domain);
		if (robots->nrules > 0)
			fprintf(stderr, "  %d rules loaded\n",
			        robots->nrules);
		else
			fprintf(stderr, "  no restrictions\n");

		if (robots_delay(robots) > 0) {
			rate_ms = robots_delay(robots) * 1000;
			fprintf(stderr, "  crawl-delay: %ds\n",
			        robots_delay(robots));
		} else {
			rate_ms = RATE_LIMIT_MS;
		}
	} else {
		rate_ms = RATE_LIMIT_MS;
	}

	norm = url_normalize(start_url);
	visited_add(visited, norm);
	free(norm);
	queue_push(queue, start_url, 0);

	pages_archived = 0;
	start_time = time(NULL);

	while (!queue_empty(queue)) {
		node = queue_pop(queue);
		url = node->url;
		depth = node->depth;

		resp = fetch_url(url);
		if (!resp || resp->status_code >= 400) {
			if (verbose)
				fprintf(stderr, "[%d] SKIP %s\n",
				        depth, url);
			response_free(resp);
			free(url);
			free(node);
			continue;
		}

		if (resp->content_type &&
		    !strstr(resp->content_type, "text/html")) {
			response_free(resp);
			free(url);
			free(node);
			continue;
		}

		final_url = resp->final_url ?
		            resp->final_url : url;

		fprintf(stderr, "[d=%d q=%zu v=%zu] %s\n",
		        depth, queue_size(queue),
		        visited_count(visited), url);

		extract_links(resp->data, final_url, queue,
		              visited, depth, robots);

		rel_path = url_to_path(url, base_domain);
		save_page(url, final_url, resp->data, rel_path);
		pages_archived++;

		if (pages_archived % PROGRESS_INTERVAL == 0) {
			now = time(NULL);
			fprintf(stderr,
			    "\n--- %d pages, %zu queued, "
			    "%zu visited, %lds ---\n\n",
			    pages_archived,
			    queue_size(queue),
			    visited_count(visited),
			    (long)(now - start_time));
		}

		free(rel_path);
		response_free(resp);
		free(url);
		free(node);

		usleep(rate_ms * 1000);
	}

	now = time(NULL);
	fprintf(stderr,
	    "\nDone: %d pages to %s/ in %lds\n",
	    pages_archived, output_dir,
	    (long)(now - start_time));

	robots_free(robots);
	queue_free(queue);
	visited_free(visited);
}

int
main(int argc, char *argv[])
{
	const char *url;
	int opt;

	while ((opt = getopt(argc, argv, "vrRd:o:a:h")) != -1) {
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'r':
			recursive = 1;
			break;
		case 'R':
			respect_robots = 0;
			break;
		case 'd':
			max_depth = atoi(optarg);
			if (max_depth < 1)
				max_depth = 1;
			break;
		case 'o':
			output_dir = optarg;
			break;
		case 'a':
			author = optarg;
			break;
		case 'h': /* fallthrough */
		default:
			usage();
		}
	}

	if (optind >= argc)
		usage();

	url = argv[optind];
	base_domain = url_get_domain(url);
	if (!output_dir)
		output_dir = base_domain;

	if (mkdir(output_dir, 0755) != 0 && errno != EEXIST)
		die("cannot create directory: %s", output_dir);

	fetch_init();

	fprintf(stderr, "archiver-bot %s\n", ARCHIVER_VERSION);
	fprintf(stderr, "Target: %s\n", url);
	fprintf(stderr, "Output: %s/\n", output_dir);
	if (recursive)
		fprintf(stderr, "Mode: recursive (depth %d)\n",
		        max_depth);
	if (!respect_robots)
		fprintf(stderr, "Warning: ignoring robots.txt\n");
	fprintf(stderr, "\n");

	if (recursive)
		crawl_site(url);
	else
		archive_page(url);

	fetch_cleanup();
	free(base_domain);
	return 0;
}
