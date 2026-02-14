/* See LICENSE file for copyright and license details. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robots.h"
#include "fetch.h"
#include "util.h"

/*
 * Parse robots.txt content for our user-agent.
 *
 * We look for rules matching "archiver-bot" first,
 * then fall back to "*" (wildcard) rules.
 */
Robots *
robots_fetch(const char *domain)
{
	Robots *r;
	Response *resp;
	char url[1024];
	char *line, *saveptr, *text;
	int in_our_group, in_star_group, found_our_group;
	int star_nrules, star_delay;
	RobotsRule star_rules[MAX_RULES];

	r = xmalloc(sizeof(Robots));
	r->domain = xstrdup(domain);
	r->nrules = 0;
	r->crawl_delay = 0;

	snprintf(url, sizeof(url), "https://%s/robots.txt", domain);
	resp = fetch_url(url);
	if (!resp || resp->status_code >= 400 || !resp->data) {
		/* No robots.txt = everything allowed */
		response_free(resp);
		return r;
	}

	text = xstrdup(resp->data);
	response_free(resp);

	in_our_group = 0;
	in_star_group = 0;
	found_our_group = 0;
	star_nrules = 0;
	star_delay = 0;

	for (line = strtok_r(text, "\n", &saveptr); line;
	     line = strtok_r(NULL, "\n", &saveptr)) {
		char *trimmed, *colon, *key, *val;

		trimmed = str_trim(line);

		/* Skip empty lines and comments */
		if (!*trimmed || *trimmed == '#')
			continue;

		/* Strip inline comments */
		colon = strchr(trimmed, '#');
		if (colon)
			*colon = '\0';

		/* Find key: value */
		colon = strchr(trimmed, ':');
		if (!colon)
			continue;
		*colon = '\0';
		key = str_trim(trimmed);
		val = str_trim(colon + 1);

		if (strcasecmp(key, "user-agent") == 0) {
			/* New user-agent group */
			if (strcasestr(val, "archiver-bot")) {
				in_our_group = 1;
				in_star_group = 0;
				found_our_group = 1;
			} else if (strcmp(val, "*") == 0 &&
			           !found_our_group) {
				in_star_group = 1;
				in_our_group = 0;
			} else {
				in_our_group = 0;
				in_star_group = 0;
			}
			continue;
		}

		if (strcasecmp(key, "disallow") == 0) {
			if (in_our_group && r->nrules < MAX_RULES) {
				r->rules[r->nrules].path = xstrdup(val);
				r->rules[r->nrules].allow = 0;
				r->nrules++;
			} else if (in_star_group &&
			           star_nrules < MAX_RULES) {
				star_rules[star_nrules].path = xstrdup(val);
				star_rules[star_nrules].allow = 0;
				star_nrules++;
			}
		} else if (strcasecmp(key, "allow") == 0) {
			if (in_our_group && r->nrules < MAX_RULES) {
				r->rules[r->nrules].path = xstrdup(val);
				r->rules[r->nrules].allow = 1;
				r->nrules++;
			} else if (in_star_group &&
			           star_nrules < MAX_RULES) {
				star_rules[star_nrules].path = xstrdup(val);
				star_rules[star_nrules].allow = 1;
				star_nrules++;
			}
		} else if (strcasecmp(key, "crawl-delay") == 0) {
			int delay = atoi(val);

			if (delay > 0) {
				if (in_our_group)
					r->crawl_delay = delay;
				else if (in_star_group)
					star_delay = delay;
			}
		}
	}

	/* If no specific rules for us, use wildcard rules */
	if (!found_our_group && star_nrules > 0) {
		int i;

		for (i = 0; i < star_nrules; i++)
			r->rules[i] = star_rules[i];
		r->nrules = star_nrules;
		r->crawl_delay = star_delay;
	} else if (!found_our_group) {
		/* Free star rules if we didn't use them */
		int i;

		for (i = 0; i < star_nrules; i++)
			free(star_rules[i].path);
	}

	free(text);
	return r;
}

int
robots_allowed(Robots *r, const char *path)
{
	int i, best_len, allowed;

	if (!r || r->nrules == 0)
		return 1;

	/*
	 * Match the most specific (longest) rule.
	 * If multiple rules of same length, Allow wins.
	 */
	best_len = -1;
	allowed = 1;

	for (i = 0; i < r->nrules; i++) {
		int plen = strlen(r->rules[i].path);

		/* Empty disallow = allow all */
		if (plen == 0 && !r->rules[i].allow)
			continue;

		if (strncmp(path, r->rules[i].path, plen) == 0) {
			if (plen > best_len) {
				best_len = plen;
				allowed = r->rules[i].allow;
			} else if (plen == best_len &&
			           r->rules[i].allow) {
				allowed = 1;
			}
		}
	}

	return allowed;
}

int
robots_delay(Robots *r)
{
	if (!r)
		return 0;
	return r->crawl_delay;
}

void
robots_free(Robots *r)
{
	int i;

	if (!r)
		return;
	for (i = 0; i < r->nrules; i++)
		free(r->rules[i].path);
	free(r->domain);
	free(r);
}
