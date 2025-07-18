/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_values.h"

#include "api/api_chat_participants.h"
#include "apiwrap.h"
#include "info/profile/info_profile_phone_menu.h"
#include "info/profile/info_profile_badge.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "countries/countries_instance.h"
#include "main/main_session.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/text/format_values.h" // Ui::FormatPhone
#include "ui/text/text_utilities.h"
#include "lang/lang_keys.h"
#include "data/notify/data_notify_settings.h"
#include "data/data_peer_values.h"
#include "data/data_saved_messages.h"
#include "data/data_saved_sublist.h"
#include "data/data_shared_media.h"
#include "data/data_message_reactions.h"
#include "data/data_folder.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_user.h"
#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/data_premium_limits.h"
#include "boxes/peers/edit_peer_permissions_box.h"
#include "base/unixtime.h"

namespace Info {
namespace Profile {
namespace {

using UpdateFlag = Data::PeerUpdate::Flag;

auto PlainAboutValue(not_null<PeerData*> peer) {
	return peer->session().changes().peerFlagsValue(
		peer,
		UpdateFlag::About
	) | rpl::map([=] {
		return peer->about();
	});
}

auto PlainUsernameValue(not_null<PeerData*> peer) {
	return rpl::merge(
		peer->session().changes().peerFlagsValue(peer, UpdateFlag::Username),
		peer->session().changes().peerFlagsValue(peer, UpdateFlag::Usernames)
	) | rpl::map([=] {
		return peer->username();
	});
}

auto PlainPrimaryUsernameValue(not_null<PeerData*> peer) {
	return UsernamesValue(
		peer
	) | rpl::map([=](std::vector<TextWithEntities> usernames) {
		if (!usernames.empty()) {
			return rpl::single(usernames.front().text) | rpl::type_erased();
		} else {
			return PlainUsernameValue(peer) | rpl::type_erased();
		}
	}) | rpl::flatten_latest();
}

void StripExternalLinks(TextWithEntities &text) {
	const auto local = [](const QString &url) {
		return !UrlRequiresConfirmation(QUrl::fromUserInput(url));
	};
	const auto notLocal = [&](const EntityInText &entity) {
		if (entity.type() == EntityType::CustomUrl) {
			return !local(entity.data());
		} else if (entity.type() == EntityType::Url) {
			return !local(text.text.mid(entity.offset(), entity.length()));
		} else {
			return false;
		}
	};
	text.entities.erase(
		ranges::remove_if(text.entities, notLocal),
		text.entities.end());
}

} // namespace

QString parseRegistrationTime(QString prefix, long long regTime) {
	return prefix + base::unixtime::parse(regTime)
		.toString(QLocale::system().dateFormat(QLocale::ShortFormat));
}

QString findRegistrationTime(long long userId) {
	struct UserData {
		long long id;
		long long registrationTime;
	};
	std::vector<UserData> userData = {
		{1000000, 1380326400}, // 2013
		{2768409, 1383264000},
		{7679610, 1388448000},
		{11538514, 1391212000}, // 2014
		{15835244, 1392940000},
		{23646077, 1393459000},
		{38015510, 1393632000},
		{44634663, 1399334000},
		{46145305, 1400198000},
		{54845238, 1411257000},
		{63263518, 1414454000},
		{101260938, 1425600000}, // 2015
		{101323197, 1426204000},
		{103151531, 1433376000},
		{103258382, 1432771000},
		{109393468, 1439078000},
		{111220210, 1429574000},
		{112594714, 1439683000},
		{116812045, 1437696000},
		{122600695, 1437782000},
		{124872445, 1439856000},
		{125828524, 1444003000},
		{130029930, 1441324000},
		{133909606, 1444176000},
		{143445125, 1448928000},
		{148670295, 1452211000}, // 2016
		{152079341, 1453420000},
		{157242073, 1446768000},
		{171295414, 1457481000},
		{181783990, 1460246000},
		{222021233, 1465344000},
		{225034354, 1466208000},
		{278941742, 1473465000},
		{285253072, 1476835000},
		{294851037, 1479600000},
		{297621225, 1481846000},
		{328594461, 1482969000},
		{337808429, 1487707000}, // 2017
		{341546272, 1487782000},
		{352940995, 1487894000},
		{369669043, 1490918000},
		{400169472, 1501459000},
		{616816630, 1529625600}, // 2018
		{681896077, 1532821500},
		{727572658, 1543708800},
		{796147074, 1541371800},
		{925078064, 1563290000}, // 2019
		{928636984, 1581513420}, // 2020
		{1054883348, 1585674420},
		{1057704545, 1580393640},
		{1145856008, 1586342040},
		{1227964864, 1596127860},
		{1382531194, 1600188120},
		{1658586909, 1613148540}, // 2021
		{1660971491, 1613329440},
		{1692464211, 1615402500},
		{1719536397, 1619293500},
		{1721844091, 1620224820},
		{1772991138, 1617540360},
		{1807942741, 1625520300},
		{1893429550, 1622040000},
		{1972424006, 1631669400},
		{1974255900, 1634000000},
		{2030606431, 1631992680},
		{2041327411, 1631989620},
		{2078711279, 1634321820},
		{2104178931, 1638353220},
		{2120496865, 1636714020},
		{2123596685, 1636503180},
		{2138472342, 1637590800},
		{3318845111, 1618028800},
		{4317845111, 1620028800},
		{5162494923, 1652449800}, // 2022
		{5186883095, 1648764360},
		{5304951856, 1656718440},
		{5317829834, 1653152820},
		{5318092331, 1652024220},
		{5336336790, 1646368100},
		{5362593868, 1652024520},
		{5387234031, 1662137700},
		{5396587273, 1648014800},
		{5409444610, 1659025020},
		{5416026704, 1660925460},
		{5465223076, 1661710860},
		{5480654757, 1660926300},
		{5499934702, 1662130740},
		{5513192189, 1659626400},
		{5522237606, 1654167240},
		{5537251684, 1664269800},
		{5559167331, 1656718560},
		{5568348673, 1654642200},
		{5591759222, 1659025500},
		{5608562550, 1664012820},
		{5614111200, 1661780160},
		{5666819340, 1664112240},
		{5684254605, 1662134040},
		{5684689868, 1661304720},
		{5707112959, 1663803300},
		{5756095415, 1660925940},
		{5772670706, 1661539140},
		{5778063231, 1667477640},
		{5802242180, 1671821040},
		{5853442730, 1674866100}, // 2023
		{5859878513, 1673117760},
		{5885964106, 1671081840},
		{5982648124, 1686941700},
		{6020888206, 1675534800},
		{6032606998, 1686998640},
		{6057123350, 1676198350},
		{6058560984, 1686907980},
		{6101607245, 1686830760},
		{6108011341, 1681032060},
		{6132325730, 1692033840},
		{6182056052, 1687870740},
		{6279839148, 1688399160},
		{6306077724, 1692442920},
		{6321562426, 1688486760},
		{6364973680, 1696349340},
		{6386727079, 1691696880},
		{6429580803, 1692082680},
		{6527226055, 1690289160},
		{6813121418, 1698489600},
		{6865576492, 1699052400},
		{6925870357, 1701192327},
		{7000000000, 1711889200} // 2024
	};
	std::sort(userData.begin(), userData.end(), [](const UserData& a, const UserData& b) {
		return a.id < b.id;
		});
	for (size_t i = 1; i < userData.size(); ++i) {
		if (userId >= userData[i - 1].id && userId <= userData[i].id) {
			double t = static_cast<double>(userId - userData[i - 1].id) / (userData[i].id - userData[i - 1].id);

			return parseRegistrationTime("~ ", userData[i - 1].registrationTime + t *
				(userData[i].registrationTime - userData[i - 1].registrationTime));
		}
	}
	if (userId <= 1000000) {
		return parseRegistrationTime("< ", 1380326400);
	}
	else {
		return parseRegistrationTime("> ", 1711889200);
	}
}

rpl::producer<TextWithEntities> RegistrationValue(not_null<PeerData*> peer) {
	auto userId = peer->id.to<UserId>().bare;
	return rpl::single(findRegistrationTime(userId)) | Ui::Text::ToWithEntities();
}

rpl::producer<TextWithEntities> DataCenterValue(not_null<PeerData*> peer) {
	auto dcId = std::get<StorageFileLocation>(peer->userpicLocation().file().data).dcId();
	auto dcName = (dcId == 1) ? "1, Miami" :
		(dcId == 2) ? "2, Amsterdam" :
		(dcId == 3) ? "3, Miami" :
		(dcId == 4) ? "4, Amsterdam" :
		(dcId == 5) ? "5, Singapore" :
		"Unknown";
	return rpl::single(dcName) | Ui::Text::ToWithEntities();
}

rpl::producer<QString> NameValue(not_null<PeerData*> peer) {
	if (const auto broadcast = peer->monoforumBroadcast()) {
		return NameValue(broadcast);
	}
	return peer->session().changes().peerFlagsValue(
		peer,
		UpdateFlag::Name
	) | rpl::map([=] { return peer->name(); });
}

rpl::producer<QString> TitleValue(not_null<Data::ForumTopic*> topic) {
	return topic->session().changes().topicFlagsValue(
		topic,
		Data::TopicUpdate::Flag::Title
	) | rpl::map([=] { return topic->title(); });
}

rpl::producer<DocumentId> IconIdValue(not_null<Data::ForumTopic*> topic) {
	return topic->session().changes().topicFlagsValue(
		topic,
		Data::TopicUpdate::Flag::IconId
	) | rpl::map([=] { return topic->iconId(); });
}

rpl::producer<int32> ColorIdValue(not_null<Data::ForumTopic*> topic) {
	return topic->session().changes().topicFlagsValue(
		topic,
		Data::TopicUpdate::Flag::ColorId
	) | rpl::map([=] { return topic->colorId(); });
}

rpl::producer<TextWithEntities> PhoneValue(not_null<UserData*> user) {
	return rpl::merge(
		Countries::Instance().updated(),
		user->session().changes().peerFlagsValue(
			user,
			UpdateFlag::PhoneNumber) | rpl::to_empty
	) | rpl::map([=] {
		return Ui::FormatPhone(user->phone());
	}) | Ui::Text::ToWithEntities();
}

rpl::producer<TextWithEntities> PhoneOrHiddenValue(not_null<UserData*> user) {
	return rpl::combine(
		PhoneValue(user),
		PlainUsernameValue(user),
		PlainAboutValue(user),
		tr::lng_info_mobile_hidden()
	) | rpl::map([user](
			const TextWithEntities &phone,
			const QString &username,
			const QString &about,
			const QString &hidden) {
		if (phone.text.isEmpty() && username.isEmpty() && about.isEmpty()) {
			return Ui::Text::WithEntities(hidden);
		} else if (IsCollectiblePhone(user)) {
			return Ui::Text::Link(phone, u"internal:collectible_phone/"_q
				+ user->phone() + '@' + QString::number(user->id.value));
		} else {
			return phone;
		}
	});
}

rpl::producer<TextWithEntities> UsernameValue(
		not_null<PeerData*> peer,
		bool primary) {
	return (primary
		? PlainPrimaryUsernameValue(peer)
		: (PlainUsernameValue(peer) | rpl::type_erased())
	) | rpl::map([](QString &&username) {
		return username.isEmpty()
			? QString()
			: ('@' + username);
	}) | Ui::Text::ToWithEntities();
}

QString UsernameUrl(
		not_null<PeerData*> peer,
		const QString &username,
		bool link) {
	const auto type = !peer->isUsernameEditable(username)
		? u"collectible_username"_q
		: link
		? u"username_link"_q
		: u"username_regular"_q;
	return u"internal:"_q
		+ type
		+ u"/"_q
		+ username
		+ "@"
		+ QString::number(peer->id.value);
}

rpl::producer<std::vector<TextWithEntities>> UsernamesValue(
		not_null<PeerData*> peer) {
	const auto map = [=](const std::vector<QString> &usernames) {
		return ranges::views::all(
			usernames
		) | ranges::views::transform([&](const QString &u) {
			return Ui::Text::Link(u, UsernameUrl(peer, u));
		}) | ranges::to_vector;
	};
	auto value = rpl::merge(
		peer->session().changes().peerFlagsValue(peer, UpdateFlag::Username),
		peer->session().changes().peerFlagsValue(peer, UpdateFlag::Usernames)
	);
	if (const auto user = peer->asUser()) {
		return std::move(value) | rpl::map([=] {
			return map(user->usernames());
		});
	} else if (const auto channel = peer->asChannel()) {
		return std::move(value) | rpl::map([=] {
			return map(channel->usernames());
		});
	} else {
		return rpl::single(std::vector<TextWithEntities>());
	}
}

TextWithEntities AboutWithEntities(
		not_null<PeerData*> peer,
		const QString &value) {
	auto flags = TextParseLinks | TextParseMentions;
	const auto user = peer->asUser();
	const auto isBot = user && user->isBot();
	const auto isPremium = user && user->isPremium();
	if (!user) {
		flags |= TextParseHashtags;
	} else if (isBot) {
		flags |= TextParseHashtags | TextParseBotCommands;
	}
	const auto stripExternal = peer->isChat()
		|| peer->isMegagroup()
		|| (user && !isBot && !isPremium);
	auto result = TextWithEntities{ value };
	TextUtilities::ParseEntities(result, flags);
	if (stripExternal) {
		StripExternalLinks(result);
	}
	return result;
}

rpl::producer<TextWithEntities> AboutValue(not_null<PeerData*> peer) {
	return PlainAboutValue(
		peer
	) | rpl::map([peer](const QString &value) {
		return AboutWithEntities(peer, value);
	});
}

rpl::producer<LinkWithUrl> LinkValue(not_null<PeerData*> peer, bool primary) {
	return (primary
		? PlainPrimaryUsernameValue(peer)
		: PlainUsernameValue(peer) | rpl::type_erased()
	) | rpl::map([=](QString &&username) {
		return LinkWithUrl{
			.text = (username.isEmpty()
				? QString()
				: peer->session().createInternalLinkFull(username)),
			.url = (username.isEmpty()
				? QString()
				: UsernameUrl(peer, username, true)),
		};
	});
}

rpl::producer<const ChannelLocation*> LocationValue(
		not_null<ChannelData*> channel) {
	return channel->session().changes().peerFlagsValue(
		channel,
		UpdateFlag::ChannelLocation
	) | rpl::map([=] {
		return channel->getLocation();
	});
}

rpl::producer<bool> NotificationsEnabledValue(
		not_null<Data::Thread*> thread) {
	const auto topic = thread->asTopic();
	if (!topic) {
		return NotificationsEnabledValue(thread->peer());
	}
	return rpl::merge(
		topic->session().changes().topicFlagsValue(
			topic,
			Data::TopicUpdate::Flag::Notifications
		) | rpl::to_empty,
		topic->session().changes().peerUpdates(
			topic->channel(),
			UpdateFlag::Notifications
		) | rpl::to_empty,
		topic->owner().notifySettings().defaultUpdates(topic->channel())
	) | rpl::map([=] {
		return !topic->owner().notifySettings().isMuted(topic);
	}) | rpl::distinct_until_changed();
}

rpl::producer<bool> NotificationsEnabledValue(not_null<PeerData*> peer) {
	return rpl::merge(
		peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Notifications
		) | rpl::to_empty,
		peer->owner().notifySettings().defaultUpdates(peer)
	) | rpl::map([=] {
		return !peer->owner().notifySettings().isMuted(peer);
	}) | rpl::distinct_until_changed();
}

rpl::producer<bool> IsContactValue(not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		UpdateFlag::IsContact
	) | rpl::map([=] {
		return user->isContact();
	});
}

[[nodiscard]] rpl::producer<QString> InviteToChatButton(
		not_null<UserData*> user) {
	if (!user->isBot()
		|| user->isRepliesChat()
		|| user->isVerifyCodes()
		|| user->isSupport()) {
		return rpl::single(QString());
	}
	using Flag = Data::PeerUpdate::Flag;
	return user->session().changes().peerFlagsValue(
		user,
		Flag::BotCanBeInvited | Flag::Rights
	) | rpl::map([=] {
		const auto info = user->botInfo.get();
		return info->cantJoinGroups
			? (info->channelAdminRights
				? tr::lng_profile_invite_to_channel(tr::now)
				: QString())
			: (info->channelAdminRights
				? tr::lng_profile_add_bot_as_admin(tr::now)
				: tr::lng_profile_invite_to_group(tr::now));
	});
}

[[nodiscard]] rpl::producer<QString> InviteToChatAbout(
		not_null<UserData*> user) {
	if (!user->isBot()
		|| user->isRepliesChat()
		|| user->isVerifyCodes()
		|| user->isSupport()) {
		return rpl::single(QString());
	}
	using Flag = Data::PeerUpdate::Flag;
	return user->session().changes().peerFlagsValue(
		user,
		Flag::BotCanBeInvited | Flag::Rights
	) | rpl::map([=] {
		const auto info = user->botInfo.get();
		return (info->cantJoinGroups || !info->groupAdminRights)
			? (info->channelAdminRights
				? tr::lng_profile_invite_to_channel_about(tr::now)
				: QString())
			: (info->channelAdminRights
				? tr::lng_profile_add_bot_as_admin_about(tr::now)
				: tr::lng_profile_invite_to_group_about(tr::now));
	});
}

rpl::producer<bool> CanShareContactValue(not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		UpdateFlag::CanShareContact
	) | rpl::map([=] {
		return user->canShareThisContact();
	});
}

rpl::producer<bool> CanAddContactValue(not_null<UserData*> user) {
	using namespace rpl::mappers;
	if (user->isBot() || user->isSelf() || user->isInaccessible()) {
		return rpl::single(false);
	}
	return IsContactValue(
		user
	) | rpl::map(!_1);
}

rpl::producer<Data::Birthday> BirthdayValue(not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		UpdateFlag::Birthday
	) | rpl::map([=] {
		return user->birthday();
	});
}

rpl::producer<ChannelData*> PersonalChannelValue(not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		UpdateFlag::PersonalChannel
	) | rpl::map([=] {
		const auto channelId = user->personalChannelId();
		return channelId ? user->owner().channel(channelId).get() : nullptr;
	});
}

rpl::producer<bool> AmInChannelValue(not_null<ChannelData*> channel) {
	return channel->session().changes().peerFlagsValue(
		channel,
		UpdateFlag::ChannelAmIn
	) | rpl::map([=] {
		return channel->amIn();
	});
}

rpl::producer<int> MembersCountValue(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Members
		) | rpl::map([=] {
			return chat->amIn()
				? std::max(chat->count, int(chat->participants.size()))
				: 0;
		});
	} else if (const auto channel = peer->asChannel()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Members
		) | rpl::map([=] {
			return channel->membersCount();
		});
	}
	Unexpected("User in MembersCountViewer().");
}

rpl::producer<int> PendingRequestsCountValue(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::PendingRequests
		) | rpl::map([=] {
			return chat->pendingRequestsCount();
		});
	} else if (const auto channel = peer->asChannel()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::PendingRequests
		) | rpl::map([=] {
			return channel->pendingRequestsCount();
		});
	}
	Unexpected("User in MembersCountViewer().");
}

rpl::producer<int> AdminsCountValue(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Admins | UpdateFlag::Rights
		) | rpl::map([=] {
			return chat->participants.empty()
				? 0
				: int(chat->admins.size() + (chat->creator ? 1 : 0));
		});
	} else if (const auto channel = peer->asChannel()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Admins | UpdateFlag::Rights
		) | rpl::map([=] {
			return channel->canViewAdmins()
				? channel->adminsCount()
				: 0;
		});
	}
	Unexpected("User in AdminsCountValue().");
}


rpl::producer<int> RestrictionsCountValue(not_null<PeerData*> peer) {
	const auto countOfRestrictions = [](
			Data::RestrictionsSetOptions options,
			ChatRestrictions restrictions) {
		auto count = 0;
		const auto list = Data::ListOfRestrictions(options);
		for (const auto &f : list) {
			if (restrictions & f) count++;
		}
		return int(list.size()) - count;
	};

	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Rights
		) | rpl::map([=] {
			return countOfRestrictions({}, chat->defaultRestrictions());
		});
	} else if (const auto channel = peer->asChannel()) {
		return rpl::combine(
			Data::PeerFlagValue(channel, ChannelData::Flag::Forum),
			channel->session().changes().peerFlagsValue(
				channel,
				UpdateFlag::Rights)
		) | rpl::map([=] {
			return countOfRestrictions(
				{ .isForum = channel->isForum() },
				channel->defaultRestrictions());
		});
	}
	Unexpected("User in RestrictionsCountValue().");
}

rpl::producer<not_null<PeerData*>> MigratedOrMeValue(
		not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Migration
		) | rpl::map([=] {
			return chat->migrateToOrMe();
		});
	} else {
		return rpl::single(peer);
	}
}

rpl::producer<int> RestrictedCountValue(not_null<ChannelData*> channel) {
	return channel->session().changes().peerFlagsValue(
		channel,
		UpdateFlag::BannedUsers | UpdateFlag::Rights
	) | rpl::map([=] {
		return channel->canViewBanned()
			? channel->restrictedCount()
			: 0;
	});
}

rpl::producer<int> KickedCountValue(not_null<ChannelData*> channel) {
	return channel->session().changes().peerFlagsValue(
		channel,
		UpdateFlag::BannedUsers | UpdateFlag::Rights
	) | rpl::map([=] {
		return channel->canViewBanned()
			? channel->kickedCount()
			: 0;
	});
}

rpl::producer<int> SharedMediaCountValue(
		not_null<PeerData*> peer,
		MsgId topicRootId,
		PeerId monoforumPeerId,
		PeerData *migrated,
		Storage::SharedMediaType type) {
	auto aroundId = 0;
	auto limit = 0;
	auto updated = SharedMediaMergedViewer(
		&peer->session(),
		SharedMediaMergedKey(
			SparseIdsMergedSlice::Key(
				peer->id,
				topicRootId,
				monoforumPeerId,
				migrated ? migrated->id : 0,
				aroundId),
			type),
		limit,
		limit
	) | rpl::map([](const SparseIdsMergedSlice &slice) {
		return slice.fullCount();
	}) | rpl::filter_optional();
	return rpl::single(0) | rpl::then(std::move(updated));
}

rpl::producer<int> CommonGroupsCountValue(not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		UpdateFlag::CommonChats
	) | rpl::map([=] {
		return user->commonChatsCount();
	});
}

rpl::producer<int> SimilarPeersCountValue(
		not_null<PeerData*> peer) {
	const auto participants = &peer->session().api().chatParticipants();
	participants->loadSimilarPeers(peer);
	return rpl::single(peer) | rpl::then(
		participants->similarLoaded()
	) | rpl::filter(
		rpl::mappers::_1 == peer
	) | rpl::map([=] {
		const auto &similar = participants->similar(peer);
		return int(similar.list.size()) + similar.more;
	});
}

rpl::producer<int> SavedSublistCountValue(
		not_null<PeerData*> peer) {
	const auto saved = &peer->owner().savedMessages();
	const auto sublist = saved->sublist(peer);
	if (!sublist->fullCount().has_value()) {
		sublist->loadFullCount();
		return rpl::single(0) | rpl::then(sublist->fullCountValue());
	}
	return sublist->fullCountValue();
}

rpl::producer<int> PeerGiftsCountValue(not_null<PeerData*> peer) {
	return peer->session().changes().peerFlagsValue(
		peer,
		UpdateFlag::PeerGifts
	) | rpl::map([=] {
		return peer->peerGiftsCount();
	});
}

rpl::producer<bool> CanAddMemberValue(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Rights
		) | rpl::map([=] {
			return chat->canAddMembers();
		});
	} else if (const auto channel = peer->asChannel()) {
		return peer->session().changes().peerFlagsValue(
			peer,
			UpdateFlag::Rights
		) | rpl::map([=] {
			return channel->canAddMembers();
		});
	}
	return rpl::single(false);
}

rpl::producer<int> FullReactionsCountValue(
		not_null<Main::Session*> session) {
	const auto reactions = &session->data().reactions();
	return rpl::single(rpl::empty) | rpl::then(
		reactions->defaultUpdates()
	) | rpl::map([=] {
		return int(reactions->list(Data::Reactions::Type::Active).size());
	}) | rpl::distinct_until_changed();
}

rpl::producer<bool> CanViewParticipantsValue(
		not_null<ChannelData*> megagroup) {
	if (megagroup->amCreator()) {
		return rpl::single(true);
	}
	return rpl::combine(
		megagroup->session().changes().peerFlagsValue(
			megagroup,
			UpdateFlag::Rights),
		megagroup->flagsValue(),
		[=] { return megagroup->canViewMembers(); }
	) | rpl::distinct_until_changed();
}

template <typename Flag, typename Peer>
rpl::producer<BadgeType> BadgeValueFromFlags(Peer peer) {
	return rpl::combine(
		Data::PeerFlagsValue(
			peer,
			Flag::Verified | Flag::Scam | Flag::Fake),
		Data::PeerPremiumValue(peer)
	) | rpl::map([=](base::flags<Flag> value, bool premium) {
		return (value & Flag::Scam)
			? BadgeType::Scam
			: (value & Flag::Fake)
			? BadgeType::Fake
			: peer->isMonoforum()
			? BadgeType::Direct
			: (value & Flag::Verified)
			? BadgeType::Verified
			: premium
			? BadgeType::Premium
			: BadgeType::None;
	});
}

rpl::producer<BadgeType> BadgeValue(not_null<PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return BadgeValueFromFlags<UserDataFlag>(user);
	} else if (const auto channel = peer->asChannel()) {
		return BadgeValueFromFlags<ChannelDataFlag>(channel);
	}
	return rpl::single(BadgeType::None);
}

rpl::producer<EmojiStatusId> EmojiStatusIdValue(not_null<PeerData*> peer) {
	if (peer->isChat()) {
		return rpl::single(EmojiStatusId());
	}
	return peer->session().changes().peerFlagsValue(
		peer,
		Data::PeerUpdate::Flag::EmojiStatus
	) | rpl::map([=] { return peer->emojiStatusId(); });
}

rpl::producer<QString> BirthdayLabelText(
		rpl::producer<Data::Birthday> birthday) {
	return std::move(birthday) | rpl::map([](Data::Birthday value) {
		return rpl::conditional(
			Data::IsBirthdayTodayValue(value),
			tr::lng_info_birthday_today_label(),
			tr::lng_info_birthday_label());
	}) | rpl::flatten_latest();
}

rpl::producer<QString> BirthdayValueText(
		rpl::producer<Data::Birthday> birthday) {
	return std::move(
		birthday
	) | rpl::map([](Data::Birthday value) -> rpl::producer<QString> {
		if (!value) {
			return rpl::single(QString());
		}
		return Data::IsBirthdayTodayValue(
			value
		) | rpl::map([=](bool today) {
			auto text = Data::BirthdayText(value);
			if (const auto age = Data::BirthdayAge(value)) {
				text = (today
					? tr::lng_info_birthday_today_years
					: tr::lng_info_birthday_years)(
						tr::now,
						lt_count,
						age,
						lt_date,
						text);
			}
			if (today) {
				text = tr::lng_info_birthday_today(
					tr::now,
					lt_emoji,
					Data::BirthdayCake(),
					lt_date,
					text);
			}
			return text;
		});
	}) | rpl::flatten_latest();
}

} // namespace Profile
} // namespace Info
