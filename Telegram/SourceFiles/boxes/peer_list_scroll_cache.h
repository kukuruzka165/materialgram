/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "base/flat_map.h"

class PeerListRowsScrollCache final {
public:
	explicit PeerListRowsScrollCache(Fn<void()> stopped);

	void markScrolling();
	[[nodiscard]] bool scrolling() const {
		return _scrolling;
	}

	template <typename PaintToImage>
	void paintRow(
			QPainter &p,
			uint64 rowId,
			QSize physicalSize,
			int ratio,
			PaintToImage &&paintToImage) {
		auto &image = _images[rowId];
		if (image.size() != physicalSize) {
			if (_images.size() > kLimit) {
				_images.clear();
				image = _images[rowId];
			}
			image = QImage(physicalSize, QImage::Format_RGB32);
			image.setDevicePixelRatio(ratio);
			paintToImage(image);
		}
		p.drawImage(0, 0, image);
	}

	void invalidate(uint64 rowId);

private:
	static constexpr auto kLimit = 256;

	base::flat_map<uint64, QImage> _images;
	base::Timer _stopTimer;
	bool _scrolling = false;

};
