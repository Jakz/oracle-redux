#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "oracle/core/campaign.h"

namespace oracle::content {

struct RomMetadata {
    core::Campaign campaign{};
    std::string title;
    std::string game_code;
    std::uint16_t global_checksum{};
    std::uint64_t compatibility_fingerprint{};
};

// A validated, exact US Oracle cartridge image supplied by the player.
class RomSource {
public:
    [[nodiscard]] static RomSource load(
        const std::filesystem::path& path);

    [[nodiscard]] const RomMetadata& metadata() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept;
    [[nodiscard]] std::uint8_t read_byte(std::size_t offset) const;
    [[nodiscard]] std::uint16_t read_little_u16(std::size_t offset) const;
    [[nodiscard]] std::size_t banked_file_offset(
        std::uint8_t bank,
        std::uint16_t address) const;

private:
    RomSource(std::vector<std::uint8_t> bytes, RomMetadata metadata);

    std::vector<std::uint8_t> bytes_;
    RomMetadata metadata_;
};

}  // namespace oracle::content
