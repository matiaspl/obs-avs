# Release signing

When signing secrets are configured, the macOS build signs the plugin bundle
and installer on pushes to `main`, tag pushes, and signed manual builds. Tagged
signed builds additionally notarize both the ZIP and PKG and staple the
notarization ticket to the PKG when notarization secrets are configured. Builds
continue unsigned when repository credentials are unavailable. Pull requests
never receive signing secrets and produce unsigned test artifacts.

Full Developer ID signing and notarization use these GitHub Actions repository
secrets:

| Secret | Value |
| --- | --- |
| `MACOS_SIGNING_CERT` | Base64-encoded P12 containing the Developer ID Application and Developer ID Installer certificates and private keys |
| `MACOS_SIGNING_CERT_PASSWORD` | Password used when exporting the P12 |
| `MACOS_SIGNING_APPLICATION_IDENTITY` | Full identity, for example `Developer ID Application: Example Company (TEAMID)` |
| `MACOS_SIGNING_INSTALLER_IDENTITY` | Full identity, for example `Developer ID Installer: Example Company (TEAMID)` |
| `MACOS_TEAM_ID` | Apple Developer team ID |
| `MACOS_NOTARIZATION_USERNAME` | Apple ID used for notarization |
| `MACOS_NOTARIZATION_PASSWORD` | App-specific password for that Apple ID |

Export both Developer ID identities and their private keys from Keychain Access
into one password-protected P12. Then authenticate GitHub CLI and run:

```sh
gh auth login --hostname github.com
tool/configure-github-signing.sh /path/to/developer-id-certificates.p12
```

The helper sends values directly to `gh secret set`; it does not create a
base64 or plaintext credential file. To target a fork, pass
`--repo OWNER/REPO` before the P12 path.

To verify configuration without exposing values:

```sh
gh secret list --repo matiaspl/klaps | awk '$1 ~ /^MACOS_/'
```

Use **Actions > Plugin Build > Run workflow** for a test build. Signing is
required by default. Clear **Require Developer ID signing for the macOS
artifacts** only when an intentionally unsigned manual artifact is needed.
