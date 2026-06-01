/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugins_box.h"

#include "plugins/plugins_bridge.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Plugins {
namespace {

[[nodiscard]] QString PluginFilter() {
	return u"opengram plugins (*.plg);;"_q + FileDialog::AllFilesFilter();
}

[[nodiscard]] QString VersionLabel(const Plugin &plugin) {
	auto result = plugin.version.isEmpty()
		? QString()
		: tr::lng_plugins_version(tr::now, lt_version, plugin.version);
	if (!plugin.enabled) {
		const auto off = tr::lng_plugins_state_off(tr::now);
		result = result.isEmpty()
			? off
			: (result + u" · "_q + off);
	}
	return result;
}

void DetailsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString fileName) {
	const auto bridge = &controller->session().plugins();
	const auto plugin = bridge->find(fileName);
	if (!plugin) {
		box->closeBox();
		return;
	}

	box->setTitle(rpl::single(plugin->name));
	box->setWidth(st::boxWideWidth);

	const auto inner = box->verticalLayout();
	if (!plugin->version.isEmpty()) {
		Ui::AddSkip(inner);
		const auto version = box->addRow(
			object_ptr<Ui::FlatLabel>(box, st::boxLabel));
		version->setText(
			tr::lng_plugins_version(tr::now, lt_version, plugin->version));
		version->setTextColorOverride(st::windowSubTextFg->c);
	}
	if (!plugin->description.isEmpty()) {
		Ui::AddSkip(inner);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(plugin->description),
			st::boxLabel));
	}

	Ui::AddSkip(inner);
	const auto toggle = Settings::AddButtonWithIcon(
		inner,
		tr::lng_plugins_enabled(),
		st::settingsButton,
		{ &st::menuIconSchedule });
	toggle->toggleOn(rpl::single(plugin->enabled));
	toggle->toggledValue(
	) | rpl::filter([=](bool checked) {
		const auto now = bridge->find(fileName);
		return now && (now->enabled != checked);
	}) | rpl::on_next([=](bool checked) {
		bridge->setEnabled(fileName, checked);
	}, toggle->lifetime());

	Ui::AddSkip(inner);
	Ui::AddDividerText(inner, tr::lng_plugins_install_about());

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(tr::lng_plugins_remove(), [=] {
		const auto weak = base::make_weak(box.get());
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_plugins_remove_sure(
				tr::now,
				lt_name,
				plugin->name),
			.confirmed = [=](Fn<void()> close) {
				bridge->uninstall(fileName);
				controller->showToast(tr::lng_plugins_removed(tr::now));
				close();
				if (const auto strong = weak.get()) {
					strong->closeBox();
				}
			},
			.confirmText = tr::lng_plugins_remove(tr::now),
			.confirmStyle = &st::attentionBoxButton,
		}));
	}, st::attentionBoxButton);
}

} // namespace

void InstallBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString filePath) {
	box->setTitle(tr::lng_plugins_install_title());
	box->setWidth(st::boxWideWidth);

	const auto metadata = Bridge::ReadMetadata(filePath);
	if (!metadata) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_plugins_invalid(),
			st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}

	const auto inner = box->verticalLayout();
	Ui::AddSkip(inner);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::single(Ui::Text::Bold(metadata->name)),
		st::boxLabel));
	if (!metadata->version.isEmpty()) {
		const auto version = box->addRow(
			object_ptr<Ui::FlatLabel>(box, st::boxLabel));
		version->setText(
			tr::lng_plugins_version(tr::now, lt_version, metadata->version));
		version->setTextColorOverride(st::windowSubTextFg->c);
	}
	if (!metadata->description.isEmpty()) {
		Ui::AddSkip(inner);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(metadata->description),
			st::boxLabel));
	}

	Ui::AddSkip(inner);
	Ui::AddDividerText(inner, tr::lng_plugins_install_about());

	const auto already = controller->session().plugins().find(
		metadata->fileName).has_value();

	box->addButton(
		already
			? tr::lng_plugins_reinstall()
			: tr::lng_plugins_install_button(),
		[=] {
			auto error = QString();
			if (controller->session().plugins().install(filePath, &error)) {
				controller->showToast(tr::lng_plugins_installed(tr::now));
			} else if (!error.isEmpty()) {
				controller->showToast(error);
			}
			box->closeBox();
		});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ManagerBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto bridge = &controller->session().plugins();

	box->setTitle(tr::lng_plugins_title());
	box->setWidth(st::boxWideWidth);

	const auto inner = box->verticalLayout();
	Ui::AddSkip(inner);
	const auto list = inner->add(object_ptr<Ui::VerticalLayout>(inner));

	const auto rebuild = [=] {
		while (list->count() > 0) {
			delete list->widgetAt(0);
		}
		auto items = bridge->list();
		if (items.empty()) {
			const auto empty = list->add(
				object_ptr<Ui::FlatLabel>(
					list,
					tr::lng_plugins_none(),
					st::boxLabel),
				st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0));
			empty->setTextColorOverride(st::windowSubTextFg->c);
		} else {
			for (const auto &plugin : items) {
				const auto fileName = plugin.fileName;
				const auto button = Settings::AddButtonWithLabel(
					list,
					rpl::single(plugin.name),
					rpl::single(VersionLabel(plugin)),
					st::settingsButton,
					{ &st::menuIconStickers });
				button->setClickedCallback([=] {
					controller->show(
						Box(DetailsBox, controller, fileName));
				});
			}
		}
	};

	rebuild();
	bridge->changes(
	) | rpl::on_next([=] {
		rebuild();
	}, box->lifetime());

	Ui::AddSkip(inner);
	Ui::AddDivider(inner);
	Ui::AddSkip(inner);

	Settings::AddButtonWithIcon(
		inner,
		tr::lng_plugins_add(),
		st::settingsButton,
		{ &st::menuIconShowInFolder }
	)->setClickedCallback([=] {
		const auto weak = base::make_weak(controller);
		FileDialog::GetOpenPath(
			box.get(),
			tr::lng_plugins_add(tr::now),
			PluginFilter(),
			[=](FileDialog::OpenResult &&result) {
				if (result.paths.isEmpty()) {
					return;
				}
				const auto strong = weak.get();
				if (!strong) {
					return;
				}
				strong->show(
					Box(InstallBox, strong, result.paths.front()));
			});
	});

	Settings::AddButtonWithIcon(
		inner,
		tr::lng_plugins_reload(),
		st::settingsButton,
		{ &st::menuIconRestartBot }
	)->setClickedCallback([=] {
		bridge->requestReload();
		controller->showToast(tr::lng_plugins_reloaded(tr::now));
	});

	Ui::AddSkip(inner);

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void ShowInstallBox(
		not_null<Window::SessionController*> controller,
		const QString &filePath) {
	controller->show(Box(InstallBox, controller, filePath));
}

void ShowManagerBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(ManagerBox, controller));
}

} // namespace Plugins
