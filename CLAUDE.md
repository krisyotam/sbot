# archiver-bot — CLAUDE.md

## Project

archiver-bot is a suckless web archiver written in C. It creates
self-contained archives of websites with all resources (CSS, images,
fonts) inlined as data URIs. Supports single-page and recursive
whole-site archival with GWTAR (Gwern Web Tar Archive) format headers.

## Coding Standards — Suckless C Style

All code in this project MUST follow the suckless.org coding style:

### Language
- C99 (ISO/IEC 9899:1999), no extensions
- POSIX.1-2008 (`_POSIX_C_SOURCE 200809L`)

### Indentation & Whitespace
- Tabs for indentation (1 tab = 1 level)
- Spaces for alignment only, never for indentation
- No tabs except at the beginning of a line
- Maximum line length: 79 characters

### Comments
- Use `/* */` only, never `//`
- Comment fallthrough cases in switch statements

### Variables
- All declarations at the top of the block
- Pointer `*` adjacent to variable name: `char *p`, not `char* p`
- No C99 `bool`; use `int` (0/1)
- Global/static variables not used outside TU must be `static`

### Functions
- Return type on its own line
- Function name at column 0 on next line (enables `grep ^funcname`)
- Opening `{` on its own line for functions
- Functions not used outside their file: `static`

```c
static void
usage(void)
{
	fprintf(stderr, "usage: archiver-bot [-v] [-r] url\n");
	exit(1);
}
```

### Braces
- Opening `{` on same line for control flow (if, for, while, switch)
- Closing `}` on its own line unless continuing (else, do-while)
- Use braces even for single statements when sibling branches use them

### Naming
- lowercase_with_underscores for functions and variables
- UPPERCASE for macros and constants
- CamelCase for typedef'd struct types
- No `_t` suffix (reserved by POSIX)
- Prefix module functions with module name

### Control Flow
- Space after `if`, `for`, `while`, `switch`
- No space after `(` or before `)`
- Use `goto` for cleanup/unwind, not nested ifs
- Return/exit early on failure
- Test against 0, not -1: `if (func() < 0)`

### Error Handling
- All allocation checked; goto cleanup on failure
- `die()` for fatal errors (prints message, exits)
- `warn()` for recoverable errors (prints, continues)

### File Organization Order
1. License header
2. System includes (alphabetical)
3. Local includes
4. Macros
5. Type definitions
6. Function declarations
7. Global variables
8. Function definitions (same order as declarations)

### Headers
- System headers first, alphabetical
- Local headers after blank line
- No cyclic dependencies
- Include only what is needed

## Architecture

### Module Layout

| Module | Prefix | File | Responsibility |
|--------|--------|------|----------------|
| Main | — | archiver.c | Entry point, page archiving, CSS inlining, link rewriting, crawl orchestration |
| Crawler | `queue_`, `visited_` | crawl.c | URL queue (BFS), visited set, URL normalization, path conversion |
| Fetcher | `fetch_` | fetch.c | HTTP fetching via libcurl, response management |
| Parser | `reslist_`, `parse_` | parse.c | HTML parsing, resource extraction, image inlining |
| Utilities | `die`, `warn`, `x*`, `str_*`, `url_*` | util.c | Memory wrappers, string ops, URL helpers, base64, MIME types |
| Config | — | config.h | Compile-time constants (timeouts, limits, user agent) |

### Architecture Rules
- **Separate compilation.** Every .c file compiles independently.
- **No dynamic loading.** All features compiled in.
- **libcurl only.** Single external dependency for HTTP.
- **No `system()` calls.** Direct file I/O and libcurl only.
- **Data URIs for inlining.** Resources encoded as base64 data URIs.
- **Stateless functions preferred.** Minimize mutable global state.

### Crawler Design Principles
- **BFS traversal.** URL queue processes breadth-first by depth level.
- **Same-domain only.** Never follow links to external domains.
- **Politeness.** Rate limiting between requests (configurable).
- **Depth control.** Hard limit on crawl depth to prevent runaway.
- **URL normalization.** Canonical form for deduplication.
- **Graceful degradation.** Skip failed resources, continue crawling.

## Build

```sh
make            # build archiver-bot binary
make clean      # remove build artifacts
make install    # install to /usr/local/bin
```

Dependencies: `libcurl` (via pkg-config)

## Usage

```sh
# Single page
archiver-bot https://example.com/article

# Whole site (recursive, depth 3)
archiver-bot -r -d 3 https://example.com

# Verbose with custom output dir
archiver-bot -v -r -o ./archive https://example.com
```

## Git Conventions

- No `Co-Authored-By: Claude` lines
- Commit messages: imperative, <72 chars, no period
- One logical change per commit
