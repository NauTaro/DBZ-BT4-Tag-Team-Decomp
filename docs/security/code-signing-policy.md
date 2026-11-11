# Code signing policy

## Current status

We are applying to SignPath Foundation for managed Authenticode signing. The project has not yet been approved and no Foundation certificate is configured. Existing Windows downloads must not be described as Authenticode-signed until their embedded signatures have been verified. The detached `.sig` files protect automatic update ZIPs using Ed25519 and are a separate mechanism.

If the application is approved, the required attribution will be: Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

## Maintainer roles

- Author and reviewer: [NauTaro](https://github.com/NauTaro).
- Release and signing approver: [NauTaro](https://github.com/NauTaro).

The signing provider's approval and account-security requirements must be satisfied before production signing is enabled. Provider approval must not be bypassed by supplying a test certificate or accepting a signature from an unapproved publisher.

## Scope and verification

The requested scope is the game executable and the setup executable built from this repository. The setup file is also distributed as `Install.exe`; both names receive identical signed bytes. Third-party executables and libraries retain their own provenance and copyright and are not relabeled as first-party software.

GitHub-hosted workflows build the unsigned files from source, upload them as workflow artifacts, and submit those artifacts to SignPath. The signing job checks Windows trust validation, the configured certificate thumbprint and a signing timestamp. It also checks that the signing service did not change the executable's code, resources or data. Only the checksum, certificate directory, alignment padding and appended signature may change.

Once SignPath is selected in the release configuration, a failed or unavailable signing job blocks draft replacement instead of falling back to unsigned Windows artifacts. SHA-256 inventories and the detached update signatures must be generated from the final signed bytes.

Signing identifies the publisher and helps verify integrity. It is not an antivirus verdict and does not promise that SmartScreen or every security product will immediately trust a new release.

## Data and services

See the [privacy notice](privacy.md) for the game's online, update and download services. The signing service receives only the first-party build artifacts and build provenance, not player credentials, game saves, ISO images or extracted game data.
