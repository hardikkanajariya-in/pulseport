# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest  | Yes       |

## Security Model

PulsePort binds to `127.0.0.1` by default and is designed for local-only access. It does not implement authentication since it assumes a trusted local user.

**If you change the bind address to `0.0.0.0` or a non-loopback address, you are exposing the dashboard to your network without authentication.** This is not recommended.

## Reporting a Vulnerability

If you discover a security vulnerability, please report it responsibly:

1. **Do NOT open a public issue**
2. Email the maintainers at [security contact TBD] with:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
3. Allow up to 72 hours for an initial response
4. We will coordinate disclosure timing with you

## Known Security Boundaries

- **No TLS**: Traffic is plaintext HTTP. This is acceptable for loopback but not for network exposure.
- **No authentication**: Any local process can access the API.
- **POST CSRF protection**: Origin header validation prevents cross-site requests.
- **Delete audit**: All data deletions are logged to `deletions_audit`.
- **Input validation**: Request body size capped at 1 MB. SQL parameters are always bound, never interpolated.
