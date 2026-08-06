# sbot Archival Quality Improvements (2026-05)

This document captures concrete problems found during the 2026 grok_test collection
and minimal, suckless-style fixes.

## The 4 Problems Identified

### 1. Old Web Mirrors Are Painful (Dead HTTPS + Slow Hosts)

**Evidence from tests:**
- oocities-org: Flood of "SSL peer certificate or SSH remote key was not OK" on almost every subpage.
- textfiles-com: Repeated timeouts on robots.txt and root, exhausted 3 retries and gave up.

**Root causes in current code:**
- fetch.c: strict `CURLOPT_SSL_VERIFYPEER=1` and `VERIFYHOST=2` with no option to relax for archival use.
- fetch.h: Hardcoded `FETCH_MAX_RETRIES=3` and short exponential backoff (base 2s).
- No concept of "known slow host" or per-host backoff.
- No way to treat certain certificate errors as non-fatal for old archives.

**Proposed minimal changes:**
- Add `IGNORE_SSL_ERRORS` compile-time option in config.h (default 0 for safety).
- Increase default retries or make `FETCH_MAX_RETRIES` and backoff tunable in config.h.
- Consider a small "known bad hosts" mechanism or at least better logging.

### 2. Pathological Bloat Risk

**Evidence:**
- lowendmac-com ballooned to 250MB with only 5 files because early pages inlined enormous images.

**Root causes:**
- `MAX_FILE_SIZE` (50MB) is defined in config.h but **never enforced** during fetch or inlining.
- No per-resource size cap is passed to libcurl (`CURLOPT_MAXFILESIZE` is not used).
- No early rejection of giant resources before base64 inlining (which multiplies memory).

**Proposed minimal changes:**
- Actually wire `MAX_FILE_SIZE` into fetch.c (use CURLOPT_MAXFILESIZE + check after download).
- Skip inlining resources over a certain size (new config option or reuse MAX_FILE_SIZE).
- Add a warning when a resource is skipped due to size.

### 3. Timeout Fragility on Slow Sites

**Evidence:**
- Multiple runs (including stretch targets) were killed by the 3-minute external tool timeout before sbot could finish.
- textfiles and oocities both showed repeated "Timeout was reached".

**Root causes:**
- REQUEST_TIMEOUT is 60s (reasonable).
- But there is no progressive backoff for hosts that are consistently slow.
- No "slow host" tracking that increases timeouts or reduces parallelism for that host.
- The global rate limit (RATE_LIMIT_MS) is the only politeness mechanism.

**Proposed minimal changes:**
- Make REQUEST_TIMEOUT and CONNECT_TIMEOUT configurable in config.h (they already are, but document better).
- Add optional "slow host" backoff (simple host hash + failure counter).
- Consider a compile-time `ARCHIVAL_MODE` that is more patient.

### 4. Asset Writing Failures on Messy Filenames / Long Paths

**Evidence:**
- vintage-computer-com had many "cannot write asset" warnings for paths like `2020test/images/chmopenhouse/...` with long or special names.

**Root causes:**
- `url_to_path()` does basic query/hash stripping and extension logic, but does almost no sanitization of the actual path components.
- No truncation of overly long filenames.
- No replacement of characters that are problematic on some filesystems.
- `sanitize_filename()` exists but is only used for the top-level output directory name.

**Proposed minimal changes:**
- Improve `url_to_path()` (or add a helper) to sanitize individual path segments:
  - Remove or replace dangerous characters (`/ \ : * ? " < > |`).
  - Truncate overly long segments.
  - Optionally percent-decode for nicer on-disk names (optional, behind define).
- Make the sanitization behavior controllable via config.h for "strict archival" vs "nice filenames".

## Design Principles for Fixes

- All changes must be compile-time (config.h) where possible — suckless style.
- No new dependencies.
- Keep the binary small and the code auditable.
- Prefer failing safely / warning over crashing or producing garbage archives.
- Improve robustness for the "primary archival" use case the user wants, without weakening safety for normal use.

## Implementation Order (Minimal Impact First)

1. Wire up `MAX_FILE_SIZE` enforcement (biggest bloat win).
2. Add `IGNORE_SSL_ERRORS` option (biggest old-web quality of life win).
3. Improve filename sanitization in url_to_path.
4. Make retry / timeout values more configurable + add simple slow-host backoff.
5. Update documentation and defaults if needed.


## Implemented Changes (2026-05)

All changes follow suckless principles: compile-time configuration, minimal diff, no new dependencies.

### 1. MAX_FILE_SIZE Enforcement (anti-bloat)
- Wired `CURLOPT_MAXFILESIZE` into libcurl.
- Added post-download size check in fetch_url.
- Added guard in fetch_and_encode before base64 inlining.
- This directly addresses the lowendmac 250MB bloat case.

### 2. IGNORE_SSL_ERRORS Option (old web support)
- New define in config.h (default 0 = strict).
- When set to 1, relaxes SSL peer/host verification.
- This makes archiving old Geocities-style mirrors and dead personal sites practical without constant warnings or early failure.

### 3. Improved Filename Sanitization
- Added character sanitization in url_to_path (replaces \ : * ? " < > | with _).
- Helps with the long/weird paths seen in vintage-computer dumps.

### 4. Better Defaults for Archival Use
- Increased `FETCH_MAX_RETRIES` from 3 → 5.
- Increased default `REQUEST_TIMEOUT` from 60s → 90s.
- These give more resilience on slow historical hosts without changing behavior drastically for normal use.

These changes make sbot significantly more robust as a primary archival tool while keeping the codebase small and the philosophy intact.

