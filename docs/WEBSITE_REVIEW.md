# Website clarity and trust review

Reviewed on 2026-10-24 for the website clarity and trust increment in `docs/IMPLEMENTATION_PLAN.md`.

## Acceptance and evidence

- Capability cards retain their four descriptive headings without decorative sequence numbers. Cards use content height, with reduced padding; section padding is 40-64px instead of 80-128px.
- The hardware-tested statement links to committed validation. Before the flashing command, the installation section names Kamba Abudu (`CITATION.cff`) and maintainer `@kabudu` (`GOVERNANCE.md`), links source and limitations, and identifies validation as project-run rather than independent certification.
- The footer names the maintainer, links GitHub Issues for contact/support, the security policy for private reporting instructions and the licence. It labels its date as the website update date, not a firmware release or retest date.
- Browser review of `scripts/build-site.sh build/site-preview` at 1280px and 390px covered the full page: readable headings, compact cards, provenance before flashing, wrapping footer links and preserved developer-release limitations. At 390px and 320px, DOM measurements confirmed document width equals viewport width; long commands retain their own horizontal scrolling.
- Existing site contracts plus the provenance/contact contract run under `tests/test_site.py`. The generated-site validator checks local assets, navigation fragments and accessible image text.
- Authoritative `./scripts/ci-local.sh` passed using the local ESP-IDF checkout. The reviewed PR head is checked again and its result recorded in the PR before merge.

## Review boundaries

Lazarus self-review found no runtime, firmware, credential, external dependency or hosted-workflow changes. The security policy's obsolete OTA-unimplemented sentence was aligned with the existing board-qualification evidence; no new qualification is claimed. The date is maintained explicitly when the website changes.

This review establishes presentation and navigation behaviour, not a practitioner comprehension study, independent security review, new hardware validation or production suitability. R18 records the residual assurance-misinterpretation risk. Firmware release version and installation commands are unchanged.
