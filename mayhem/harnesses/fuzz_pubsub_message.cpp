// Fuzz the Twitch PubSub websocket client: one raw websocket text frame (as received from
// pubsub-edge.twitch.tv) -> PubSubClient::onMessage, the client's own entry point. It parses the
// envelope (parsePubSubBaseMessage), the MESSAGE wrapper (PubSubMessageMessage, whose payload is a
// JSON document inside a JSON string) and the pinned-chat / community-points inner messages.
// pointReward.redeemed has no listener: its consumer (ChannelPointReward) creates Images in the
// process-wide image cache, which would carry state from one input to the next.
// Kept across inputs on purpose: the QCoreApplication and the PubSub manager, which is never
// started (no connection); an input can only bump its diag counters, which nothing reads back.
// Rebuilt per input: the PubSubClient (nonces, heartbeat state).
#include "providers/twitch/PubSubClient.hpp"
#include "providers/twitch/PubSubManager.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <chrono>
#include <cstdint>
#include <memory>

using namespace chatterino;

namespace {
PubSub *pubsub = nullptr;
}  // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_LOGGING_RULES", "*=false");
    new QCoreApplication(*argc, *argv);
    pubsub = new PubSub(QStringLiteral("wss://127.0.0.1:1"));
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    QByteArray frame(reinterpret_cast<const char *>(data),
                     static_cast<qsizetype>(size));
    auto client = std::make_shared<PubSubClient>(*pubsub,
                                                 std::chrono::seconds(15));
    client->onMessage(frame);
    return 0;
}
