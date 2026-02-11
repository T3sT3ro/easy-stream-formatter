// syntax.h - Tag syntax constants and color/style mappings
#pragma once

#include <cstdint>
#include <string_view>

// Color codes (compatible with ANSI color offsets)
enum Color : uint8_t {
    BLACK   = 0,
    RED     = 1,
    GREEN   = 2,
    YELLOW  = 3,
    BLUE    = 4,
    MAGENTA = 5,
    CYAN    = 6,
    WHITE   = 7,
    DEFAULT = 9,  // terminal's default color
    CURRENT = 10, // inherit from stack
};

// Format specifier character mappings
constexpr std::string_view COLOR_CHARS_LOWER = "krgybmcw"; // 0-7
constexpr std::string_view COLOR_CHARS_UPPER = "KRGYBMCW"; // bright variants

// Tag syntax constants
namespace syntax {

constexpr char ESCAPE_CHAR   = '\\';
constexpr char TRIM_ESCAPE   = '#';
constexpr char COLOR_CURRENT = ';';
constexpr char COLOR_DEFAULT = 'd';
constexpr char RESET_CHAR    = '0';

// Style characters and their bit positions
namespace style {

// Characters used in format specifiers
constexpr char REVERSED         = '%';
constexpr char BLINK            = '!';
constexpr char BOLD             = '*';
constexpr char ITALIC           = '/';
constexpr char UNDERLINE        = '_';
constexpr char OVERLINE         = '^';
constexpr char DOUBLE_UNDERLINE = '=';
constexpr char STRIKETHROUGH    = '~';
constexpr char DIM              = '.';

// Bit positions for style_bits() - used for XOR toggling
constexpr uint16_t REVERSED_BIT         = 1 << 0;
constexpr uint16_t BLINK_BIT            = 1 << 1;
constexpr uint16_t BOLD_BIT             = 1 << 2;
constexpr uint16_t ITALIC_BIT           = 1 << 3;
constexpr uint16_t UNDERLINE_BIT        = 1 << 4;
constexpr uint16_t OVERLINE_BIT         = 1 << 5;
constexpr uint16_t DOUBLE_UNDERLINE_BIT = 1 << 6;
constexpr uint16_t STRIKETHROUGH_BIT    = 1 << 7;
constexpr uint16_t DIM_BIT              = 1 << 8;

} // namespace style
} // namespace syntax
