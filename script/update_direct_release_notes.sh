#!/usr/bin/env bash
set -euo pipefail

version="$(python3 script/version.py)"
case "${GITHUB_REF:?Missing release ref}" in
  refs/heads/main) tag="dev-${GITHUB_SHA:?Missing source commit}" ;;
  refs/tags/v*) tag="${GITHUB_REF_NAME:?Missing release tag}" ;;
  *) echo "No direct-release notes for $GITHUB_REF"; exit 0 ;;
esac
if ! gh release view "$tag" >/dev/null 2>&1; then
  echo "No verified platform published for $tag; no release notes to update."
  exit 0
fi

assets="$(gh release view "$tag" --json assets --jq '.assets[].name')"
has_verified_artifact() {
  local name="$1" directory expected actual
  grep -Fxq "$name" <<< "$assets" || return 1
  grep -Fxq "$name.sha256" <<< "$assets" || return 1
  directory="$(mktemp -d "$RUNNER_TEMP/ewaf-release-notes.XXXXXX")"
  gh release download "$tag" --repo "$GITHUB_REPOSITORY" \
    --pattern "$name" --pattern "$name.sha256" --dir "$directory"
  expected="$(awk 'NR == 1 { print $1 }' "$directory/$name.sha256")"
  actual="$(sha256sum "$directory/$name" | awk '{ print $1 }')"
  if [[ ! "$expected" =~ ^[[:xdigit:]]{64}$ || "$expected" != "$actual" ]]; then
    echo "Existing release checksum mismatch for $name" >&2
    exit 1
  fi
}

macos_arm64=unavailable
macos_x64=unavailable
windows_x64=unavailable
windows_arm64=unavailable
linux_x64=unavailable
linux_arm64=unavailable
has_verified_artifact "EWAF-$version-macos-arm64.zip" && macos_arm64=available
has_verified_artifact "EWAF-$version-macos-x86_64.zip" && macos_x64=available
has_verified_artifact "ewaf-$version-windows-x64.msi" && windows_x64=available
has_verified_artifact "ewaf-$version-windows-arm64.msi" && windows_arm64=available
has_verified_artifact "ewaf-$version-linux-x64.AppImage" && linux_x64=available
has_verified_artifact "ewaf-$version-linux-arm64.AppImage" && linux_arm64=available

gh release view "$tag" --json body --jq .body \
  | python3 script/update_release_notes.py \
      --status "macOS ARM64" "$macos_arm64" \
      --status "macOS x64" "$macos_x64" \
      --status "Windows x64" "$windows_x64" \
      --status "Windows ARM64" "$windows_arm64" \
      --status "Linux x64" "$linux_x64" \
      --status "Linux ARM64" "$linux_arm64" \
  > "$RUNNER_TEMP/ewaf-release-notes"
gh release edit "$tag" --notes-file "$RUNNER_TEMP/ewaf-release-notes"
