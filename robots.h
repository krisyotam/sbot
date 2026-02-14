/* See LICENSE file for copyright and license details. */

#ifndef ROBOTS_H
#define ROBOTS_H

/* Maximum rules per robots.txt */
#define MAX_RULES 512

/* Rule types */
typedef struct {
	char *path;
	int allow; /* 1 = allow, 0 = disallow */
} RobotsRule;

/* Parsed robots.txt for a domain */
typedef struct {
	char *domain;
	RobotsRule rules[MAX_RULES];
	int nrules;
	int crawl_delay; /* seconds, 0 = none specified */
} Robots;

/* Fetch and parse robots.txt for a domain */
Robots *robots_fetch(const char *domain);

/* Check if a path is allowed */
int robots_allowed(Robots *r, const char *path);

/* Get crawl delay in seconds (0 = none) */
int robots_delay(Robots *r);

/* Free robots struct */
void robots_free(Robots *r);

#endif /* ROBOTS_H */
