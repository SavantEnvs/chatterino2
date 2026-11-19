// Fuzz the 7TV EventAPI websocket client: one raw websocket text frame (as received from
// events.7tv.io) -> seventv::eventapi::Client::onMessage, the client's own entry point. It parses
// the envelope (parseBaseMessage), then for a dispatch walks the body into Dispatch,
// EmoteAdd/Update/RemoveDispatch, UserConnectionUpdateDispatch, CosmeticCreateDispatch and
// EntitlementCreateDeleteDispatch, validating each.
// Kept across inputs on purpose: the QCoreApplication and the SeventvEventAPI manager, which is
// never started (no connection, no subscriptions) and whose signals have no listeners, so an
// input cannot change it. Rebuilt per input: the Client (heartbeat state). No IApplication is
// installed, so the cosmetic/entitlement handlers stop at tryGetApp() == nullptr instead of
// mutating global badge state.
#include "providers/seventv/eventapi/Client.hpp"
#include "providers/seventv/SeventvEventAPI.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <chrono>
#include <cstdint>
#include <memory>

using namespace chatterino;

namespace {
SeventvEventAPI *api = nullptr;
}  // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_LOGGING_RULES", "*=false");
    new QCoreApplication(*argc, *argv);
    api = new SeventvEventAPI(QStringLiteral("wss://127.0.0.1:1/v3"),
                              std::chrono::milliseconds(25000));
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    QByteArray frame(reinterpret_cast<const char *>(data),
                     static_cast<qsizetype>(size));
    auto client = std::make_shared<seventv::eventapi::Client>(
        *api, std::chrono::milliseconds(25000));
    client->onMessage(frame);
    return 0;
}
