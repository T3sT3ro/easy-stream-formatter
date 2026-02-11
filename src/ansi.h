// ansi.h - ANSI escape sequence constants and SGR codes
#pragma once

#include <cstddef>
#include <string_view>

namespace ansi {

// Base values for color codes
constexpr int FG_BASE       = 30;
constexpr int BG_BASE       = 40;
constexpr int BRIGHT_OFFSET = 60;

// SGR (Select Graphic Rendition) codes
enum SGR : int {
    RESET            = 0,
    BOLD             = 1,
    DIM              = 2,
    ITALIC           = 3,
    UNDERLINE        = 4,
    BLINK            = 6,
    REVERSED         = 7,
    STRIKETHROUGH    = 9,
    DOUBLE_UNDERLINE = 21,
    OVERLINE         = 53,
};

// Escape sequence delimiters
constexpr std::string_view ESC_START = "\e[";
constexpr char             ESC_END   = 'm';
constexpr char             SEP       = ';';

// Max ANSI sequence length: "\e[" + ~12 codes×3 chars + "m" ≈ 48 bytes
constexpr size_t MAX_SEQ_LEN = 48;

} // namespace ansi
