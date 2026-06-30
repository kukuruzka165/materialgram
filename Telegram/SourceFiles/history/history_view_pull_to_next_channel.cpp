/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/history_view_pull_to_next_channel.h"

#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/event_filter.h"
#include "base/platform/base_platform_haptic.h"
#include "data/data_chat_filters.h"
#include "data/data_messages.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/components/sponsored_messages.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "history/history_inner_widget.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/storage_shared_media.h"
#include "support/support_preload.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/continuous_scroll.h"
#include "ui/widgets/elastic_scroll.h"
#include "ui/rect.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "ui/userpic_view.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_dialogs.h"

namespace HistoryView {
namespace {

constexpr auto kDirectionLock = 8.;
constexpr auto kResetReachedOn = 0.95;
constexpr auto kReleaseShowDuration = crl::time(250);
constexpr auto kReleaseHideDuration = crl::time(220);
constexpr auto kPanelDuration = crl::time(320);
constexpr auto kRetractDuration = crl::time(250);
constexpr auto kExpandDuration = crl::time(250);
constexpr auto kBounceDuration = crl::time(400);

[[nodiscard]] History *FindNextUnreadChannel(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> current) {
	auto &data = controller->session().data();
	const auto filterId = controller->activeChatsFilterCurrent();
	const auto list = filterId
		? data.chatsFilters().chatsList(filterId)
		: data.chatsList();
	for (const auto &row : list->indexed()->all()) {
		const auto history = row->history();
		if (!history) {
			continue;
		}
		const auto peer = history->peer;
		if (peer != current
			&& peer->isBroadcast()
			&& history->unreadCount() > 0) {
			return history;
		}
	}
	return nullptr;
}

[[nodiscard]] Storage::SharedMediaResult TopPinnedSlice(
		not_null<PeerData*> peer) {
	return peer->session().storage().snapshot(Storage::SharedMediaQuery(
		Storage::SharedMediaKey(
			peer->id,
			MsgId(0), // topicRootId
			PeerId(0), // monoforumPeerId
			Storage::SharedMediaType::Pinned,
			ServerMaxMsgId - 1),
		1,
		1));
}

void PreloadPinnedBar(not_null<History*> next) {
	const auto peer = next->peer;
	if (TopPinnedSlice(peer).count.has_value()) {
		return;
	}
	peer->session().api().requestSharedMedia(
		peer,
		MsgId(0), // topicRootId
		PeerId(0), // monoforumPeerId
		Storage::SharedMediaType::Pinned,
		ServerMaxMsgId - 1,
		Data::LoadDirection::Before);
}

[[nodiscard]] bool PinnedBarReady(not_null<History*> next) {
	const auto peer = next->peer;
	const auto slice = TopPinnedSlice(peer);
	if (!slice.count.has_value()) {
		return false;
	} else if (slice.messageIds.empty()) {
		return true;
	}
	const auto id = FullMsgId(peer->id, slice.messageIds.back());
	return (peer->owner().message(id) != nullptr);
}

void PaintStroke(
		QPainter &p,
		const QPainterPath &path,
		const QColor &fg,
		float64 widthScale = 1.) {
	auto pen = QPen(fg);
	pen.setWidthF(st::historyPullNextArrowStroke * widthScale);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawPath(path);
}

void PaintArrow(QPainter &p, QPointF center, float64 size, const QColor &fg) {
	if (size <= 0.) {
		return;
	}
	const auto progress = size / st::historyPullNextArrowSize;
	const auto scale = style::ConvertFloatScale(progress);
	auto path = QPainterPath();
	path.moveTo(12.5, 4.);
	path.lineTo(12.5, 22.);
	path.moveTo(3.5, 12.);
	path.lineTo(12.5, 3.5);
	path.lineTo(21.5, 12.);
	auto transform = QTransform();
	transform.translate(
		center.x(),
		center.y() - st::historyPullNextArrowRaise);
	transform.scale(scale, scale);
	transform.translate(-12., 8.);
	PaintStroke(p, transform.map(path), fg, progress);
}

void PaintCheck(QPainter &p, QPointF center, float64 size, const QColor &fg) {
	if (size <= 0.) {
		return;
	}
	const auto s = size / 2.4;
	auto path = QPainterPath();
	path.moveTo(center + QPointF(-s, 0.));
	path.lineTo(center + QPointF(-s / 3., s * 0.7));
	path.lineTo(center + QPointF(s, -s * 0.7));
	PaintStroke(p, path, fg);
}

[[nodiscard]] float64 BounceProgress(float64 t) {
	const auto ease = [](float64 x) {
		if (x < 0.5) {
			return 4. * x * x * x;
		}
		const auto u = -2. * x + 2.;
		return 1. - u * u * u / 2.;
	};
	if (t < 0.45) {
		return ease(t / 0.45);
	} else if (t < 0.75) {
		return 1. - ease((t - 0.45) / 0.3) * 1.5;
	}
	return -0.5 + ease((t - 0.75) / 0.25) * 0.5;
}

} // namespace

class PullToNextChannel::Indicator final : public Ui::RpWidget {
public:
	Indicator(
		not_null<QWidget*> parent,
		not_null<const Ui::ChatStyle*> st);

	void setData(float64 offset, bool ready, History *next);
	void hideNow();

private:
	void paintEvent(QPaintEvent *e) override;

	const not_null<const Ui::ChatStyle*> _st;

	float64 _offset = 0.;
	bool _ready = false;
	History *_next = nullptr;
	QString _name;
	Ui::PeerUserpicView _userpic;
	Ui::Animations::Simple _releaseProgress;
	Ui::Animations::Simple _bounce;

};

PullToNextChannel::Indicator::Indicator(
	not_null<QWidget*> parent,
	not_null<const Ui::ChatStyle*> st)
: RpWidget(parent)
, _st(st) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_TranslucentBackground);
	hide();
}

void PullToNextChannel::Indicator::setData(
		float64 offset,
		bool ready,
		History *next) {
	if (_next != next) {
		_next = next;
		_userpic = {};
		_name = next ? next->peer->name() : QString();
		if (next) {
			next->peer->loadUserpic();
		}
	}
	_offset = offset;
	if (_ready != ready) {
		_ready = ready;
		_releaseProgress.start(
			[=] { update(); },
			ready ? 0. : 1.,
			ready ? 1. : 0.,
			ready ? kReleaseShowDuration : kReleaseHideDuration,
			anim::easeOutQuint);
		if (ready) {
			_bounce.start(
				[=] { update(); },
				0.,
				1.,
				kBounceDuration,
				anim::linear);
		}
	}
	if (offset <= 0.) {
		hide();
		return;
	} else if (isHidden()) {
		show();
	}
	update();
}

void PullToNextChannel::Indicator::hideNow() {
	_offset = 0.;
	_ready = false;
	_releaseProgress.stop();
	_bounce.stop();
	hide();
}

void PullToNextChannel::Indicator::paintEvent(QPaintEvent *e) {
	const auto offset = _offset;
	if (offset <= st::lineWidth) {
		return;
	}

	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);

	const auto card = float64(st::historyPullNextExpand);
	const auto progress = (card > 0.)
		? std::clamp(offset / card, 0., 1.)
		: 0.;
	const auto release = _releaseProgress.value(_ready ? 1. : 0.);
	const auto alpha = std::clamp(progress, 0., 1.);
	const auto h = float64(height());
	const auto cx = width() / 2.;
	const auto avatar = float64(st::historyPullNextAvatar);
	const auto circleRadius = avatar / 2.;
	const auto bg = _st->msgServiceBg()->c;
	const auto fg = _st->msgServiceFg()->c;
	const auto bounceOffset = -BounceProgress(_bounce.value(1.))
		* st::historyPullNextBounce;

	const auto widthRadius = std::clamp(
		circleRadius * (offset - st::historyPullNextSkip)
			/ (card - st::historyPullNextSkip),
		0.,
		circleRadius);
	const auto widthRadius2 = std::max(
		0.,
		std::min(
			circleRadius * progress,
			offset / 2. - st::historyPullNextSkip * progress));
	const auto size
		= (widthRadius2 * 2. - st::historyPullNextInset) * (1. - release)
		+ avatar * release;

	if (release < 1.) {
		const auto capsuleTop = h - offset;
		const auto capsuleBottom = h
			+ (-st::historyPullNextSkip * (1. - release)
				+ (-offset + avatar) * release);
		auto rect = QRectF(
			cx - widthRadius,
			capsuleTop,
			2. * widthRadius,
			capsuleBottom - capsuleTop);
		auto bgAlpha = alpha;
		if (release > 0.) {
			const auto inset = st::historyPullNextInset * release;
			rect = rect - Margins(inset);
			bgAlpha = alpha * (1. - release);
		}
		if (rect.width() > 0. && rect.height() > 0.) {
			auto capsule = QPainterPath();
			capsule.addRoundedRect(rect, widthRadius, widthRadius);

			p.setOpacity(bgAlpha);
			p.setPen(Qt::NoPen);
			p.setBrush(bg);
			p.drawPath(capsule);

			const auto arrowCy = h
				+ (-offset
					+ st::historyPullNextArrowSize
					+ st::historyPullNextSkip * (1. - progress)
					- st::historyPullNextRise * release);
			p.save();
			p.setClipRect(rect);
			p.setOpacity(alpha * (1. - release));
			PaintArrow(
				p,
				QPointF(cx, arrowCy),
				st::historyPullNextArrowSize * progress,
				fg);
			p.restore();
		}
	}

	const auto name = _next
		? _name
		: tr::lng_pull_no_unread_channels(tr::now);
	if (release > 0. && !name.isEmpty()) {
		const auto nameFont = st::historyPullNextNameFont;
		const auto centerWidth = width() - st::historyScroll.width;
		const auto avail = centerWidth - 4 * st::historyPullNextSkip;
		const auto elName = nameFont->elided(name, avail);
		const auto nameW = float64(nameFont->width(elName));
		const auto y = h
			+ (st::historyPullNextNameTop * (1. - release)
				- st::historyPullNextRise * release)
			+ bounceOffset;
		const auto pill = QRectF(
			(centerWidth - nameW) / 2. - st::historyPullNextSkip,
			y - st::historyPullNextPadding,
			nameW + 2 * st::historyPullNextSkip,
			nameFont->height + st::historyPullNextSkip);
		p.setOpacity(alpha * release);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(
			pill,
			st::historyPullNextNameRadius,
			st::historyPullNextNameRadius);
		p.setPen(fg);
		p.setFont(nameFont);
		p.drawText(
			QRectF((centerWidth - nameW) / 2., y, nameW, nameFont->height),
			elName,
			QTextOption(Qt::AlignCenter));
	}

	if (size >= 1.) {
		const auto top = h
			+ ((-st::historyPullNextSkip
					- st::historyPullNextSkip * progress
					- size) * (1. - release)
				+ (-offset + st::historyPullNextPadding) * release)
			+ bounceOffset;
		const auto avRect = QRectF(cx - size / 2., top, size, size);
		p.setOpacity(alpha);
		if (_next) {
			const auto count = _next->unreadCount();
			const auto badgeShown = (count > 0) && (release > 0.);
			const auto font = st::historyPullNextBadgeFont;
			const auto badgeHeight = float64(st::historyPullNextBadge);
			const auto string = badgeShown
				? ((count > 999) ? u"999+"_q : QString::number(count))
				: QString();
			const auto badgeWidth = badgeShown
				? std::max(
					font->width(string) + badgeHeight - font->height,
					badgeHeight)
				: 0.;
			const auto badge = badgeShown
				? QRectF(
					avRect.x() + avRect.width() - badgeWidth + badgeHeight / 3.,
					avRect.y() - badgeHeight / 4.,
					badgeWidth,
					badgeHeight)
				: QRectF();
			const auto ringWidth = float64(st::historyPullNextBadgeRing);
			const auto layerRect = badgeShown
				? avRect.united(badge + Margins(ringWidth))
				: avRect;

			const auto ratio = style::DevicePixelRatio();
			auto layer = QImage(
				(layerRect.size() * ratio).toSize(),
				QImage::Format_ARGB32_Premultiplied);
			layer.setDevicePixelRatio(ratio);
			layer.fill(Qt::transparent);
			{
				auto q = QPainter(&layer);
				q.setRenderHint(QPainter::Antialiasing);
				q.translate(-layerRect.topLeft());

				q.save();
				q.translate(rect::center(avRect));
				q.scale(size / avatar, size / avatar);
				q.translate(-avatar / 2., -avatar / 2.);
				_next->peer->paintUserpic(q, _userpic, 0, 0, int(avatar), true);
				q.restore();

				if (badgeShown) {
					q.translate(rect::center(badge));
					q.scale(release, release);
					q.translate(-rect::center(badge));
					q.setPen(Qt::NoPen);
					q.setBrush(st::dialogsUnreadBg->c);
					q.drawRoundedRect(
						badge,
						badgeHeight / 2.,
						badgeHeight / 2.);
					q.setPen(st::dialogsUnreadFg->c);
					q.setFont(font);
					q.drawText(badge, string, QTextOption(Qt::AlignCenter));

					auto pen = QPen(Qt::transparent);
					pen.setWidthF(ringWidth);
					const auto ring = badge + Margins(ringWidth / 2.);
					q.setCompositionMode(QPainter::CompositionMode_Source);
					q.setPen(pen);
					q.setBrush(Qt::NoBrush);
					q.drawRoundedRect(
						ring,
						ring.height() / 2.,
						ring.height() / 2.);
				}
			}
			p.setOpacity(alpha);
			p.drawImage(layerRect.topLeft(), layer);
		} else {
			p.setPen(Qt::NoPen);
			p.setBrush(bg);
			p.drawEllipse(avRect);
			PaintCheck(p, rect::center(avRect), size / 2., fg);
		}
	}
}

class PullToNextChannel::HintOverlay final : public Ui::RpWidget {
public:
	explicit HintOverlay(not_null<QWidget*> parent);

	void setData(bool visible, bool ready, History *next);
	void hideNow();

private:
	void paintEvent(QPaintEvent *e) override;

	bool _visible = false;
	bool _ready = false;
	bool _has = false;
	Ui::Animations::Simple _panel;
	Ui::Animations::Simple _releaseProgress;

};

PullToNextChannel::HintOverlay::HintOverlay(not_null<QWidget*> parent)
: RpWidget(parent) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	hide();
}

void PullToNextChannel::HintOverlay::setData(
		bool visible,
		bool ready,
		History *next) {
	_has = (next != nullptr);
	const auto want = visible && _has;
	if (_visible != want) {
		_visible = want;
		_panel.start([=] {
			update();
			if (!_panel.animating() && !_visible) {
				hide();
			}
		}, want ? 0. : 1., want ? 1. : 0., kPanelDuration, anim::easeOutQuint);
	}
	if (_ready != ready) {
		_ready = ready;
		_releaseProgress.start(
			[=] { update(); },
			ready ? 0. : 1.,
			ready ? 1. : 0.,
			ready ? kReleaseShowDuration : kReleaseHideDuration,
			anim::easeOutQuint);
	}
	if (want && isHidden()) {
		show();
	}
	update();
}

void PullToNextChannel::HintOverlay::hideNow() {
	_visible = false;
	_ready = false;
	_panel.stop();
	_releaseProgress.stop();
	hide();
}

void PullToNextChannel::HintOverlay::paintEvent(QPaintEvent *e) {
	const auto panel = _panel.value(_visible ? 1. : 0.);
	if (panel <= 0.) {
		return;
	}

	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);

	p.setOpacity(panel);
	p.fillRect(rect(), st::windowBg);

	const auto release = _releaseProgress.value(_ready ? 1. : 0.);
	const auto slide = st::historyPullNextHintSlide;
	const auto font = st::historyPullNextHintFont;
	const auto avail = width() - 4 * st::historyPullNextSkip;
	const auto top = (height() - font->height) / 2.;
	p.setFont(font);
	p.setPen(st::windowSubTextFg);
	if (release < 1.) {
		p.setOpacity(panel * (1. - release));
		const auto pull = font->elided(
			tr::lng_pull_next_channel(tr::now),
			avail);
		p.drawText(
			QRectF(0., top - slide * release, width(), font->height),
			pull,
			QTextOption(Qt::AlignCenter));
	}
	if (release > 0.) {
		p.setOpacity(panel * release);
		const auto rel = font->elided(
			tr::lng_release_next_channel(tr::now),
			avail);
		p.drawText(
			QRectF(0., top + slide * (1. - release), width(), font->height),
			rel,
			QTextOption(Qt::AlignCenter));
	}
}

PullToNextChannel::PullToNextChannel(
	not_null<Ui::RpWidget*> parent,
	not_null<Ui::ContinuousScroll*> scroll,
	not_null<Window::SessionController*> controller)
: _parent(parent)
, _scroll(scroll)
, _controller(controller)
, _indicator(base::make_unique_q<Indicator>(scroll, controller->chatStyle()))
, _hint(base::make_unique_q<HintOverlay>(parent)) {
}

PullToNextChannel::~PullToNextChannel() = default;

void PullToNextChannel::attachToContent(not_null<HistoryInner*> inner) {
	reset();
	_inner = inner.get();
	_filter = base::unique_qptr<QObject>(base::install_event_filter(
		inner,
		[=](not_null<QEvent*> e) {
			return (e->type() == QEvent::Wheel
					&& processWheel(static_cast<QWheelEvent*>(e.get())))
				? base::EventFilterResult::Cancel
				: base::EventFilterResult::Continue;
		}));
}

void PullToNextChannel::setHistory(History *history) {
	if (_history == history) {
		return;
	}
	_history = history;
	reset();
}

bool PullToNextChannel::active() const {
	return _history
		&& _history->peer->isBroadcast()
		&& atBottom()
		&& !_controller->session().sponsoredMessages().hasUnshownFor(_history);
}

bool PullToNextChannel::atBottom() const {
	return (_scroll->scrollTop() >= _scroll->scrollTopMax())
		&& _history->loadedAtBottom();
}

bool PullToNextChannel::processWheel(not_null<QWheelEvent*> e) {
	const auto phase = e->phase();
	if (phase == Qt::NoScrollPhase) {
		return false;
	} else if (phase == Qt::ScrollBegin) {
		reset();
		return false;
	} else if (phase == Qt::ScrollEnd || phase == Qt::ScrollMomentum) {
		return release() || _retract.animating() || _swallowMomentum;
	} else if (!_engaged
		&& (_gaveUp
			|| !_history
			|| !_history->peer->isBroadcast()
			|| !atBottom())) {
		return false;
	}
	const auto delta = Ui::ScrollDeltaF(e);
	return applyDelta(delta.x(), delta.y());
}

bool PullToNextChannel::applyDelta(float64 deltaX, float64 deltaY) {
	if (!_engaged) {
		_swipeX += deltaX;
		_swipeY += deltaY;
		const auto down = -_swipeY;
		const auto sideways = std::abs(_swipeX);
		if (sideways > kDirectionLock && sideways >= down) {
			_gaveUp = true;
			return false;
		} else if (down < -kDirectionLock) {
			_gaveUp = true;
			return false;
		} else if (!active()) {
			return false;
		} else if (down <= kDirectionLock || down <= sideways) {
			return true;
		}
		_engaged = true;
		_retract.stop();
		_accumulated = down;
		_next = FindNextUnreadChannel(_controller, _history->peer);
		if (_next && !_next->isReadyFor(ShowAtUnreadMsgId)) {
			[[maybe_unused]] const auto id = Support::SendPreloadRequest(
				_next,
				[] {});
		}
		if (_next) {
			PreloadPinnedBar(_next);
		}
	} else {
		_accumulated = std::max(0., _accumulated - deltaY);
	}
	const auto threshold = float64(st::historyPullNextThreshold);
	_offset = std::max(0., std::min(
		float64(st::historyPullNextMaxHeight),
		float64(Ui::OverscrollFromAccumulated(
			int(base::SafeRound(_accumulated))))));
	const auto ratio = threshold ? (_offset / threshold) : 0.;
	if (_next && !_reached && ratio >= 1.) {
		_reached = true;
		base::Platform::Haptic();
		startExpand(true);
	} else if (_reached && ratio < kResetReachedOn) {
		_reached = false;
		startExpand(false);
	}
	push(_offset, _reached, _offset > 0., _next);
	return true;
}

bool PullToNextChannel::release() {
	if (!_engaged) {
		return false;
	}
	const auto next = _next;
	const auto fromAccumulated = _accumulated;
	const auto ready = (_offset >= float64(st::historyPullNextThreshold))
		&& next
		&& next->unreadCount() > 0;
	_swallowMomentum = true;
	clearState();
	if (ready) {
		crl::on_main(_parent.get(), [=] { jumpWhenReady(next, 0); });
	} else {
		startRetract(fromAccumulated, next);
	}
	return true;
}

void PullToNextChannel::push(
		float64 offset,
		bool ready,
		bool visible,
		History *next) {
	_pushOffset = offset;
	_pushVisible = visible;
	_pushNext = next;
	render(ready);
}

void PullToNextChannel::render(bool ready) {
	const auto full = float64(st::historyPullNextExpand);
	const auto expand = _expand.value(ready ? 1. : 0.);
	const auto effective = _pushOffset + (full - _pushOffset) * expand;
	applyShift(int(base::SafeRound(effective)));
	_indicator->setData(effective, ready, _pushNext);
	_hint->setData(_pushVisible, ready, _pushNext);
}

void PullToNextChannel::startExpand(bool ready) {
	_expand.start(
		[=] { render(_reached); },
		ready ? 0. : 1.,
		ready ? 1. : 0.,
		kExpandDuration,
		anim::easeOutQuint);
}

void PullToNextChannel::applyShift(int shift) {
	if (_inner && _inner->pullBottomInset() != shift) {
		_inner->setPullBottomInset(shift);
		_scroll->scrollToY(_scroll->scrollTopMax());
		_inner->update();
	}
}

void PullToNextChannel::startRetract(float64 fromAccumulated, History *next) {
	if (fromAccumulated <= 0.) {
		push(0., false, false, nullptr);
		return;
	}
	_retract.start([=] {
		const auto progress = _retract.value(0.);
		if (_retract.animating()) {
			const auto offset = float64(Ui::OverscrollFromAccumulated(
				int(base::SafeRound(fromAccumulated * progress))));
			push(offset, false, true, next);
		} else {
			push(0., false, false, nullptr);
		}
	}, 1., 0., kRetractDuration, anim::sineInOut);
}

void PullToNextChannel::clearState() {
	_expand.stop();
	_accumulated = 0.;
	_offset = 0.;
	_swipeX = 0.;
	_swipeY = 0.;
	_engaged = false;
	_reached = false;
	_gaveUp = false;
	_next = nullptr;
}

void PullToNextChannel::reset() {
	_retract.stop();
	_swallowMomentum = false;
	clearState();
	applyShift(0);
	_indicator->hideNow();
	_hint->hideNow();
}

void PullToNextChannel::updateGeometry() {
	const auto height = st::historyPullNextMaxHeight;
	_indicator->setGeometry(
		0,
		_scroll->height() - height,
		_scroll->width(),
		height);
	_indicator->raise();

	const auto top = _scroll->y() + _scroll->height();
	const auto bottom = _parent->height();
	if (bottom > top) {
		_hint->setGeometry(_scroll->x(), top, _scroll->width(), bottom - top);
		_hint->raise();
	}
}

void PullToNextChannel::jumpWhenReady(
		not_null<History*> next,
		crl::time waited) {
	constexpr auto kInterval = crl::time(100);
	constexpr auto kMaxWait = crl::time(1500);
	constexpr auto kPinnedMaxWait = crl::time(600);
	const auto historyReady = next->isReadyFor(ShowAtUnreadMsgId);
	const auto pinnedReady = (waited >= kPinnedMaxWait) || PinnedBarReady(next);
	if ((historyReady && pinnedReady) || waited >= kMaxWait) {
		jumpTo(next);
		return;
	}
	base::call_delayed(kInterval, _parent.get(), [=] {
		jumpWhenReady(next, waited + kInterval);
	});
}

void PullToNextChannel::jumpTo(not_null<History*> history) {
	auto params = Window::SectionShow(Window::SectionShow::Way::ClearStack);
	params.slideFromBottom = true;
	_controller->showPeerHistory(history, params);
}

} // namespace HistoryView
