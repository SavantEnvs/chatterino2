// Fuzz chatterino's message-filter language (src/controllers/filters/lang): tokenizer, parser,
// type checker and evaluator. Filters come from user config, shared filter strings and plugins.
// Per input: Filter::fromString(); a valid filter is printed (filterString/debugString) and
// executed against a fixed message, as in tests/src/Filters.cpp. The fixed message is rebuilt
// per input, so nothing an input does carries to the next.
#include "controllers/filters/lang/Filter.hpp"
#include "messages/Message.hpp"
#include "mocks/BaseApplication.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "singletons/Resources.hpp"

#include <QApplication>
#include <QColor>
#include <QString>

#include <cstdint>
#include <variant>

using namespace chatterino;
using namespace chatterino::filters;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_QPA_PLATFORM", "minimal");
    qputenv("QT_LOGGING_RULES", "*=false");
    // Heap-allocated and never destroyed, so no Qt object is torn down at exit.
    new QApplication(*argc, *argv);
    initResources();
    new mock::BaseApplication();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const QString input = QString::fromUtf8(
        reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    auto result = Filter::fromString(input);
    auto *filter = std::get_if<Filter>(&result);
    if (filter == nullptr)
    {
        return 0;
    }
    (void)filter->filterString();
    (void)filter->debugString();

    Message message;
    message.displayName = "icelys";
    message.usernameColor = QColor(0xff0000);
    message.messageText = "hey there :) 2038-01-19 123 456";
    message.channelName = "forsen";
    message.twitchBadges = {
        TwitchBadge("moderator", ""),
        TwitchBadge("staff", ""),
    };
    message.externalBadges = {"frankerfacez:bot"};
    RunContext ctx{
        .message = message,
        .channel = nullptr,
    };
    (void)filter->execute(ctx);
    return 0;
}
