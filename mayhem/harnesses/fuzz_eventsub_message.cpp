// Fuzz Twitch EventSub websocket messages: raw JSON frame -> Session::handleMessage, which parses
// the envelope with boost::json and runs the generated per-subscription payload deserializers.
// Mirrors lib/twitch-eventsub-ws/tests/src/parse.cpp. No state survives an input: a fresh
// io_context, Session and listener per input. The io_context is declared before the Session so the
// Session (its strands and the keepalive timer a session_welcome arms) is destroyed first, and the
// context's destructor then drops any handler that was queued for it. Kept across inputs on purpose:
// the TLS client context, which handleMessage never touches (it only matters for connecting).
#include "twitch-eventsub-ws/listener.hpp"
#include "twitch-eventsub-ws/logger.hpp"
#include "twitch-eventsub-ws/session.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <cstdint>
#include <cstring>
#include <memory>

namespace {

using namespace chatterino::eventsub::lib;

class NoOpListener : public chatterino::eventsub::lib::Listener
{
    void onSessionWelcome(
        const messages::Metadata &metadata,
        const payload::session_welcome::Payload &payload) override
    {
    }

    void onNotification(const messages::Metadata &metadata,
                        const boost::json::value &jv) override
    {
    }

    void onChannelBan(const messages::Metadata &metadata,
                      const payload::channel_ban::v1::Payload &payload) override
    {
    }

    void onStreamOnline(
        const messages::Metadata &metadata,
        const payload::stream_online::v1::Payload &payload) override
    {
    }

    void onStreamOffline(
        const messages::Metadata &metadata,
        const payload::stream_offline::v1::Payload &payload) override
    {
    }

    void onChannelChatNotification(
        const messages::Metadata &metadata,
        const payload::channel_chat_notification::v1::Payload &payload) override
    {
    }

    void onChannelUpdate(
        const messages::Metadata &metadata,
        const payload::channel_update::v1::Payload &payload) override
    {
    }

    void onChannelChatMessage(
        const messages::Metadata &metadata,
        const payload::channel_chat_message::v1::Payload &payload) override
    {
    }

    void onChannelModerate(
        const messages::Metadata &metadata,
        const payload::channel_moderate::v2::Payload &payload) override
    {
    }

    void onAutomodMessageHold(
        const messages::Metadata &metadata,
        const payload::automod_message_hold::v2::Payload &payload) override
    {
    }
    void onAutomodMessageUpdate(
        const messages::Metadata &metadata,
        const payload::automod_message_update::v2::Payload &payload) override
    {
    }
    void onChannelSuspiciousUserMessage(
        const messages::Metadata &metadata,
        const payload::channel_suspicious_user_message::v1::Payload &payload)
        override
    {
    }
    void onChannelSuspiciousUserUpdate(
        const messages::Metadata &metadata,
        const payload::channel_suspicious_user_update::v1::Payload &payload)
        override
    {
    }
    void onChannelChatUserMessageHold(
        const messages::Metadata &metadata,
        const payload::channel_chat_user_message_hold::v1::Payload &payload)
        override
    {
    }
    void onChannelChatUserMessageUpdate(
        const messages::Metadata &metadata,
        const payload::channel_chat_user_message_update::v1::Payload &payload)
        override
    {
    }
};

boost::asio::ssl::context &ssl()
{
    static boost::asio::ssl::context ctx(
        boost::asio::ssl::context::method::tls_client);
    return ctx;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    boost::beast::flat_buffer buf;
    auto inner = buf.prepare(size);
    if (size > 0)
    {
        std::memcpy(inner.data(), data, size);
    }
    buf.commit(size);

    boost::asio::io_context ioc;
    {
        auto log = std::make_shared<NullLogger>();
        std::unique_ptr<Listener> listener = std::make_unique<NoOpListener>();
        auto sess =
            std::make_shared<Session>(ioc, ssl(), std::move(listener), log);
        (void)sess->handleMessage(buf);
    }
    return 0;
}
