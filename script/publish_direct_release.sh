#!/usr/bin/env bash
set -euo pipefail

platform="${1:?Expected macos, windows or linux}"
arch="${2:?Expected release architecture}"
version="$(python3 script/version.py)"
case "$platform/$arch" in
  macos/arm64) artifact_name=macos-26; package_name="EWAF-$version-macos-arm64.zip" ;;
  macos/x86_64) artifact_name=macos-15-intel; package_name="EWAF-$version-macos-x86_64.zip" ;;
  windows/x64) artifact_name=EWAF-windows-x64; package_name="ewaf-$version-windows-x64.msi" ;;
  windows/arm64) artifact_name=EWAF-windows-ARM64; package_name="ewaf-$version-windows-arm64.msi" ;;
  linux/x64) artifact_name=EWAF-linux-ubuntu-24.04; package_name="ewaf-$version-linux-x64.AppImage" ;;
  linux/arm64) artifact_name=EWAF-linux-ubuntu-24.04-arm; package_name="ewaf-$version-linux-arm64.AppImage" ;;
  *) echo "Unsupported release architecture: $platform/$arch" >&2; exit 1 ;;
esac

case "${GITHUB_REF:?Missing release ref}" in
  refs/heads/main)
    mode=development
    if [[ "$(python3 script/development_release_eligible.py)" != true ]]; then
      echo 'Documentation-only change; no development release.'
      exit 0
    fi
    [[ "${GITHUB_SHA:?Missing source commit}" =~ ^[[:xdigit:]]{40}$ ]] || {
      echo 'Development releases require a full source commit SHA.' >&2
      exit 1
    }
    tag="dev-$GITHUB_SHA"
    prerelease=true
    release_title="EWAF $version development ${GITHUB_SHA:0:12}"
    release_notes="Development prerelease for commit $GITHUB_SHA. Platform packages passed available automated build, package, integrity and functional checks. Linux and Windows ARM64 hardware/runtime coverage remains best effort where unavailable. Unsigned direct downloads are not authenticated automatic updates. Missing platforms remain tracked in the CI/CD failure report."
    if [[ "$platform" == macos ]]; then
      artifact_name="EWAF-development-macos-$artifact_name"
    fi
    ;;
  refs/tags/v*)
    mode=tagged
    tag="${GITHUB_REF_NAME:?Missing release tag}"
    if [[ "$tag" != "v$version" && ! "$tag" =~ ^v${version//./\\.}-(beta|rc)\.[0-9]+$ ]]; then
      echo 'Tag must match the Cargo application version.' >&2
      exit 1
    fi
    prerelease=false
    [[ "$tag" == "v$version" ]] || prerelease=true
    release_title="EWAF $tag"
    release_notes='Platform artifacts are published independently after architecture-specific verification. Unsigned direct downloads are not authenticated updater releases. Missing platforms remain tracked in the CI/CD failure report.'
    if [[ "$platform" == macos ]]; then
      artifact_name="EWAF-macos-$artifact_name"
    fi
    ;;
  *) echo "Unsupported release ref: $GITHUB_REF" >&2; exit 1 ;;
esac
test -f LICENSE || { echo 'Root GPL license is required for release.' >&2; exit 1; }

release_dir="$RUNNER_TEMP/ewaf-$platform-$arch-release"
mkdir -p "$release_dir/artifacts" "$release_dir/publish"
gh api "repos/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID/artifacts" \
  --paginate --jq ".artifacts[] | select(.expired == false and .name == \"$artifact_name\") | .name" \
  > "$release_dir/artifact-names"
if [[ ! -s "$release_dir/artifact-names" ]]; then
  echo "No verified $platform/$arch artifact was uploaded; nothing to publish."
  exit 0
fi
if [[ "$(wc -l < "$release_dir/artifact-names")" -ne 1 ]] ||
    [[ "$(cat "$release_dir/artifact-names")" != "$artifact_name" ]]; then
  echo "Unexpected artifact inventory for $platform/$arch." >&2
  exit 1
fi
gh run download "$GITHUB_RUN_ID" --repo "$GITHUB_REPOSITORY" \
  --name "$artifact_name" --dir "$release_dir/artifacts"
package="$release_dir/artifacts/$package_name"
sidecar="$package.sha256"
if [[ ! -f "$package" || ! -f "$sidecar" ]]; then
  echo "Missing verified package or checksum sidecar: $package_name" >&2
  exit 1
fi
expected="$(awk 'NR == 1 { print $1 }' "$sidecar")"
actual="$(sha256sum "$package" | awk '{ print $1 }')"
if [[ ! "$expected" =~ ^[[:xdigit:]]{64}$ || "$expected" != "$actual" ]]; then
  echo "Checksum mismatch: $package_name" >&2
  exit 1
fi
cp "$package" "$sidecar" "$release_dir/publish/"

if ! gh release view "$tag" >/dev/null 2>&1; then
  create_release() {
    gh release create "$tag" --target "$GITHUB_SHA" --title "$release_title" \
      --draft \
      --notes "$release_notes" \
      "$@"
  }
  if [[ "$prerelease" == true ]]; then
    release_result=0
    create_release --prerelease || release_result=$?
  else
    release_result=0
    create_release || release_result=$?
  fi
  if [[ "$release_result" -ne 0 ]]; then
    for attempt in 1 2 3 4 5; do
      gh release view "$tag" >/dev/null 2>&1 && break
      sleep 2
    done
    gh release view "$tag" >/dev/null
  fi
fi

if [[ "$mode" == development ]]; then
  tag_sha="$(gh api "repos/$GITHUB_REPOSITORY/git/ref/tags/$tag" --jq .object.sha)"
  if [[ "$tag_sha" != "$GITHUB_SHA" ]]; then
    echo "Development tag points to a different source commit: $tag" >&2
    exit 1
  fi
fi

existing_assets="$(gh release view "$tag" --json assets --jq '.assets[].name')"
verify_existing() {
  local file="$1" name verify_dir
  name="$(basename "$file")"
  grep -Fxq "$name" <<< "$existing_assets" || return 0
  verify_dir="$(mktemp -d "$RUNNER_TEMP/ewaf-existing.XXXXXX")"
  gh release download "$tag" --repo "$GITHUB_REPOSITORY" \
    --pattern "$name" --dir "$verify_dir"
  cmp -s "$file" "$verify_dir/$name" || {
    echo "Existing release asset differs from verified package: $name" >&2
    exit 1
  }
  echo "Keeping verified existing release asset: $name"
}

# Check every existing asset before changing the release.
while IFS= read -r -d '' file; do
  verify_existing "$file"
done < <(find "$release_dir/publish" -type f -print0)

upload_missing() {
  local file="$1" name retry_dir
  name="$(basename "$file")"
  grep -Fxq "$name" <<< "$existing_assets" && return 0
  if gh release upload "$tag" "$file"; then
    return 0
  fi
  # A concurrent publisher may have uploaded the same name after our preflight.
  gh release view "$tag" --json assets --jq '.assets[].name' | grep -Fxq "$name" || return 1
  retry_dir="$(mktemp -d "$RUNNER_TEMP/ewaf-race.XXXXXX")"
  gh release download "$tag" --repo "$GITHUB_REPOSITORY" \
    --pattern "$name" --dir "$retry_dir"
  cmp -s "$file" "$retry_dir/$name" || {
    echo "Concurrent release asset differs from verified package: $name" >&2
    exit 1
  }
}

# Publish the checksum first so the package is never visible without it.
upload_missing "$release_dir/publish/$package_name.sha256"
upload_missing "$release_dir/publish/$package_name"

if [[ "$(gh release view "$tag" --json isDraft --jq .isDraft)" == true ]]; then
  release_id="$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$tag" --jq .id)"
  gh api --method PATCH "repos/$GITHUB_REPOSITORY/releases/$release_id" \
    -F draft=false -F prerelease="$prerelease" >/dev/null
fi
