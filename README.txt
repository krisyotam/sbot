sbot


Simple Archiver Bot -- a suckless web archiver written in C.

sbot creates self-contained archives of web pages and entire websites.
Every resource -- CSS, images, fonts, scripts -- is fetched and inlined
directly into the HTML as base64 data URIs. The result is a single file
(or directory of files) that renders perfectly offline, with no external
dependencies, forever.


WHY

  Web pages disappear. Link rot is real. The average web page has a
  half-life of about two years. Bookmarks break, articles vanish,
  references evaporate.

  sbot solves this by creating archives that are:

  - Self-contained. Everything is inlined. No external requests needed.
  - Human-readable. Output is standard HTML. Open it in any browser.
  - Permanent. No database, no server, no special viewer. Just files.
  - Metadata-rich. GWTAR headers record provenance, date, and source.


MODES

  SINGLE PAGE ARCHIVE

    sbot https://example.com/article

    Archives a single page in GWTAR format (Gwern Web Tar Archive). This
    is the default mode and the most common use case. The output is one
    .gwtar.html file containing:

    - A GWTAR metadata header (HTML comment) with title, source URL,
      domain, author, archive date, and generator version
    - The full HTML with all CSS stylesheets inlined as style blocks
    - All images, fonts, and media encoded as data: URIs
    - A completely self-contained document that renders identically to
      the original

    GWTAR format is ideal for:

    - Archiving individual articles, blog posts, and essays
    - Preserving references and citations
    - Building a personal web archive / digital library
    - Saving pages before they disappear behind paywalls or get deleted


  WHOLE SITE ARCHIVE

    sbot -r https://example.com

    Recursively crawls an entire website and archives every page. The
    output is a directory tree that mirrors the site structure, with each
    page saved as a self-contained HTML file. Internal links are rewritten
    to relative paths so navigation works offline.

    Features:

    - BFS crawl order. Breadth-first traversal ensures important pages
      (closer to root) are archived first.
    - Same-domain only. Never follows links to external sites.
    - robots.txt compliance. Respects Disallow rules and Crawl-delay
      directives by default. Override with -R.
    - Depth control. Set maximum crawl depth with -d to limit scope.
    - Rate limiting. Configurable delay between requests to be polite to
      servers (default: 1 second).
    - Progress reporting. Periodic status lines showing pages archived,
      queue depth, and elapsed time.
    - Graceful degradation. Failed resources are skipped; the crawl
      continues.

    This mode is ideal for:

    - Archiving entire blogs or documentation sites
    - Creating offline mirrors of reference material
    - Preserving small-to-medium websites wholesale
    - Building browseable offline copies of sites you depend on


USAGE

  usage: sbot [-vrR] [-d depth] [-o dir] [-a author] url

    -v          verbose output
    -r          recursive (crawl entire site)
    -R          ignore robots.txt
    -d depth    max crawl depth (default: 5)
    -o dir      output directory
    -a author   site author name


  EXAMPLES

    Archive a single article:
      sbot https://example.com/blog/post

    Archive with author metadata:
      sbot -a "John Doe" https://example.com/article

    Crawl a blog, max depth 3:
      sbot -r -d 3 https://blog.example.com

    Verbose crawl to custom directory:
      sbot -v -r -o ./my-archive https://docs.example.com

    Crawl ignoring robots.txt restrictions:
      sbot -r -R https://example.com


GWTAR FORMAT

  Every archived page includes a GWTAR (Gwern Web Tar Archive) metadata
  header as an HTML comment at the top of the file:

    <!--
    ================================================================
      GWTAR ARCHIVE
    ================================================================

      Title:        Example Article
      Source URL:   https://example.com/article
      Domain:       example.com
      Author:       John Doe

      Archived by:  Kris Yotam
      Archived on:  krisyotam.com
      Archive date: 2026-02-14

      Generator:    sbot/0.3.0
      Format:       GWTAR (Gwern Web Tar Archive)

    ================================================================
    -->

  This header provides full provenance tracking: what was archived,
  where it came from, who archived it, and when.


RESOURCE INLINING

  sbot inlines all resources to create truly self-contained archives:

  Resource Type       Inlining Method
  ------------------  ------------------------------------------------
  CSS stylesheets     Fetched and inserted as style blocks
  Images              Base64-encoded as data:image/* URIs
  Fonts               Base64-encoded as data:font/* URIs
  Other media         Base64-encoded with appropriate MIME type

  Resources that fail to fetch are silently skipped -- the archive
  degrades gracefully rather than failing entirely.


BUILD

  Requires libcurl development headers.

  Arch Linux:
    sudo pacman -S curl

  Debian/Ubuntu:
    sudo apt install libcurl4-openssl-dev

  Build:
    make

  Install to /usr/local/bin:
    sudo make install

  Clean:
    make clean


CONFIGURATION

  All configuration is compile-time via config.h:

  Setting            Default       Description
  -----------------  ------------  -----------------------------------
  USER_AGENT         sbot/0.3      HTTP User-Agent string
  CONNECT_TIMEOUT    30s           Connection timeout
  REQUEST_TIMEOUT    60s           Total request timeout
  MAX_REDIRECTS      10            Maximum HTTP redirects to follow
  MAX_DEPTH          5             Default recursive crawl depth
  RATE_LIMIT_MS      1000ms        Delay between requests
  MAX_FILE_SIZE      50 MB         Maximum size per resource
  OUTPUT_EXT         .gwtar.html   File extension for archives

  Edit config.h and rebuild to change any setting. This is the suckless
  way -- no runtime configuration files, no environment variables, no
  hidden defaults.


ARCHITECTURE

  archiver.c   Main entry, page archiving, CSS inlining, link rewriting
  crawl.c      URL queue (BFS), visited set, URL normalization
  fetch.c      HTTP fetching via libcurl
  parse.c      HTML parsing, resource extraction, image inlining
  robots.c     robots.txt fetching, parsing, rule matching
  util.c       Memory wrappers, string ops, base64, MIME types
  config.h     Compile-time constants

  Single external dependency: libcurl. No XML parsers, no HTML5 parsers,
  no JavaScript engines. The HTML parsing is deliberately simple --
  regex-based extraction of src, href, and url() references. This handles
  the vast majority of real-world pages and keeps the codebase small and
  auditable.


PHILOSOPHY

  sbot follows the suckless philosophy:

  - Written in C99 with POSIX.1-2008
  - Minimal dependencies (libcurl only)
  - Configuration through config.h (edit and recompile)
  - Small, readable codebase
  - Does one thing well


LICENSE

  MIT/X Consortium License. See LICENSE for details.
