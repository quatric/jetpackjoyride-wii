#!/bin/sh
# Homebrew Channel zip: apps/jetpackjoyride/{boot.dol,meta.xml}. Game assets come from the APK and are NOT redistributed;
# pass an SD-layout folder as $1 (e.g. one made by the extraction tools) to bundle them for your own use only.
set -e
cd "$(dirname "$0")"
out=$(mktemp -d); d=$out/apps/jetpackjoyride; mkdir -p "$d"
cp app/jetpackjoyride_wii.dol "$d/boot.dol"; cp app/meta.xml "$d/"
[ -n "$1" ] && cp -R "$1"/. "$d"/ && cp app/jetpackjoyride_wii.dol "$d/boot.dol"
(cd "$out" && zip -qr "$OLDPWD/jetpackjoyride-wii-hbc.zip" apps)
rm -rf "$out"; echo "wrote $(pwd)/jetpackjoyride-wii-hbc.zip"
