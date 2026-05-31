/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugins_bridge.h"

#include "main/main_session.h"
#include "apiwrap.h"
#include "api/api_common.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "settings.h"
#include "logs.h"

#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Plugins {
namespace {

[[nodiscard]] QString PythonExecutable() {
#ifdef Q_OS_WIN
	return u"python"_q;
#else // Q_OS_WIN
	return u"python3"_q;
#endif // Q_OS_WIN
}

[[nodiscard]] QString PluginsRoot() {
	return cWorkingDir() + u"plugins"_q;
}

} // namespace

Bridge::Bridge(not_null<Main::Session*> session) : _session(session) {
	start();
}

Bridge::~Bridge() {
	if (_process.state() == QProcess::NotRunning) {
		return;
	}
	sendEvent({ { u"event"_q, u"shutdown"_q } });
	_process.closeWriteChannel();
	if (!_process.waitForFinished(1000)) {
		_process.kill();
		_process.waitForFinished(1000);
	}
}

void Bridge::start() {
	const auto root = PluginsRoot();
	if (!QDir(root).exists()) {
		LOG(("Plugins: directory not found, bridge disabled (%1).").arg(root));
		return;
	}

	_process.setProgram(PythonExecutable());
	_process.setArguments({
		u"-m"_q,
		u"opengram_plugins.sidecar"_q,
		root + u"/examples"_q,
	});
	_process.setWorkingDirectory(root);

	QObject::connect(&_process, &QProcess::readyReadStandardOutput, [=] {
		readActions();
	});
	QObject::connect(
		&_process,
		&QProcess::errorOccurred,
		[=](QProcess::ProcessError error) {
			LOG(("Plugins: process error %1.").arg(int(error)));
		});

	_process.start();

	_session->data().newItemAdded(
	) | rpl::on_next([=](not_null<HistoryItem*> item) {
		handleIncoming(item);
	}, _lifetime);
}

void Bridge::handleIncoming(not_null<HistoryItem*> item) {
	if (item->out()) {
		return;
	}
	const auto text = item->originalText().text;
	if (text.isEmpty()) {
		return;
	}
	const auto peer = item->history()->peer;
	sendEvent({
		{ u"event"_q, u"message"_q },
		{ u"chat"_q, QString::number(peer->id.value) },
		{ u"sender"_q, QString::number(item->from()->id.value) },
		{ u"text"_q, text },
	});
}

void Bridge::sendEvent(const QJsonObject &event) {
	if (_process.state() != QProcess::Running) {
		return;
	}
	_process.write(QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n');
}

void Bridge::readActions() {
	while (_process.canReadLine()) {
		const auto line = _process.readLine();
		const auto parsed = QJsonDocument::fromJson(line);
		if (parsed.isObject()) {
			handleAction(parsed.object());
		}
	}
}

void Bridge::handleAction(const QJsonObject &action) {
	const auto type = action.value(u"action"_q).toString();
	if (type == u"send_message"_q) {
		const auto raw = action.value(u"chat"_q).toString().toULongLong();
		const auto text = action.value(u"text"_q).toString();
		if (!raw || text.isEmpty()) {
			return;
		}
		const auto history = _session->data().history(PeerId(BareId(raw)));
		auto message = Api::MessageToSend(Api::SendAction(history));
		message.textWithTags = { text, {} };
		_session->api().sendMessage(std::move(message));
	} else if (type == u"log"_q) {
		LOG(("Plugins: %1").arg(action.value(u"text"_q).toString()));
	}
}

} // namespace Plugins
