#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "oracle/content/rom_text.h"

namespace oracle::ui {

struct DialogueOption {
    std::uint8_t original_index{};
    std::string label;
};

struct DialogueModel {
    content::MessageId message{};
    std::string page_text;
    std::size_t page_index{};
    std::size_t page_count{};
    std::vector<DialogueOption> options;
    std::size_t selected_option{};
    bool visible{};
};

}  // namespace oracle::ui
