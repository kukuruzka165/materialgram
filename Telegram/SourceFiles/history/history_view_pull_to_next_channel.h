/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"
#include "ui/effects/animations.h"

class History;
class HistoryInner;
class QEvent;
class QWheelEvent;

namespace Ui {
class RpWidget;
class ContinuousScroll;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace HistoryView {

class PullToNextChannel final {
public:
	PullToNextChannel(
		not_null<Ui::RpWidget*> parent,
		not_null<Ui::ContinuousScroll*> scroll,
		not_null<Window::SessionController*> controller);
	~PullToNextChannel();

	void attachToContent(not_null<HistoryInner*> inner);

	void setHistory(History *history);

	void updateGeometry();

private:
	class Indicator;
	class HintOverlay;

	[[nodiscard]] bool active() const;
	[[nodiscard]] bool atBottom() const;
	[[nodiscard]] bool processWheel(not_null<QWheelEvent*> e);
	[[nodiscard]] bool applyDelta(float64 deltaX, float64 deltaY);
	[[nodiscard]] bool release();
	void push(float64 offset, bool ready, bool visible, History *next);
	void render(bool ready);
	void startExpand(bool ready);
	void applyShift(int shift);
	void startRetract(float64 fromAccumulated, History *next);
	void clearState();
	void reset();
	void jumpWhenReady(not_null<History*> next, crl::time waited);
	void jumpTo(not_null<History*> history);

	const not_null<Ui::RpWidget*> _parent;
	const not_null<Ui::ContinuousScroll*> _scroll;
	const not_null<Window::SessionController*> _controller;
	const base::unique_qptr<Indicator> _indicator;
	const base::unique_qptr<HintOverlay> _hint;

	QPointer<HistoryInner> _inner;
	History *_history = nullptr;
	History *_next = nullptr;

	base::unique_qptr<QObject> _filter;
	Ui::Animations::Simple _retract;
	Ui::Animations::Simple _expand;

	float64 _pushOffset = 0.;
	bool _pushVisible = false;
	History *_pushNext = nullptr;

	float64 _accumulated = 0.;
	float64 _offset = 0.;
	float64 _swipeX = 0.;
	float64 _swipeY = 0.;
	bool _engaged = false;
	bool _reached = false;
	bool _gaveUp = false;
	bool _swallowMomentum = false;

};

} // namespace HistoryView
