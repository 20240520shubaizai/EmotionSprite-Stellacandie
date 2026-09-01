# Security Policy

Security fixes currently target the latest release candidate only.

Do not publish API keys, credentials, private conversations, user databases or secret-memory contents in a public Issue. Contact the repository owner through the GitHub profile and provide a minimal reproduction with redacted logs.

## Data boundaries

- API keys are stored in Windows Credential Manager;
- SQLite is the local source of truth and synchronization is disabled by default;
- secret and local-only memories are excluded from ordinary traces and sync;
- images and local paths are not part of the default synchronization contract;
- destructive deletion requires an exact confirmation phrase.

The current Windows release is not Authenticode-signed. Verify downloads against the attached `SHA256SUMS.txt`.
