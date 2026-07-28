#include "oracle/content/rom_text.h"

#include <cstddef>
#include <stdexcept>

namespace oracle::content {
namespace {

constexpr std::size_t ages_text_base_1_table = 0xfcfb3;
constexpr std::size_t ages_text_base_2_table = 0xfcfcb;
constexpr std::size_t ages_language_table = 0xfcfe3;
constexpr std::size_t seasons_text_base_1_table = 0xfcfe2;
constexpr std::size_t seasons_text_base_2_table = 0xfcffa;
constexpr std::size_t seasons_language_table = 0xfd012;
constexpr std::uint8_t second_text_base_group = 0x2c;
constexpr std::size_t maximum_message_bytes = 8192;
constexpr std::size_t maximum_dictionary_depth = 16;

struct TextTables {
    std::size_t base_1{};
    std::size_t base_2{};
    std::size_t group_table{};
};

std::size_t banked_offset(
    const std::uint8_t bank,
    const std::uint16_t address) {
    return
        static_cast<std::size_t>(bank) * 0x4000 +
        (address & 0x3fff);
}

std::size_t read_bank_then_pointer(
    const RomSource& rom,
    const std::size_t offset) {
    return banked_offset(
        rom.read_byte(offset),
        rom.read_little_u16(offset + 1));
}

std::size_t read_pointer_then_bank(
    const RomSource& rom,
    const std::size_t offset) {
    return banked_offset(
        rom.read_byte(offset + 2),
        rom.read_little_u16(offset));
}

TextTables text_tables(const RomSource& rom) {
    const bool ages =
        rom.metadata().campaign == core::Campaign::ages;
    const auto base_1_table =
        ages ? ages_text_base_1_table : seasons_text_base_1_table;
    const auto base_2_table =
        ages ? ages_text_base_2_table : seasons_text_base_2_table;
    const auto language_table =
        ages ? ages_language_table : seasons_language_table;
    return TextTables{
        read_bank_then_pointer(rom, base_1_table),
        read_bank_then_pointer(rom, base_2_table),
        read_pointer_then_bank(rom, language_table),
    };
}

std::size_t text_address(
    const RomSource& rom,
    const TextTables tables,
    const std::uint8_t high_index,
    const std::uint8_t low_index) {
    const auto group =
        tables.group_table +
        rom.read_little_u16(
            tables.group_table +
            static_cast<std::size_t>(high_index) * 2);
    const auto relative =
        rom.read_little_u16(
            group + static_cast<std::size_t>(low_index) * 2);
    const auto base =
        high_index < second_text_base_group
        ? tables.base_1
        : tables.base_2;
    const auto address = base + relative;
    if (address >= rom.bytes().size()) {
        throw std::runtime_error{
            "text pointer exceeds the ROM source"};
    }
    return address;
}

void decompress(
    const RomSource& rom,
    const TextTables tables,
    const std::uint8_t high_index,
    const std::uint8_t low_index,
    const std::size_t depth,
    const bool dictionary,
    std::vector<std::uint8_t>& output) {
    if (depth > maximum_dictionary_depth) {
        throw std::runtime_error{
            "text dictionary recursion exceeds its safety limit"};
    }
    auto cursor =
        text_address(rom, tables, high_index, low_index);
    while (output.size() < maximum_message_bytes) {
        const auto byte = rom.read_byte(cursor++);
        if (byte >= 2 && byte < 6) {
            const auto dictionary_index = rom.read_byte(cursor++);
            decompress(
                rom,
                tables,
                static_cast<std::uint8_t>(byte - 2),
                dictionary_index,
                depth + 1,
                true,
                output);
            continue;
        }
        if (byte >= 6 && byte < 0x10) {
            output.push_back(byte);
            output.push_back(rom.read_byte(cursor++));
            continue;
        }
        if (byte == 0) {
            if (!dictionary) {
                output.push_back(0);
            }
            return;
        }
        output.push_back(byte);
    }
    throw std::runtime_error{
        "decoded text exceeds its safety limit"};
}

void append_atom_text(
    std::string& page,
    const TextAtom atom) {
    switch (atom.kind) {
    case TextAtomKind::glyph:
        page.push_back(atom.glyph);
        break;
    case TextAtomKind::new_line:
        page.push_back('\n');
        break;
    case TextAtomKind::option:
        page.append("> ");
        break;
    case TextAtomKind::substitution:
        page.append(atom.value == 0 ? "Link" : "Child");
        break;
    case TextAtomKind::symbol:
        page.push_back('*');
        break;
    case TextAtomKind::stop:
    case TextAtomKind::color:
    case TextAtomKind::command:
        break;
    }
}

TextAtom decode_atom(
    const std::uint8_t byte,
    const std::uint8_t parameter) {
    if (byte == 1) {
        return TextAtom{TextAtomKind::new_line};
    }
    if (byte >= 0x20 && byte < 0x7e) {
        return TextAtom{
            TextAtomKind::glyph,
            byte,
            static_cast<char>(byte == 0x5c ? '~' : byte),
        };
    }
    if (byte == 0x7e || byte == 0x7f) {
        return TextAtom{TextAtomKind::symbol, byte};
    }
    if (byte == 9) {
        return TextAtom{TextAtomKind::color, parameter};
    }
    if (byte == 0x0a) {
        return TextAtom{TextAtomKind::substitution, parameter};
    }
    if (byte == 0x0c) {
        const auto command = static_cast<std::uint8_t>(parameter >> 3u);
        if (command == 2) {
            return TextAtom{TextAtomKind::option, parameter};
        }
        if (command == 3) {
            return TextAtom{TextAtomKind::stop, parameter};
        }
    }
    if (
        (byte >= 0x10 && byte <= 0x19) ||
        (byte >= 0x80 && byte < 0xb1)) {
        return TextAtom{TextAtomKind::symbol, byte};
    }
    return TextAtom{TextAtomKind::command, parameter};
}

}  // namespace

RomTextDecoder::RomTextDecoder(const RomSource& rom) : rom_{rom} {}

DecodedMessage RomTextDecoder::decode(const MessageId id) const {
    const auto tables = text_tables(rom_);
    const auto group =
        static_cast<std::uint8_t>((id >> 8u) + 4u);
    const auto index = static_cast<std::uint8_t>(id & 0xffu);
    std::vector<std::uint8_t> bytes;
    decompress(
        rom_,
        tables,
        group,
        index,
        0,
        false,
        bytes);

    DecodedMessage message{
        .id = id,
        .original_bytes = bytes,
    };
    std::string page;
    for (std::size_t cursor = 0; cursor < bytes.size(); ++cursor) {
        const auto byte = bytes[cursor];
        if (byte == 0) {
            break;
        }
        std::uint8_t parameter{};
        if (byte >= 6 && byte < 0x10) {
            if (cursor + 1 >= bytes.size()) {
                throw std::runtime_error{
                    "text command is missing its parameter"};
            }
            parameter = bytes[++cursor];
        }
        const auto atom = decode_atom(byte, parameter);
        message.atoms.push_back(atom);
        if (atom.kind == TextAtomKind::option) {
            ++message.option_count;
        }
        if (atom.kind == TextAtomKind::stop) {
            message.pages.push_back(page);
            page.clear();
        } else {
            append_atom_text(page, atom);
        }
    }
    if (!page.empty() || message.pages.empty()) {
        message.pages.push_back(page);
    }
    return message;
}

}  // namespace oracle::content
