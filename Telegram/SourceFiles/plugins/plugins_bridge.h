/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <rpl/event_stream.h>
#include <rpl/lifetime.h>

#include <QtCore/QProcess>

class HistoryItem;
class QJsonObject;

namespace Main {
class Session;
} // namespace Main

namespace Plugins {

struct Plugin {
	QString fileName;
	QString name;
	QString version;
	QString description;
	QString author;
	bool enabled = true;
};

class Bridge final {
public:
	explicit Bridge(not_null<Main::Session*> session);
	~Bridge();

	[[nodiscard]] QString directory() const;
	[[nodiscard]] std::vector<Plugin> list() const;
	[[nodiscard]] std::optional<Plugin> find(const QString &fileName) const;

	bool install(const QString &sourcePath, QString *errorText = nullptr);
	void uninstall(const QString &fileName);
	void setEnabled(const QString &fileName, bool enabled);
	void requestReload();

	[[nodiscard]] rpl::producer<> changes() const;

	[[nodiscard]] static std::optional<Plugin> ReadMetadata(
		const QString &path);

private:
	void start();
	void ensureDirectory();
	void handleIncoming(not_null<HistoryItem*> item);
	void handleOutgoing(
		not_null<PeerData*> peer,
		const QString &text,
		MsgId replyToId,
		const QString &replyPath);
	void sendEvent(const QJsonObject &event);
	void readActions();
	void handleAction(const QJsonObject &action);
	[[nodiscard]] QString resolveOnDiskPath(const QString &fileName) const;

	const not_null<Main::Session*> _session;
	QProcess _process;
	rpl::event_stream<> _changes;
	rpl::lifetime _lifetime;

};

} // namespace Plugins
