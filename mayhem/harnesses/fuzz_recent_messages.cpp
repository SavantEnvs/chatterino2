// Fuzz the recent-messages path: one JSON response body of the recent-messages API
// (recent-messages.robotty.de or a user-configured server, fetched when a channel is joined) ->
// recentmessages::detail::parseRecentMessages, which pulls the raw IRC lines out of "messages"
// and runs Communi::IrcMessage::fromData on each, -> buildRecentMessages, which turns them into
// displayed messages through IrcMessageHandler::parseMessageInto (plus day separators from
// rm-received-ts). Mock fixture shared with fuzz_irc_message.cpp (copied from
// tests/src/IrcMessageHandler.cpp).
// Kept across inputs on purpose: the mock application (settings/emotes/badges, read-only for
// parsing). Rebuilt per input: the TwitchChannel (recent chatters, colors, lastDate_) and sink.

#include "providers/recentmessages/Impl.hpp"
#include "providers/twitch/IrcMessageHandler.hpp"

#include "common/Env.hpp"
#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/highlights/HighlightController.hpp"
#include "controllers/ignores/IgnorePhrase.hpp"
#include "controllers/sound/NullBackend.hpp"
#include "messages/Emote.hpp"
#include "messages/Message.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/ChatterinoBadges.hpp"
#include "mocks/DisabledStreamerMode.hpp"
#include "mocks/EmoteController.hpp"
#include "mocks/Helix.hpp"
#include "mocks/LinkResolver.hpp"
#include "mocks/Logging.hpp"
#include "mocks/TwitchIrcServer.hpp"
#include "mocks/UserData.hpp"
#include "providers/bttv/BttvBadges.hpp"
#include "providers/ffz/FfzBadges.hpp"
#include "providers/seventv/SeventvBadges.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/ChannelPointReward.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "providers/twitch/TwitchBadges.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/IrcHelpers.hpp"
#include "util/VectorMessageSink.hpp"

#include <IrcConnection>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringBuilder>

#include <unordered_map>
#include <vector>

using namespace chatterino;
using namespace literals;
#include "common/Args.hpp"
#include "common/Modes.hpp"
#include "common/network/NetworkManager.hpp"
#include "singletons/Resources.hpp"

#include <gmock/gmock.h>
#include <QApplication>
#include <QLocale>
#include <QLoggingCategory>
#include <QTemporaryDir>

#include <cstdint>
#include <cstdlib>

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication(const QString &settingsData)
        : mock::BaseApplication(settingsData, /*runMigrations*/ true)
        , highlights(this->settings, &this->accounts)
    {
    }

    EmoteController *getEmotes() override
    {
        return &this->emotes;
    }

    IUserDataController *getUserData() override
    {
        return &this->userData;
    }

    AccountController *getAccounts() override
    {
        return &this->accounts;
    }

    ITwitchIrcServer *getTwitch() override
    {
        return &this->twitch;
    }

    IChatterinoBadges *getChatterinoBadges() override
    {
        return &this->chatterinoBadges;
    }

    FfzBadges *getFfzBadges() override
    {
        return &this->ffzBadges;
    }

    BttvBadges *getBttvBadges() override
    {
        return &this->bttvBadges;
    }

    SeventvBadges *getSeventvBadges() override
    {
        return &this->seventvBadges;
    }

    HighlightController *getHighlights() override
    {
        return &this->highlights;
    }

    BttvEmotes *getBttvEmotes() override
    {
        return &this->bttvEmotes;
    }

    FfzEmotes *getFfzEmotes() override
    {
        return &this->ffzEmotes;
    }

    SeventvEmotes *getSeventvEmotes() override
    {
        return &this->seventvEmotes;
    }

    ILogging *getChatLogger() override
    {
        return &this->logging;
    }

    TwitchBadges *getTwitchBadges() override
    {
        return &this->twitchBadges;
    }

    ILinkResolver *getLinkResolver() override
    {
        return &this->linkResolver;
    }

    ISoundController *getSound() override
    {
        return &this->sound;
    }

    mock::EmptyLogging logging;
    AccountController accounts;
    mock::EmoteController emotes;
    mock::Helix helix;
    mock::UserDataController userData;
    mock::MockTwitchIrcServer twitch;
    mock::ChatterinoBadges chatterinoBadges;
    FfzBadges ffzBadges;
    BttvBadges bttvBadges;
    SeventvBadges seventvBadges;
    HighlightController highlights;
    BttvEmotes bttvEmotes;
    FfzEmotes ffzEmotes;
    SeventvEmotes seventvEmotes;
    TwitchBadges twitchBadges;
    mock::EmptyLinkResolver linkResolver;
    NullBackend sound;
};

std::pair<const EmoteName, EmotePtr> makeEmote(Emote &&emote)
{
    auto ptr = std::make_shared<Emote>(std::move(emote));
    ptr->homePage = {u"https://chatterino.com/" % ptr->name.string};
    ptr->tooltip = {ptr->name.string % u" Tooltip"_s};
    ptr->author = {u"Chatterino"_s};
    ptr->images = {
        Url{u"https://chatterino.com/" % ptr->name.string % u".png"}};
    return {ptr->name, ptr};
}

using EmoteMapPtr = std::shared_ptr<const EmoteMap>;

EmoteMapPtr makeEmotes(auto &&...emotes)
{
    auto map = std::make_shared<EmoteMap>();
    ((map->emplace(makeEmote(std::forward<decltype(emotes)>(emotes)))), ...);
    return map;
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wmissing-field-initializers")

struct MockEmotes {
    EmoteMapPtr seventv;
    EmoteMapPtr bttv;
    EmoteMapPtr ffz;
    EmoteMapPtr twitchAccount;

    static MockEmotes channel()
    {
        return {
            .seventv = makeEmotes(
                Emote{
                    .name = {u"7TVEmote"_s},
                    .id = {u"1"_s},
                },
                Emote{
                    .name = {u"7TVEmote0w"_s},
                    .zeroWidth = true,
                    .id = {u"2"_s},
                    .baseName = EmoteName{u"ZeroWidth"_s},
                },
                Emote{
                    .name = {u"PogChamp"_s},
                    .id = {u"3"_s},
                }),
            .bttv = makeEmotes(
                Emote{
                    .name = {u"BTTVEmote"_s},
                },
                Emote{
                    .name = {u"Kappa"_s},
                }),
            .ffz = makeEmotes(
                Emote{
                    .name = {u"FFZEmote"_s},
                },
                Emote{
                    .name = {u"Keepo"_s},
                }),
        };
    }

    static MockEmotes twitchdev()
    {
        return {
            .seventv = makeEmotes(Emote{
                .name = {u"7TVTwitchDev"_s},
                .id = {u"t5"_s},
            }),
            .bttv = makeEmotes(Emote{
                .name = {u"BTTVTwitchDev"_s},
            }),
            .ffz = makeEmotes(Emote{
                .name = {u"FFZTwitchDev"_s},
            }),
        };
    }

    static MockEmotes global()
    {
        return {
            .seventv = makeEmotes(Emote{
                .name = {u"7TVGlobal"_s},
                .id = {u"G1"_s},
            }),
            .bttv = makeEmotes(Emote{
                .name = {u"BTTVGlobal"_s},
            }),
            .ffz = makeEmotes(Emote{
                .name = {u"FFZGlobal"_s},
            }),
            .twitchAccount = makeEmotes(Emote{
                .name = {u"MyCoolTwitchEmote"_s},
                .id = {u"5678"_s},
            }),
        };
    }
};

const QByteArray CHEERMOTE_JSON{R"({
    "prefix": "Cheer",
    "tiers": [
    {
        "min_bits": 1,
        "id": "1",
        "color": "#979797",
        "images": {
            "dark": {
                "animated": {
                    "1": "https://chatterino.com/bits/1.gif",
                    "2": "https://chatterino.com/bits/2.gif",
                    "4": "https://chatterino.com/bits/4.gif"
                },
                "static": {
                    "1": "https://chatterino.com/bits/1.png",
                    "2": "https://chatterino.com/bits/2.png",
                    "4": "https://chatterino.com/bits/4.png"
                }
            }
        },
        "can_cheer": true,
        "show_in_bits_card": true
    },
    {
        "min_bits": 100,
        "id": "100",
        "color": "#9c3ee8",
        "images": {
            "dark": {
                "animated": {
                    "1": "https://chatterino.com/bits/1.gif",
                    "2": "https://chatterino.com/bits/2.gif",
                    "4": "https://chatterino.com/bits/4.gif"
                },
                "static": {
                    "1": "https://chatterino.com/bits/1.png",
                    "2": "https://chatterino.com/bits/2.png",
                    "4": "https://chatterino.com/bits/4.png"
                }
            }
        },
        "can_cheer": true,
        "show_in_bits_card": true
    }
    ],
    "type": "global_first_party",
    "order": 1,
    "last_updated": "2018-05-22T00:06:04Z",
    "is_charitable": false
})"_ba};

const QByteArray LOCAL_BADGE_JSON{R"({
    "data": [
        {
            "set_id": "subscriber",
            "versions": [
                {
                "click_url": null,
                "description": "Subscriber",
                "id": "3072",
                "image_url_1x": "https://chatterino.com/tb-1",
                "image_url_2x": "https://chatterino.com/tb-2",
                "image_url_4x": "https://chatterino.com/tb-3",
                "title": "Subscriber"
                }
            ]
        }
    ]
})"_ba};

const QByteArray SETTINGS_DEFAULT{"{}"_ba};

std::shared_ptr<TwitchChannel> makeMockTwitchChannel(
    const QString &name)
{
    auto chan = std::make_shared<TwitchChannel>(name);
    auto mocks = MockEmotes::channel();
    chan->setSeventvEmotes(std::move(mocks.seventv));
    chan->setBttvEmotes(std::move(mocks.bttv));
    chan->setFfzEmotes(std::move(mocks.ffz));

    QJsonObject defaultImage{
        {u"url_1x"_s, u"https://chatterino.com/reward1x.png"_s},
        {u"url_2x"_s, u"https://chatterino.com/reward2x.png"_s},
        {u"url_4x"_s, u"https://chatterino.com/reward4x.png"_s},
    };
    chan->addKnownChannelPointReward({{
        {u"channel_id"_s, u"11148817"_s},
        {u"id"_s, u"unused"_s},
        {u"reward"_s,
         {{
             {u"channel_id"_s, u"11148817"_s},
             {u"cost"_s, 1},
             {u"id"_s, u"31a2344e-0fce-4229-9453-fb2e8b6dd02c"_s},
             {u"is_user_input_required"_s, true},
             {u"title"_s, u"my reward"_s},
             {u"image"_s, defaultImage},
         }}},
    }});
    chan->addKnownChannelPointReward({{
        {u"channel_id"_s, u"11148817"_s},
        {u"id"_s, u"unused"_s},
        {u"reward"_s,
         {{
             {u"channel_id"_s, u"11148817"_s},
             {u"cost"_s, 1},
             {u"id"_s, u"dc8d1dac-256e-42b9-b7ba-40b32e5294e2"_s},
             {u"is_user_input_required"_s, false},
             {u"title"_s, u"test"_s},
             {u"image"_s, defaultImage},
         }}},
    }});
    chan->addKnownChannelPointReward({{
        {u"channel_id"_s, u"11148817"_s},
        {u"id"_s, u"unused"_s},
        {u"reward"_s,
         {{
             {u"channel_id"_s, u"11148817"_s},
             {u"cost"_s, 1},
             {u"default_bits_cost"_s, 2},
             {u"bits_cost"_s, 0},
             {u"pricing_type"_s, u"BITS"_s},
             {u"reward_type"_s, u"CELEBRATION"_s},
             {u"is_user_input_required"_s, false},
             {u"title"_s, u"BitReward"_s},
             {u"image"_s, defaultImage},
         }}},
        {u"redemption_metadata"_s,
         QJsonObject{
             {u"celebration_emote_metadata"_s,
              QJsonObject{
                  {u"emote"_s,
                   {{
                       {u"id"_s, u"42"_s},
                       {u"token"_s, u"MyBitsEmote"_s},
                   }}},
              }},
         }},
    }});

    chan->setUserColor("UserColor", {1, 2, 3, 4});
    chan->setUserColor("UserColor2", {5, 6, 7, 8});
    chan->addRecentChatter("UserChatter");
    chan->addRecentChatter("UserColor");

    chan->setCheerEmoteSets({
        HelixCheermoteSet{QJsonDocument::fromJson(CHEERMOTE_JSON).object()},
    });

    chan->setFfzChannelBadges({{u"123456"_s, {3, 4}}});

    chan->addTwitchBadgeSets(HelixChannelBadges{
        QJsonDocument::fromJson(LOCAL_BADGE_JSON).object(),
    });

    if (false)
    {
        chan->setFfzCustomVipBadge(std::make_shared<Emote>(Emote{
            .name = {},
            .images = {Url{"https://chatterino.com/ffz-vip1x.png"}},
            .tooltip = {"VIP"},
            .homePage = {},
        }));
    }
    if (false)
    {
        chan->setFfzCustomModBadge(std::make_shared<Emote>(Emote{
            .name = {},
            .images = {Url{"https://chatterino.com/ffz-mod1x.png"}},
            .tooltip = {"Moderator"},
            .homePage = {},
        }));
    }

    return chan;
}

QT_WARNING_POP

MockApplication *gApp = nullptr;

void setUpApplication()
{
    // Everything global is heap-allocated and never destroyed: Qt objects must not be torn down
    // by exit-time static destructors after QApplication is gone.
    auto *settingsDir = new QTemporaryDir;
    auto *args = new Args;
    new Settings(Modes(*args), *args, settingsDir->path());

    gApp = new MockApplication(QString::fromUtf8(SETTINGS_DEFAULT));
    // Helix calls (e.g. shared-chat lookups) go to the app's gmock Helix, as in the tests.
    testing::GMOCK_FLAG(verbose) = "error";
    initializeHelix(&gApp->helix);

    auto mocks = MockEmotes::global();
    gApp->seventvEmotes.setGlobalEmotes(mocks.seventv);
    gApp->bttvEmotes.setEmotes(mocks.bttv);
    gApp->ffzEmotes.setEmotes(mocks.ffz);
    gApp->getAccounts()->twitch.getCurrent()->setEmotes(mocks.twitchAccount);
    gApp->getUserData()->setUserColor(u"117691339"_s, u"#DAA521"_s);
    gApp->getAccounts()->twitch.getCurrent()->blockUserLocally(u"12345"_s,
                                                              u"blocked"_s);
    gApp->getTwitchBadges()->loadLocalBadges();

    auto twitchdev = std::make_shared<TwitchChannel>("twitchdev");
    auto tdMocks = MockEmotes::twitchdev();
    twitchdev->setSeventvEmotes(std::move(tdMocks.seventv));
    twitchdev->setBttvEmotes(std::move(tdMocks.bttv));
    twitchdev->setFfzEmotes(std::move(tdMocks.ffz));
    gApp->twitch.mockChannels.emplace("twitchdev", twitchdev);
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_QPA_PLATFORM", "minimal");
    qputenv("QT_LOGGING_RULES", "*=false");
    new QApplication(*argc, *argv);
    QLocale::setDefault(QLocale("en_GB"));
    initResources();
    NetworkManager::init();
    setUpApplication();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    QByteArray body(reinterpret_cast<const char *>(data),
                    static_cast<qsizetype>(size));
    // Mirrors recentmessages::load's onSuccess: NetworkResult::parseJson, then the two steps.
    auto root = QJsonDocument::fromJson(body).object();
    auto ircMessages = recentmessages::detail::parseRecentMessages(root);
    {
        auto channel = makeMockTwitchChannel(u"pajlada"_s);
        auto messages =
            recentmessages::detail::buildRecentMessages(ircMessages, channel.get());
        for (const auto &message : messages)
        {
            for (const auto &element : message->elements)
            {
                (void)element->toJson();
            }
        }
    }
    // buildRecentMessages deleteLater()s every IrcMessage it consumed.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return 0;
}
