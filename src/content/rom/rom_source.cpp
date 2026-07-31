#include "oracle/content/rom_source.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace oracle::content {
namespace {

constexpr std::size_t oracle_us_rom_size = 1024 * 1024;
constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ull;
constexpr std::uint64_t fnv_prime = 1099511628211ull;
constexpr std::uint64_t ages_us_fingerprint = 0x1c14c1c5fcbbaeaaull;
constexpr std::uint64_t seasons_us_fingerprint = 0xf392466e826b6405ull;

constexpr std::array<std::uint8_t, 48> nintendo_logo{
    0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d,
    0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99,
    0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
    0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
};

std::string ascii_field(
    const std::span<const std::uint8_t> bytes,
    const std::size_t begin,
    const std::size_t end) {
    std::string value;
    for (std::size_t offset = begin; offset < end; ++offset) {
        const auto character = bytes[offset];
        if (character == 0 || character == ' ') {
            break;
        }
        value.push_back(static_cast<char>(character));
    }
    return value;
}

std::uint64_t fingerprint(
    const std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t value = fnv_offset_basis;
    for (const auto byte : bytes) {
        value ^= byte;
        value *= fnv_prime;
    }
    return value;
}

std::uint8_t compute_header_checksum(
    const std::span<const std::uint8_t> bytes) noexcept {
    std::uint8_t checksum = 0;
    for (std::size_t offset = 0x134; offset < 0x14d; ++offset) {
        checksum = static_cast<std::uint8_t>(checksum - bytes[offset] - 1);
    }
    return checksum;
}

std::uint16_t compute_global_checksum(
    const std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t checksum = 0;
    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        if (offset != 0x14e && offset != 0x14f) {
            checksum += bytes[offset];
        }
    }
    return static_cast<std::uint16_t>(checksum);
}

}  // namespace

RomSource RomSource::load(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{
            "could not open ROM source: " + path.string()};
    }

    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
    if (bytes.size() != oracle_us_rom_size) {
        throw std::runtime_error{
            "unsupported ROM size; provide an exact 1 MiB US Oracle ROM"};
    }
    const std::span<const std::uint8_t> view{bytes};
    if (!std::equal(
            nintendo_logo.begin(),
            nintendo_logo.end(),
            view.begin() + 0x104)) {
        throw std::runtime_error{"ROM source has an invalid Nintendo logo"};
    }
    if (compute_header_checksum(view) != view[0x14d]) {
        throw std::runtime_error{"ROM source has an invalid header checksum"};
    }

    const auto stored_global_checksum = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(view[0x14e]) << 8u) | view[0x14f]);
    if (compute_global_checksum(view) != stored_global_checksum) {
        throw std::runtime_error{"ROM source has an invalid global checksum"};
    }

    const auto game_code = ascii_field(view, 0x13f, 0x143);
    const auto rom_fingerprint = fingerprint(view);
    core::Campaign campaign{};
    std::uint64_t expected_fingerprint{};
    if (game_code == "AZ8E") {
        campaign = core::Campaign::ages;
        expected_fingerprint = ages_us_fingerprint;
    } else if (game_code == "AZ7E") {
        campaign = core::Campaign::seasons;
        expected_fingerprint = seasons_us_fingerprint;
    } else {
        throw std::runtime_error{
            "unsupported cartridge; provide a US Oracle of Ages or Seasons ROM"};
    }
    if (rom_fingerprint != expected_fingerprint) {
        throw std::runtime_error{
            "ROM checksum is valid but its compatibility fingerprint is not "
            "the supported original US revision"};
    }

    RomMetadata metadata{
        .campaign = campaign,
        .title = ascii_field(view, 0x134, 0x13f),
        .game_code = game_code,
        .global_checksum = stored_global_checksum,
        .compatibility_fingerprint = rom_fingerprint,
    };
    return RomSource{std::move(bytes), std::move(metadata)};
}

RomSource::RomSource(
    std::vector<std::uint8_t> bytes,
    RomMetadata metadata)
    : bytes_{std::move(bytes)}, metadata_{std::move(metadata)} {}

const RomMetadata& RomSource::metadata() const noexcept {
    return metadata_;
}

std::span<const std::uint8_t> RomSource::bytes() const noexcept {
    return bytes_;
}

std::uint8_t RomSource::read_byte(const std::size_t offset) const {
    if (offset >= bytes_.size()) {
        throw std::out_of_range{"ROM read exceeds source size"};
    }
    return bytes_[offset];
}

std::uint16_t RomSource::read_little_u16(const std::size_t offset) const {
    if (offset + 1 >= bytes_.size()) {
        throw std::out_of_range{"ROM word read exceeds source size"};
    }
    return static_cast<std::uint16_t>(
        bytes_[offset] |
        (static_cast<std::uint16_t>(bytes_[offset + 1]) << 8u));
}

std::size_t RomSource::banked_file_offset(
    const std::uint8_t bank,
    const std::uint16_t address) const {
    std::size_t offset{};
    if (bank == 0 && address < 0x4000) {
        offset = address;
    } else if (bank != 0 && address >= 0x4000 && address < 0x8000) {
        offset =
            static_cast<std::size_t>(bank) * 0x4000 + (address & 0x3fff);
    } else {
        throw std::out_of_range{"invalid banked ROM address"};
    }
    if (offset >= bytes_.size()) {
        throw std::out_of_range{"banked ROM address exceeds source size"};
    }
    return offset;
}

}  // namespace oracle::content
