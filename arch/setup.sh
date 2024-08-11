#!/bin/bash

# This is a temporary workaround. Feel free to suggest how to do this properly using cmake.

mkdir -p usr/bin
mkdir -p usr/share/applications
mkdir -p usr/share/dbus-1/services
mkdir -p usr/share/icons/hicolor/symbolic/apps
mkdir -p usr/share/metainfo

for icon_size in 16 32 48 64 128 256 512; do
	icon_dir="usr/share/icons/hicolor/${icon_size}x${icon_size}/apps"
	mkdir -p "$icon_dir"
	cp "../Telegram/Resources/art/icon${icon_size}.png" "$icon_dir/materialgram.png"
done

cp ../Telegram/Resources/icons/tray_monochrome.svg usr/share/icons/hicolor/symbolic/apps/materialgram-symbolic.svg
cp ../out/Release/materialgram usr/bin/
cp ../lib/xdg/io.github.kukuruzka165.materialgram.desktop usr/share/applications/
cp ../lib/xdg/io.github.kukuruzka165.materialgram.metainfo.xml usr/share/metainfo/
cp ../lib/xdg/io.github.kukuruzka165.materialgram.service usr/share/dbus-1/services/
sed -i 's|Exec=@CMAKE_INSTALL_FULL_BINDIR@/materialgram|Exec=/usr/bin/materialgram|' usr/share/dbus-1/services/io.github.kukuruzka165.materialgram.service
