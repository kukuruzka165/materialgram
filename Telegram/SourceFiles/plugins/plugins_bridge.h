/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <rpl/lifetime.h>

#include <QtCore/QProcess>

class HistoryItem;
class QJsonObject;

namespace Main {
class Session;
} // namespace Main

namespace Plugins {

class Bridge final {
public:
	explicit Bridge(not_null<Main::Session*> session);
	~Bridge();

private:
	void start();
	void handleIncoming(not_null<HistoryItem*> item);
	void sendEvent(const QJsonObject &event);
	void readActions();
	void handleAction(const QJsonObject &action);

	const not_null<Main::Session*> _session;
	QProcess _process;
	rpl::lifetime _lifetime;

};

} // namespace Plugins
