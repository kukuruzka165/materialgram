/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Plugins {

void InstallBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	QString filePath);

void ManagerBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

void ShowInstallBox(
	not_null<Window::SessionController*> controller,
	const QString &filePath);

void ShowManagerBox(not_null<Window::SessionController*> controller);

} // namespace Plugins
