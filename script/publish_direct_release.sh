#!/usr/bin/env bash
set -euo pipefail

platform="${1:?Expected macos, windows or linux}"
case "$platform" in
  macos) extension=zip ;;
  windows) extension=msi ;;
  linux) extension=AppImage ;;
  *) echo "Unsupported release platform: $platform" >&2; exit 1 ;;
esac

tag="${GITHUB_REF_NAME:?Missing release tag}"
version="$(python3 script/version.py)"
if [[ "$tag" != "v$version" && ! "$tag" =~ ^v${version//./\\.}-(beta|rc)\.[0-9]+$ ]]; then
  echo "Tag must match the Cargo application version." >&2
  exit 1
fi
test -f LICENSE || { echo 'Root GPL license is required for release.' >&2; exit 1; }
prerelease=false
[[ "$tag" == "v$version" ]] || prerelease=true

release_dir="$RUNNER_TEMP/ewaf-$platform-release"
mkdir -p "$release_dir/artifacts" "$release_dir/publish"
gh api "repos/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID/artifacts" \
  --paginate --jq ".artifacts[] | select(.expired == false and (.name | startswith(\"EWAF-$platform-\"))) | .name" \
  > "$release_dir/artifact-names"
if [[ ! -s "$release_dir/artifact-names" ]]; then
  echo "No verified $platform artifact was uploaded; nothing to publish."
  exit 0
fi

while IFS= read -r artifact; do
  gh run download "$GITHUB_RUN_ID" --repo "$GITHUB_REPOSITORY" \
    --name "$artifact" --dir "$release_dir/artifacts/$artifact"
done < "$release_dir/artifact-names"

while IFS= read -r -d '' package; do
  name="$(basename "$package")"
  if [[ "$name" != "EWAF-$version-$platform-"*".$extension" ]]; then
    echo "Unexpected $platform package name: $name" >&2
    exit 1
  fi
  sidecar="$package.sha256"
  if [[ ! -f "$sidecar" ]]; then
    echo "Missing checksum sidecar: $name" >&2
    exit 1
  fi
  expected="$(awk 'NR == 1 { print $1 }' "$sidecar")"
  actual="$(sha256sum "$package" | awk '{ print $1 }')"
  if [[ ! "$expected" =~ ^[[:xdigit:]]{64}$ || "$expected" != "$actual" ]]; then
    echo "Checksum mismatch: $name" >&2
    exit 1
  fi
  for source in "$sidecar" "$package"; do
    destination="$release_dir/publish/$(basename "$source")"
    if [[ -e "$destination" ]]; then
      cmp -s "$source" "$destination" || {
        echo "Conflicting same-name platform artifact: $destination" >&2
        exit 1
      }
    else
      cp "$source" "$destination"
    fi
  done
done < <(find "$release_dir/artifacts" -type f -name "*.$extension" -print0)

while IFS= read -r -d '' sidecar; do
  if [[ ! -f "${sidecar%.sha256}" ]]; then
    echo "Orphan checksum sidecar: $sidecar" >&2
    exit 1
  fi
done < <(find "$release_dir/artifacts" -type f -name '*.sha256' -print0)

if [[ -z "$(find "$release_dir/publish" -type f -name "*.$extension" -print -quit)" ]]; then
  echo "No verified $platform package was found; nothing to publish."
  exit 0
fi

if ! gh release view "$tag" >/dev/null 2>&1; then
  create_release() {
    gh release create "$tag" --target "$GITHUB_SHA" --title "EWAF $tag" \
      --draft \
      --notes "Platform artifacts are published independently after platform-specific verification. Unsigned direct downloads are not authenticated updater releases. Missing platforms remain tracked in the CI/CD failure report." \
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

upload_or_verify() {
  local file="$1" name verify_dir
  name="$(basename "$file")"
  if gh release view "$tag" --json assets --jq '.assets[].name' | grep -Fxq "$name"; then
    verify_dir="$(mktemp -d "$RUNNER_TEMP/ewaf-existing.XXXXXX")"
    gh release download "$tag" --repo "$GITHUB_REPOSITORY" \
      --pattern "$name" --dir "$verify_dir"
    cmp -s "$file" "$verify_dir/$name" || {
      echo "Existing release asset differs from verified package: $name" >&2
      exit 1
    }
    echo "Keeping verified existing release asset: $name"
  else
    gh release upload "$tag" "$file"
  fi
}

# Publish checksums first so a package is never visible without its sidecar.
while IFS= read -r -d '' sidecar; do
  upload_or_verify "$sidecar"
done < <(find "$release_dir/publish" -type f -name '*.sha256' -print0)
while IFS= read -r -d '' package; do
  upload_or_verify "$package"
done < <(find "$release_dir/publish" -type f -name "*.$extension" -print0)

if [[ "$(gh release view "$tag" --json isDraft --jq .isDraft)" == true ]]; then
  release_id="$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$tag" --jq .id)"
  gh api --method PATCH "repos/$GITHUB_REPOSITORY/releases/$release_id" \
    -F draft=false -F prerelease="$prerelease" >/dev/null
fi
