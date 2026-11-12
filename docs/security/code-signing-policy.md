# Code signing policy

## Current status

As of November 12, 2026, SignPath Foundation has provisioned the BT4decomp OSS organization and project. The account has a valid self-signed test certificate. The production certificate is still marked `CSR PENDING`, and its release signing policy is not yet valid. Existing Windows downloads must not be described as Authenticode-signed until their embedded production signatures have been verified. The detached `.sig` files protect automatic update ZIPs using Ed25519 and are a separate mechanism.

Code signing service provided by [SignPath.io](https://signpath.io); production certificate issuance by [SignPath Foundation](https://signpath.org) is pending.

## Maintainer roles

- Author and reviewer: [NauTaro](https://github.com/NauTaro).
- Release and signing approver: [NauTaro](https://github.com/NauTaro).

The signing provider's approval and account-security requirements must be satisfied before production signing is enabled. Provider approval must not be bypassed by supplying a test certificate or accepting a signature from an unapproved publisher.

## Scope and verification

The requested scope is the game executable and the setup executable built from this repository. The setup file is also distributed as `Install.exe`; both names receive identical signed bytes. Third-party executables and libraries retain their own provenance and copyright and are not relabeled as first-party software.

GitHub-hosted workflows build the unsigned files from source, upload them as workflow artifacts, and submit those artifacts to SignPath. The signing job checks Windows trust validation, the configured certificate thumbprint and a signing timestamp. It also checks that the signing service did not change the executable's code, resources or data. Only the checksum, certificate directory, alignment padding and appended signature may change.

Once SignPath is selected in the release configuration, a failed or unavailable signing job blocks draft replacement instead of falling back to unsigned Windows artifacts. SHA-256 inventories and the detached update signatures must be generated from the final signed bytes.

Signing identifies the publisher and helps verify integrity. It is not an antivirus verdict and does not promise that SmartScreen or every security product will immediately trust a new release.

## Maintainer setup

The `bt4-windows` artifact configuration in SignPath must match [signpath-bt4-windows.xml](signpath-bt4-windows.xml). It signs only the two first-party executables. The `version` parameter records the build being submitted. Both the production approval process and trusted GitHub source verification remain enabled.

Configure these repository Actions variables from the organization's project details:

| Variable | Value |
| --- | --- |
| `SIGNPATH_ORGANIZATION_ID` | `b2cd0df8-5b52-4803-bd1c-db6982ce1094` |
| `SIGNPATH_PROJECT_SLUG` | `DBZ-BT4-Tag-Team-Decomp` |
| `SIGNPATH_POLICY_SLUG` | `release-signing` |
| `BT4_WINDOWS_TEST_SIGNER_THUMBPRINT` | SHA-1 thumbprint shown on the test certificate details page |
| `BT4_WINDOWS_SIGNER_THUMBPRINT` | SHA-1 thumbprint of the issued production certificate, once available |

Store the CI builds user's submitter token directly as the `SIGNPATH_API_TOKEN` Actions secret. Never put it in a source file, issue, build log, or repository variable.

Run the **BT4 release** workflow manually on `main` with `signpath_test: true` to validate the GitHub connector using `test-signing`. This run creates only short-lived Actions test artifacts. It does not create or replace releases. Test outputs are explicitly marked and must not be distributed to players or installed as trusted root certificates.

After the Foundation issues the production certificate, verify its identity and fingerprint, configure `BT4_WINDOWS_SIGNER_THUMBPRINT`, and set `BT4_WINDOWS_SIGNING` to `signpath`. Production signing must pass Windows trust validation and timestamp validation before repackaging. A test-signing result does not satisfy that requirement. Existing published assets require a separate reviewed replacement of both the Windows ZIP and its matching Ed25519 update signature; the draft workflow will not overwrite an already published release.

## Data and services

See the [privacy notice](privacy.md) for the game's online, update and download services. The signing service receives only the first-party build artifacts and build provenance, not player credentials, game saves, ISO images or extracted game data.
