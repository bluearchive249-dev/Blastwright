#!/usr/bin/env bash
# Copyright © 2026 Grimsfrost. All rights reserved.
#
# vcpkg's downloader for the Windows build (X_VCPKG_ASSET_SOURCES x-script): vcpkg-fetch.sh <url> <sha512> <dst>
# Some vcpkg ports fetch patches as github.com/<owner>/<repo>/commit/<sha>.patch, which GitHub often answers with 503
# to build machines. This tries the URL patiently, and for a commit patch falls back to fetching the commit with git
# (which goes through) and writing the same patch with git format-patch, kept only if its SHA-512 matches.
url="$1"
want="$(echo "$2" | tr 'A-F' 'a-f')"
dst="$(cygpath -u "$3" 2>/dev/null || echo "$3")"
tmp="$dst.part"

ok() { [ -f "$1" ] && [ "$(sha512sum "$1" | cut -d' ' -f1)" = "$want" ]; }
keep() { mv -f "$1" "$dst"; exit 0; }

for i in 1 2 3; do
  if curl -sSfL --retry 2 -o "$tmp" "$url" && ok "$tmp"; then keep "$tmp"; fi
  case "$url" in https://github.com/*) sleep $((i * 10)) ;; *) break ;; esac
done

if [[ "$url" =~ ^https://github\.com/([^/]+)/([^/]+)/commit/([0-9a-f]{40})\.(patch|diff) ]]; then
  owner="${BASH_REMATCH[1]}"; repo="${BASH_REMATCH[2]}"; commit="${BASH_REMATCH[3]}"; kind="${BASH_REMATCH[4]}"
  g="${RUNNER_TEMP:-/tmp}/gitpatch-$commit"
  rm -rf "$g"; git init -q "$g"
  if git -C "$g" fetch -q --depth 2 "https://github.com/$owner/$repo.git" "$commit"; then
    full=""; [[ "$url" == *full_index=1* ]] && full="--full-index"
    if [ "$kind" = "patch" ]; then
      # GitHub's patch is git format-patch's; its signature (the server's git version) is unknown, so try them all
      base="$tmp.base"
      git -C "$g" format-patch -1 --stdout $full --no-signature "$commit" > "$base"
      cp "$base" "$tmp" && ok "$tmp" && keep "$tmp"
      for minor in $(seq 20 60); do
        for fix in $(seq 0 15); do
          { cat "$base"; printf -- '-- \n2.%d.%d\n\n' "$minor" "$fix"; } > "$tmp"
          if ok "$tmp"; then rm -f "$base"; keep "$tmp"; fi
        done
      done
      rm -f "$base"
    else
      git -C "$g" diff $full "$commit~1" "$commit" > "$tmp" && ok "$tmp" && keep "$tmp"
    fi
  fi
  echo "vcpkg-fetch: the patch made from $owner/$repo@$commit doesn't match its SHA-512" >&2
fi
rm -f "$tmp"
exit 1
