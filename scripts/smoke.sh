#!/bin/bash
# Smoke test of the built frontend: starts a viewer and a managed hub from one
# binary, drives both in a headless Chrome (the pages load, every tab opens,
# nothing throws, no asset is missing), and stops them. Build first:
#
#     ./scripts/build-html.sh && cmake --build build --target AIS-catcher
#     ./scripts/smoke.sh [binary]            default build/AIS-catcher
#
# VIEWER_PORT (8125) and HUB_PORT (8140) pick the ports, CHROME the browser.
# Sharing to the community feed is off in both instances.
set -euo pipefail
cd "$(dirname "$0")/.."
BIN=${1:-build/AIS-catcher}
BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")   # absolute: the hub starts in its own directory
VIEWER_PORT=${VIEWER_PORT:-8125}
HUB_PORT=${HUB_PORT:-8140}
TMP=$(mktemp -d)
# the hub runs its engine as a child: stop the children before the parents
cleanup() { for p in "${VPID:-}" "${HPID:-}"; do [ -n "$p" ] && { pkill -P "$p" 2>/dev/null; kill "$p" 2>/dev/null; } || true; done; rm -rf "$TMP"; }
trap cleanup EXIT

# a viewer needs an input to start; a port nothing listens on keeps it waiting, with no data
"$BIN" -X off -o 0 -t txt 127.0.0.1 1 -N "$VIEWER_PORT" >/dev/null 2>"$TMP/viewer.err" &
VPID=$!

mkdir "$TMP/hub"
cat > "$TMP/hub/config.json" <<EOF
{ "config": "aiscatcher", "version": 1, "engine": "off", "sharing": false,
  "control": { "wizard": false,
               "viewer": { "share_loc": false, "realtime": true, "decoder": true, "log": false,
                           "station": "smoke", "file": "$TMP/hub/stats", "backup": 10 } } }
EOF
(cd "$TMP/hub" && exec "$BIN" -E "$TMP/hub/config.json" 127.0.0.1:"$HUB_PORT") >/dev/null 2>"$TMP/hub.err" &
HPID=$!

for i in $(seq 1 40); do
    curl -sf -o /dev/null "http://127.0.0.1:$VIEWER_PORT/" && curl -sf -o /dev/null "http://127.0.0.1:$HUB_PORT/" && break
    sleep 0.5
done
for port in "$VIEWER_PORT" "$HUB_PORT"; do
    curl -sf -o /dev/null "http://127.0.0.1:$port/" || { echo "nothing answers on $port after 20 s:"; cat "$TMP/viewer.err" "$TMP/hub.err" | tail -5; exit 1; }
done

node frontend/test/smoke.mjs "http://127.0.0.1:$VIEWER_PORT/?welcome=false" "http://127.0.0.1:$HUB_PORT/"
