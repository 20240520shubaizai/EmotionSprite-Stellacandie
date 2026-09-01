# Changelog

## [0.9.0-rc.1] - 2026-09-01

### Added

- Bundled Python Agent Runtime with structured orchestration, RAG, tools and evaluation;
- local-first Repository and schema migration layers with optional typed synchronization;
- standard per-user Windows installer, Start Menu integration and upgrade registration;
- release allow-list, sensitive-content checks and SHA-256 generation;
- public user, architecture, privacy, testing and engineering documentation.

### Fixed

- prevent multiple desktop instances from competing for the same SQLite data;
- invalidate deleted memories across storage, RAG and model context;
- parse Chinese relative reminders and prevent pending-intent cross-talk;
- stop running-program installation before any files change;
- obey exact summary point counts and avoid assistant-side secret echo;
- keep the Agent sync database in the user data directory instead of the installation directory;
- replace stale conflicting Stellacandie shortcuts through the standard installer.

### Known limitations

- unsigned Windows binaries may trigger SmartScreen;
- real HTTPS/MySQL synchronization requires a separately deployed service;
- Qt UI Automation accessibility needs further work;
- this is a release candidate, not a final 1.0 release.
