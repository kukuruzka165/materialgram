/*
This file is part of materialGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/materialGramDesktop/materialGramDesktop/blob/dev/LEGAL
*/
#pragma once

namespace MaterialLang {
namespace Lang {

struct Var {
	Var() {};
	Var(const QString &k, const QString &v) {
		key = k;
		value = v;
	}

	QString key;
	QString value;
};

struct EntVar {
	EntVar() {};
	EntVar(const QString &k, TextWithEntities v) {
		key = k;
		value = v;
	}

	QString key;
	TextWithEntities value;
};

void Load(const QString &baseLangCode, const QString &langCode);

QString Translate(
	const QString &key,
	Var var1 = Var(),
	Var var2 = Var(),
	Var var3 = Var(),
	Var var4 = Var());
QString Translate(
	const QString &key,
	float64 value,
	Var var1 = Var(),
	Var var2 = Var(),
	Var var3 = Var(),
	Var var4 = Var());

TextWithEntities TranslateWithEntities(
	const QString &key,
	EntVar var1 = EntVar(),
	EntVar var2 = EntVar(),
	EntVar var3 = EntVar(),
	EntVar var4 = EntVar());
TextWithEntities TranslateWithEntities(
	const QString &key,
	float64 value,
	EntVar var1 = EntVar(),
	EntVar var2 = EntVar(),
	EntVar var3 = EntVar(),
	EntVar var4 = EntVar());

rpl::producer<> Events();

} // namespace Lang
} // namespace MaterialLang

// Shorthands

inline QString ktr(
	const QString &key,
	::MaterialLang::Lang::Var var1 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var2 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var3 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var4 = ::MaterialLang::Lang::Var()) {
	return ::MaterialLang::Lang::Translate(key, var1, var2, var3, var4);
}

inline QString ktr(
	const QString &key,
	float64 value,
	::MaterialLang::Lang::Var var1 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var2 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var3 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var4 = ::MaterialLang::Lang::Var()) {
	return ::MaterialLang::Lang::Translate(key, value, var1, var2, var3, var4);
}

inline TextWithEntities ktre(
	const QString &key,
	::MaterialLang::Lang::EntVar var1 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var2 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var3 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var4 = ::MaterialLang::Lang::EntVar()) {
	return ::MaterialLang::Lang::TranslateWithEntities(key, var1, var2, var3, var4);
}

inline TextWithEntities ktre(
	const QString &key,
	float64 value,
	::MaterialLang::Lang::EntVar var1 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var2 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var3 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var4 = ::MaterialLang::Lang::EntVar()) {
	return ::MaterialLang::Lang::TranslateWithEntities(key, value, var1, var2, var3, var4);
}

inline rpl::producer<QString> rktr(
	const QString &key,
	::MaterialLang::Lang::Var var1 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var2 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var3 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var4 = ::MaterialLang::Lang::Var()) {
	return rpl::single(
			::MaterialLang::Lang::Translate(key, var1, var2, var3, var4)
		) | rpl::then(
			::MaterialLang::Lang::Events() | rpl::map(
				[=]{ return ::MaterialLang::Lang::Translate(key, var1, var2, var3, var4); })
		);
}

inline rpl::producer<QString> rktr(
	const QString &key,
	float64 value,
	::MaterialLang::Lang::Var var1 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var2 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var3 = ::MaterialLang::Lang::Var(),
	::MaterialLang::Lang::Var var4 = ::MaterialLang::Lang::Var()) {
	return rpl::single(
			::MaterialLang::Lang::Translate(key, value, var1, var2, var3, var4)
		) | rpl::then(
			::MaterialLang::Lang::Events() | rpl::map(
				[=]{ return ::MaterialLang::Lang::Translate(key, value, var1, var2, var3, var4); })
		);
}

inline rpl::producer<TextWithEntities> rktre(
	const QString &key,
	::MaterialLang::Lang::EntVar var1 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var2 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var3 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var4 = ::MaterialLang::Lang::EntVar()) {
	return rpl::single(
			::MaterialLang::Lang::TranslateWithEntities(key, var1, var2, var3, var4)
		) | rpl::then(
			::MaterialLang::Lang::Events() | rpl::map(
				[=]{ return ::MaterialLang::Lang::TranslateWithEntities(key, var1, var2, var3, var4); })
		);
}

inline rpl::producer<TextWithEntities> rktre(
	const QString &key,
	float64 value,
	::MaterialLang::Lang::EntVar var1 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var2 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var3 = ::MaterialLang::Lang::EntVar(),
	::MaterialLang::Lang::EntVar var4 = ::MaterialLang::Lang::EntVar()) {
	return rpl::single(
			::MaterialLang::Lang::TranslateWithEntities(key, value, var1, var2, var3, var4)
		) | rpl::then(
			::MaterialLang::Lang::Events() | rpl::map(
				[=]{ return ::MaterialLang::Lang::TranslateWithEntities(key, value, var1, var2, var3, var4); })
		);
}