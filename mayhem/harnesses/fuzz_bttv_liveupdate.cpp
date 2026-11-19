// Fuzz the BetterTTV live-update websocket client: one raw websocket text frame (as received from
// sockets.betterttv.net) -> BttvLiveUpdateClient::onMessage, the client's own entry point. It
// parses the JSON and builds/validates BttvLiveUpdateEmoteUpdateAddMessage,
// BttvLiveUpdateEmoteRemoveMessage and BttvLiveUpdateUserUpdateMessage.
// Kept across inputs on purpose: the QCoreApplication and the BttvLiveUpdates manager, which is
// never started (no connection, no subscriptions) and whose signals have no listeners. Rebuilt per
// input: the client. No IApplication is installed, so lookup_user stops at tryGetApp() == nullptr
// instead of registering badges in global state.
#include "providers/bttv/BttvLiveUpdates.hpp"
#include "providers/bttv/liveupdates/BttvLiveUpdateClient.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <cstdint>
#include <memory>

using namespace chatterino;

namespace {
BttvLiveUpdates *manager = nullptr;
}  // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_LOGGING_RULES", "*=false");
    new QCoreApplication(*argc, *argv);
    manager = new BttvLiveUpdates(QStringLiteral("wss://127.0.0.1:1/ws"));
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    QByteArray frame(reinterpret_cast<const char *>(data),
                     static_cast<qsizetype>(size));
    auto client = std::make_shared<BttvLiveUpdateClient>(*manager);
    client->onMessage(frame);
    return 0;
}
