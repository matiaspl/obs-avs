#!/usr/bin/env bash

set -euo pipefail

repo="${GITHUB_REPOSITORY:-matiaspl/klaps}"

usage()
{
	cat <<EOF
Usage: $0 [--repo OWNER/REPO] PATH_TO_SIGNING_CERTIFICATES.p12

Uploads the macOS Developer ID signing and notarization credentials as GitHub
Actions repository secrets. The P12 should contain both the Developer ID
Application and Developer ID Installer certificates and private keys.
EOF
}

if [[ "${1:-}" == "--repo" ]]; then
	if (( $# < 3 )); then
		usage >&2
		exit 2
	fi
	repo="$2"
	shift 2
fi

if (( $# != 1 )); then
	usage >&2
	exit 2
fi

p12_file="$1"
if [[ ! -r "$p12_file" ]]; then
	echo "Certificate file is not readable: $p12_file" >&2
	exit 1
fi

if ! command -v gh >/dev/null; then
	echo "GitHub CLI (gh) is required." >&2
	exit 1
fi

if ! gh auth status --hostname github.com >/dev/null 2>&1; then
	echo "GitHub CLI is not authenticated. Run: gh auth login --hostname github.com" >&2
	exit 1
fi

read -r -p "Developer ID Application identity: " application_identity
read -r -p "Developer ID Installer identity: " installer_identity
read -r -p "Apple Developer Team ID: " team_id
read -r -p "Notarization Apple ID: " notarization_username
read -r -s -p "P12 password: " certificate_password
echo
read -r -s -p "Apple app-specific password for notarization: " notarization_password
echo

set_secret()
{
	local name="$1"
	local value="$2"
	printf '%s' "$value" | gh secret set "$name" --repo "$repo"
}

base64 < "$p12_file" | tr -d '\r\n' | gh secret set MACOS_SIGNING_CERT --repo "$repo"
set_secret MACOS_SIGNING_CERT_PASSWORD "$certificate_password"
set_secret MACOS_SIGNING_APPLICATION_IDENTITY "$application_identity"
set_secret MACOS_SIGNING_INSTALLER_IDENTITY "$installer_identity"
set_secret MACOS_TEAM_ID "$team_id"
set_secret MACOS_NOTARIZATION_USERNAME "$notarization_username"
set_secret MACOS_NOTARIZATION_PASSWORD "$notarization_password"

unset certificate_password notarization_password

echo "Configured signing secrets for $repo:"
gh secret list --repo "$repo" | awk '$1 ~ /^MACOS_/'
