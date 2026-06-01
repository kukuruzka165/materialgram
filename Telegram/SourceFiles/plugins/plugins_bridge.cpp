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
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

namespace Plugins {
namespace {

constexpr auto kReadLimit = 256 * 1024;

const auto kDisabledSuffix = u".off"_q;
const auto kPluginSuffix = u".plg"_q;

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

[[nodiscard]] QString CanonicalName(const QString &fileName) {
	return fileName.endsWith(kDisabledSuffix, Qt::CaseInsensitive)
		? fileName.left(fileName.size() - kDisabledSuffix.size())
		: fileName;
}

[[nodiscard]] QString CapturedValue(
		const QString &source,
		const QString &field) {
	const auto expression = QRegularExpression(
		u"^\\s*"_q + field + u"\\s*=\\s*[\"']([^\"']*)[\"']"_q,
		QRegularExpression::MultilineOption);
	const auto match = expression.match(source);
	return match.hasMatch() ? match.captured(1).trimmed() : QString();
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

QString Bridge::directory() const {
	return PluginsRoot() + u"/installed"_q;
}

void Bridge::ensureDirectory() {
	QDir().mkpath(directory());
}

void Bridge::start() {
	const auto root = PluginsRoot();
	if (!QDir(root).exists()) {
		LOG(("Plugins: directory not found, bridge disabled (%1).").arg(root));
		return;
	}
	ensureDirectory();

	_process.setProgram(PythonExecutable());
	_process.setArguments({
		u"-m"_q,
		u"opengram_plugins.sidecar"_q,
		directory(),
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

std::optional<Plugin> Bridge::ReadMetadata(const QString &path) {
	const auto info = QFileInfo(path);
	const auto canonical = CanonicalName(info.fileName());
	if (!canonical.endsWith(kPluginSuffix, Qt::CaseInsensitive)) {
		return std::nullopt;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return std::nullopt;
	}
	const auto source = QString::fromUtf8(file.read(kReadLimit));
	if (!source.contains(u"Plugin"_q)) {
		return std::nullopt;
	}
	auto result = Plugin();
	result.fileName = canonical;
	result.name = CapturedValue(source, u"name"_q);
	if (result.name.isEmpty()) {
		result.name = canonical.chopped(kPluginSuffix.size());
	}
	result.version = CapturedValue(source, u"version"_q);
	result.description = CapturedValue(source, u"description"_q);
	return result;
}

std::vector<Plugin> Bridge::list() const {
	auto result = std::vector<Plugin>();
	const auto dir = QDir(directory());
	if (!dir.exists()) {
		return result;
	}
	const auto entries = dir.entryList(
		{ u"*"_q + kPluginSuffix, u"*"_q + kPluginSuffix + kDisabledSuffix },
		QDir::Files,
		QDir::Name);
	for (const auto &entry : entries) {
		auto parsed = ReadMetadata(dir.filePath(entry));
		if (!parsed) {
			continue;
		}
		parsed->enabled = !entry.endsWith(
			kDisabledSuffix,
			Qt::CaseInsensitive);
		result.push_back(std::move(*parsed));
	}
	return result;
}

std::optional<Plugin> Bridge::find(const QString &fileName) const {
	const auto canonical = CanonicalName(fileName);
	for (auto &plugin : list()) {
		if (plugin.fileName == canonical) {
			return plugin;
		}
	}
	return std::nullopt;
}

QString Bridge::resolveOnDiskPath(const QString &fileName) const {
	const auto canonical = CanonicalName(fileName);
	const auto dir = QDir(directory());
	const auto enabled = dir.filePath(canonical);
	if (QFile::exists(enabled)) {
		return enabled;
	}
	const auto disabled = enabled + kDisabledSuffix;
	if (QFile::exists(disabled)) {
		return disabled;
	}
	return QString();
}

bool Bridge::install(const QString &sourcePath, QString *errorText) {
	const auto metadata = ReadMetadata(sourcePath);
	if (!metadata) {
		if (errorText) {
			*errorText = u"This is not a valid opengram plugin."_q;
		}
		return false;
	}
	ensureDirectory();

	const auto existing = resolveOnDiskPath(metadata->fileName);
	if (!existing.isEmpty()) {
		QFile::remove(existing);
	}

	const auto target = QDir(directory()).filePath(metadata->fileName);
	if (!QFile::copy(sourcePath, target)) {
		if (errorText) {
			*errorText = u"Could not copy the plugin file."_q;
		}
		return false;
	}

	_changes.fire({});
	requestReload();
	return true;
}

void Bridge::uninstall(const QString &fileName) {
	const auto path = resolveOnDiskPath(fileName);
	if (path.isEmpty() || !QFile::remove(path)) {
		return;
	}
	_changes.fire({});
	requestReload();
}

void Bridge::setEnabled(const QString &fileName, bool enabled) {
	const auto path = resolveOnDiskPath(fileName);
	if (path.isEmpty()) {
		return;
	}
	const auto canonical = QDir(directory()).filePath(
		CanonicalName(fileName));
	const auto target = enabled ? canonical : (canonical + kDisabledSuffix);
	if (path == target) {
		return;
	}
	if (!QFile::rename(path, target)) {
		return;
	}
	_changes.fire({});
	requestReload();
}

void Bridge::requestReload() {
	if (_process.state() == QProcess::Running) {
		sendEvent({ { u"event"_q, u"reload"_q } });
	} else if (_process.state() == QProcess::NotRunning) {
		start();
	}
}

rpl::producer<> Bridge::changes() const {
	return _changes.events();
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
