# Security policy

## Supported versions

Cirvane has no stable public release yet. Security fixes target the current `master` branch unless a future release policy states otherwise. Pre-release source and development images are not supported for safety-critical, unattended or production deployment.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting form under the repository Security tab. Do not report a suspected vulnerability in a public issue, pull request, discussion, benchmark transcript or social post.

Include the affected commit, hardware and build profile, reproduction steps, impact, relevant logs with secrets removed and whether the issue is already public. Never send working credentials, signing keys or unrelated personal data.

The maintainer aims to acknowledge a report within five business days and provide a status update within ten business days. These are response goals, not a service-level agreement. Disclosure timing will be coordinated according to severity, exploitability, fix availability and downstream impact. A report may be declined when it concerns an explicitly unsupported deployment, requires physical access already outside the threat model or cannot be reproduced, but the reason will be stated.

## Current security boundaries

- The USB shell is a privileged physical interface without user authentication.
- Wi-Fi credentials are session-only, hidden from command history and cleared on disconnect or reboot.
- Software image signatures and dual-slot rollback are tested, but hardware Secure Boot, flash encryption and production eFuse provisioning are not enabled by the project.
- Authenticated remote OTA transport is not implemented.
- Independent penetration testing and safety certification have not been completed.

See `docs/THREAT_MODEL.md` and `docs/RELEASE.md` for the complete boundary and stop-ship conditions.
