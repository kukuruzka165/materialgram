// Reference only — NOT wired into the opengram build yet.
// Shows how the C++ (Qt) side drives the Python plugin sidecar over NDJSON.
// Protocol: see opengram_plugins/sidecar.py.

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

class PluginBridge {
public:
	PluginBridge(const QString &pluginsRoot, const QString &pluginsDir) {
		_process.setProgram("python");
		_process.setArguments({ "-m", "opengram_plugins.sidecar", pluginsDir });
		_process.setWorkingDirectory(pluginsRoot);

		QObject::connect(&_process, &QProcess::readyReadStandardOutput, [=] {
			while (_process.canReadLine()) {
				const auto line = _process.readLine();
				const auto action = QJsonDocument::fromJson(line).object();
				handleAction(action);
			}
		});

		_process.start();
	}

	void sendMessage(const QString &chat, const QString &sender, const QString &text) {
		send(QJsonObject{
			{ "event", "message" },
			{ "chat", chat },
			{ "sender", sender },
			{ "text", text },
		});
	}

	void shutdown() {
		send(QJsonObject{ { "event", "shutdown" } });
		_process.waitForFinished();
	}

private:
	void send(const QJsonObject &event) {
		_process.write(QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n');
	}

	void handleAction(const QJsonObject &action) {
		const auto type = action["action"].toString();
		if (type == "send_message") {
			// TODO: call the real opengram API here, e.g.
			// SendMessageToChat(action["chat"].toString(), action["text"].toString());
		} else if (type == "log") {
			// TODO: route to opengram logs, e.g. LOG(("[plugin] %1").arg(action["text"].toString()));
		}
	}

	QProcess _process;

};
