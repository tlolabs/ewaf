#!/usr/bin/env bash
set -euo pipefail

tag="${GITHUB_REF_NAME:?Missing release tag}"
version="$(python3 script/version.py)"
if ! gh release view "$tag" >/dev/null 2>&1; then
  echo "No verified platform published for $tag; no release notes to update."
  exit 0
fi

assets="$(gh release view "$tag" --json assets --jq '.assets[].name')"
has_verified_artifact() {
  local suffix="$1" name directory expected actual
  while IFS= read -r name; do
    [[ "$name" == EWAF-"$version"-*"$suffix" ]] || continue
    grep -Fxq "$name.sha256" <<< "$assets" || continue
    directory="$(mktemp -d "$RUNNER_TEMP/ewaf-release-notes.XXXXXX")"
    gh release download "$tag" --repo "$GITHUB_REPOSITORY" \
      --pattern "$name" --pattern "$name.sha256" --dir "$directory"
    expected="$(awk 'NR == 1 { print $1 }' "$directory/$name.sha256")"
    actual="$(sha256sum "$directory/$name" | awk '{ print $1 }')"
    if [[ ! "$expected" =~ ^[[:xdigit:]]{64}$ || "$expected" != "$actual" ]]; then
      echo "Existing release checksum mismatch for $name" >&2
      exit 1
    fi
    return 0
  done <<< "$assets"
  return 1
}

macos_arm64=unavailable
macos_x64=unavailable
windows_x64=unavailable
windows_arm64=unavailable
linux_x64=unavailable
linux_arm64=unavailable
has_verified_artifact '-macos-arm64.zip' && macos_arm64=available
has_verified_artifact '-macos-x86_64.zip' && macos_x64=available
has_verified_artifact '-windows-x64.msi' && windows_x64=available
has_verified_artifact '-windows-arm64.msi' && windows_arm64=available
has_verified_artifact '-linux-x64.AppImage' && linux_x64=available
has_verified_artifact '-linux-arm64.AppImage' && linux_arm64=available

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
