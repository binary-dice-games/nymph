#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Uploads the extension zip to its Chrome Web Store item and publishes it,
# which submits it for review (spec/07_release_pipeline/DESIGN.md section
# 4.7). The item, its listing and the API credentials are set up by hand
# once (docs/releasing.md).
#
#   sh publish.sh <nymph-extension-x.y.z.zip>
#
# Environment:
#   CHROME_EXTENSION_ID     the store item
#   CHROME_PUBLISHER_ID     the publisher (developer account) id
#   CHROME_CLIENT_ID, CHROME_CLIENT_SECRET, CHROME_REFRESH_TOKEN
#                           OAuth credentials with the chromewebstore scope
#                           (or CHROME_ACCESS_TOKEN, a token already made)
#   CHROME_API_BASE, CHROME_UPLOAD_BASE, CHROME_TOKEN_URL
#                           the Chrome Web Store API v2 and OAuth endpoints
#                           (defaults: Google's; tests point them at a stub)
#
# Exits 0 when the store accepted the package (or already has this
# version), 1 with the store's answer otherwise. Never prints a credential.

set -eu
zip=$1

fail() {
  echo "chrome publish: $*" >&2
  exit 1
}
say() {
  echo "chrome publish: $*"
}

: "${CHROME_EXTENSION_ID:?CHROME_EXTENSION_ID is not set}"
: "${CHROME_PUBLISHER_ID:?CHROME_PUBLISHER_ID is not set}"
[ -f "$zip" ] || fail "no file $zip"
api=${CHROME_API_BASE:-https://chromewebstore.googleapis.com}
upload_api=${CHROME_UPLOAD_BASE:-https://chromewebstore.googleapis.com/upload}
token_url=${CHROME_TOKEN_URL:-https://oauth2.googleapis.com/token}
item="publishers/$CHROME_PUBLISHER_ID/items/$CHROME_EXTENSION_ID"

# The value of a top-level string field of a one-object JSON answer.
json_field() { # <json> <field>
  printf '%s' "$1" | tr -d '\n' | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
}

# ── Access token ──────────────────────────────────────────────────────────────

if [ -n "${CHROME_ACCESS_TOKEN:-}" ]; then
  token=$CHROME_ACCESS_TOKEN
else
  : "${CHROME_CLIENT_ID:?CHROME_CLIENT_ID is not set}"
  : "${CHROME_CLIENT_SECRET:?CHROME_CLIENT_SECRET is not set}"
  : "${CHROME_REFRESH_TOKEN:?CHROME_REFRESH_TOKEN is not set}"
  answer=$(curl -sS --fail-with-body -X POST "$token_url" \
    --data-urlencode "client_id=$CHROME_CLIENT_ID" \
    --data-urlencode "client_secret=$CHROME_CLIENT_SECRET" \
    --data-urlencode "refresh_token=$CHROME_REFRESH_TOKEN" \
    --data-urlencode "grant_type=refresh_token" 2>&1) || fail "no access token (the OAuth credentials were refused): $answer"
  token=$(json_field "$answer" access_token)
  [ -n "$token" ] || fail "no access token in the OAuth answer"
fi
auth="Authorization: Bearer $token"

# Calls the API; prints the answer; returns curl's status (22 on HTTP errors).
call() { # <method> <url> [curl args...]
  method=$1
  url=$2
  shift 2
  curl -sS --fail-with-body -X "$method" -H "$auth" "$@" "$url" 2>&1
}

# A refusal because the store has this version already.
already_there() {
  printf '%s' "$1" | grep -Eiq 'version' && printf '%s' "$1" | grep -Eiq 'already|greater|higher|larger'
}

# ── Upload ────────────────────────────────────────────────────────────────────

say "uploading $(basename "$zip") to item $CHROME_EXTENSION_ID"
if ! answer=$(call POST "$upload_api/v2/$item:upload" -H "Content-Type: application/zip" --data-binary "@$zip"); then
  if already_there "$answer"; then
    say "the store already has this version; nothing to do"
    exit 0
  fi
  fail "upload refused: $answer"
fi
state=$(json_field "$answer" uploadState)
tries=0
while [ "$state" = IN_PROGRESS ] || [ "$state" = UPLOAD_IN_PROGRESS ]; do
  tries=$((tries + 1))
  [ $tries -le 30 ] || fail "the upload is still being processed after 30 checks"
  sleep "${CHROME_POLL_SECONDS:-10}"
  answer=$(call GET "$api/v2/$item:fetchStatus") || fail "status check refused: $answer"
  state=$(json_field "$answer" lastAsyncUploadState)
  [ -n "$state" ] || state=$(json_field "$answer" uploadState)
done
case $state in
  SUCCEEDED | SUCCESS) say "upload accepted" ;;
  *)
    if already_there "$answer"; then
      say "the store already has this version; nothing to do"
      exit 0
    fi
    fail "upload not accepted (state '$state'): $answer"
    ;;
esac

# ── Publish (submit for review) ───────────────────────────────────────────────

answer=$(call POST "$api/v2/$item:publish" -H "Content-Type: application/json" --data '{}') ||
  fail "publish refused: $answer"
say "submitted for review: $(json_field "$answer" state)"
