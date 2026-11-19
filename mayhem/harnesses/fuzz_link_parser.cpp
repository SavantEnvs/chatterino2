// Fuzz chatterino's link scanner: every word of every chat message is passed to
// linkparser::parse() (src/common/LinkParser.cpp) to find URLs (protocol, host/TLD, IPv4, port,
// path). Input bytes are decoded as UTF-8 like chat text. Stateless.
#include "common/LinkParser.hpp"

#include <QList>
#include <QString>
#include <QStringView>

#include <cstdint>

using namespace chatterino;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const QString source = QString::fromUtf8(
        reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    // Chat messages are split on spaces before links are parsed; do the same, and also feed the
    // whole input once.
    auto check = [](QStringView word) {
        auto parsed = linkparser::parse(word);
        if (parsed)
        {
            volatile qsizetype n = parsed->link.size() + parsed->host.size() +
                                   parsed->rest.size() + parsed->protocol.size() +
                                   parsed->prefix(word).size() +
                                   parsed->suffix(word).size();
            (void)n;
        }
    };
    check(source);
    for (auto word : QStringView(source).split(u' ', Qt::SkipEmptyParts))
    {
        check(word);
    }
    return 0;
}
