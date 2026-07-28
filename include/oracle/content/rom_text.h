#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "oracle/content/rom_source.h"

namespace oracle::content {

using MessageId = std::uint16_t;

enum class TextAtomKind : std::uint8_t {
    glyph,
    new_line,
    option,
    stop,
    color,
    substitution,
    symbol,
    command,
};

struct TextAtom {
    TextAtomKind kind{};
    std::uint8_t value{};
    char glyph{};
};

struct DecodedMessage {
    MessageId id{};
    std::vector<std::uint8_t> original_bytes;
    std::vector<TextAtom> atoms;
    std::vector<std::string> pages;
    std::size_t option_count{};
};

// Decodes the US cartridge text tables, including their four recursive
// dictionary groups. Control codes remain semantic atoms; the runtime does not
// flatten options, stops, colors, or substitutions into presentation markup.
class RomTextDecoder {
public:
    explicit RomTextDecoder(const RomSource& rom);

    [[nodiscard]] DecodedMessage decode(MessageId id) const;

private:
    const RomSource& rom_;
};

}  // namespace oracle::content
