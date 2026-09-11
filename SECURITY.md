# Security Policy

## Supported version

Only the latest public-preview release is actively considered for fixes.

## Reporting a security issue

Please do not publish exploit details in a public issue. Contact the maintainer through the project's Discord and clearly mark the report as a security issue.

When sharing logs, screenshots, crash reports, or project files, redact passwords, API keys, access tokens, private URLs, email addresses, usernames, machine names, and local filesystem paths that you do not intend to make public.

If a credential or secret is ever committed, revoke or rotate it immediately. Removing it in a later commit is not sufficient because it may remain in Git history.

## Runtime binaries

The source repository does not commit NVIDIA proprietary runtime binaries. Precompiled release packages may include required NVIDIA runtime components under NVIDIA's applicable license terms. Do not upload third-party proprietary binaries to issues, pull requests, or source commits.
