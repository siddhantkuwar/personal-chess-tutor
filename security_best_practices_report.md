# Security Best-Practices Audit

## Executive summary

The complete pending worktree was reviewed before publication, including the C++ loopback HTTP service, persistence changes, React/Vite frontend, tests, product documentation, and new reference assets. No open Critical or High findings remain. One High local-service boundary issue and two defense-in-depth issues were fixed during the audit.

The repository contains no tracked environment files, private keys, credential files, runtime data, or visual-QA artifacts. The only credential-like string found is an intentional fake value in a redaction test. The frontend dependency audit reports zero known production vulnerabilities.

## Addressed findings

### SEC-001 — Hostile browser origins could write to the loopback API

- Rule ID: LOCAL-HTTP-001
- Severity: High — addressed
- Location: `src/service/http_server.cpp`, `valid_loopback_authority`, `valid_loopback_origin`, and `HttpServer::handle_client` (lines 69-92 and 1093-1109)
- Evidence: WebSocket upgrades already required a loopback `Origin`, but ordinary HTTP API requests were previously dispatched without validating either `Host` or an optional browser `Origin`.
- Impact: A malicious website could attempt cross-origin, state-changing requests against a running local service; a hostile host authority could also support DNS-rebinding-style access.
- Fix: Requests carrying a non-loopback `Host` or `Origin` now receive `403 Forbidden` before API or static-file dispatch. Loopback `localhost` and `127.0.0.1`, with validated optional ports, remain supported.
- Mitigation: The service continues to bind only to `INADDR_LOOPBACK`, keeps browser CORS closed, and independently validates WebSocket origins.
- False-positive notes: This is a single-user local service without account authentication, so public multi-user authorization findings do not apply. The browser-origin boundary is still required because loopback alone does not prevent cross-site requests.

### SEC-002 — Browser response hardening was incomplete

- Rule ID: REACT-HEADERS-001 / REACT-CSP-001
- Severity: Medium — addressed
- Location: `src/service/http_server.cpp`, `HttpServer::handle_client` (lines 1120-1128)
- Evidence: Responses previously set `X-Content-Type-Options` and a basic CSP but did not explicitly deny framing or restrict base, object, and form targets.
- Impact: Missing browser defenses increased the blast radius of future markup or navigation mistakes.
- Fix: Added `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`, a restrictive `Permissions-Policy`, and CSP directives for `base-uri`, `object-src`, `form-action`, and `frame-ancestors` without enabling `unsafe-inline` or `unsafe-eval`.
- Mitigation: React renders API strings through escaped JSX and the application loads no third-party scripts.
- False-positive notes: TLS and HSTS are intentionally out of scope because the service is loopback-only and not a public deployment.

### SEC-003 — Dynamic UI indicators depended on CSP-blocked inline styles

- Rule ID: REACT-CSP-001
- Severity: Low — addressed
- Location: `web/src/react/Board.tsx`, `EvaluationBar` (lines 47-59), and `web/src/react/App.tsx`, `AnalysisView` (lines 530-540)
- Evidence: Evaluation and job progress previously used React `style` attributes while the enforced CSP intentionally disallows inline styles.
- Impact: The indicators could fail silently and create pressure to weaken CSP with `unsafe-inline`.
- Fix: Replaced inline styles with SVG geometry attributes and the native `progress` element; the strict CSP remains unchanged.
- Mitigation: A source scan confirms no remaining React `style={{...}}` usage.
- False-positive notes: This was primarily a CSP-compatibility defect, not a direct injection path, because the values were numeric and locally computed.

## Checks performed

- Reviewed the complete Git diff and all untracked files intended for publication.
- Scanned for private keys, environment files, credential filenames, hard-coded secrets, dangerous DOM sinks, dynamic code execution, unsafe navigation, cross-window messaging, persistent auth tokens, service workers, and third-party script injection.
- Confirmed runtime data, private docs, build outputs, and visual-QA artifacts remain ignored.
- Ran `npm audit --omit=dev`: zero vulnerabilities.
- Built the React production bundle and ran all frontend geometry, review-state, and insight tests.
- Built the native project with AddressSanitizer, UndefinedBehaviorSanitizer, and warnings-as-errors.
- Ran the complete native test suite outside the network sandbox: all three test targets passed.
- Ran Clang Static Analyzer on the changed HTTP server, repository, and event-log code: no findings.
- Ran `git diff --check`: clean.

## Residual risk and limitations

- No dedicated history-aware secret scanner such as Gitleaks was installed. The current tree, full pending diff, tracked filenames, ignored sensitive paths, and credential-pattern matches were reviewed with repository-native tools.
- The standalone `restructure-plan.html` contains an `innerHTML` assignment sourced only from a fixed in-file constant table. It is not served by the application and has no untrusted data path, so it is not reported as an exploitable XSS finding.
