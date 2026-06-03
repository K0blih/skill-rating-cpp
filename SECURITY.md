# Security Policy

## Supported Versions

Version 1.x is the supported release line. Security fixes are handled on the
default branch and included in subsequent patch or minor releases.

## Reporting a Vulnerability

Please report security issues privately to the repository maintainer instead of
opening a public issue. Include:

- affected commit or version
- steps to reproduce
- expected impact
- any suggested fix or mitigation

The HTTP service is intended for local development and controlled internal use.
Do not expose it directly to the public internet without adding appropriate
deployment controls such as TLS termination, authentication, request limits,
logging, monitoring, and a reverse proxy.
