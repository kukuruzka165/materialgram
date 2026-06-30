/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/peer_list_scroll_cache.h"

namespace {

constexpr auto kStopTimeout = crl::time(120);

} // namespace

PeerListRowsScrollCache::PeerListRowsScrollCache(Fn<void()> stopped) {
	_stopTimer.setCallback([=] {
		_scrolling = false;
		_images.clear();
		stopped();
	});
}

void PeerListRowsScrollCache::markScrolling() {
	_scrolling = true;
	_stopTimer.callOnce(kStopTimeout);
}

void PeerListRowsScrollCache::invalidate(uint64 rowId) {
	if (const auto i = _images.find(rowId); i != end(_images)) {
		_images.erase(i);
	}
}
