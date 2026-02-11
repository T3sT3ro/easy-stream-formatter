// format.h - Format representation for text styling
#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include "ansi.h"
#include "syntax.h"

// Format state using bitfields for compact representation
struct Format {
    // Colors (5 bits each: 4 for color + 1 for bright)
    uint8_t fg_color : 4  = DEFAULT;
    uint8_t fg_bright : 1 = 0;
    uint8_t bg_color : 4  = DEFAULT;
    uint8_t bg_bright : 1 = 0;

    // Style flags (1 bit each)
    uint8_t reversed : 1         = 0;
    uint8_t blink : 1            = 0;
    uint8_t bold : 1             = 0;
    uint8_t italic : 1           = 0;
    uint8_t underline : 1        = 0;
    uint8_t overline : 1         = 0;
    uint8_t double_underline : 1 = 0;
    uint8_t strikethrough : 1    = 0;
    uint8_t dim : 1              = 0;

    // Control flags
    uint8_t reset : 1 = 0;
    uint8_t valid : 1 = 0;

    // Factory: create initial/reset format
    static Format initial() {
        Format f;
        f.fg_color = DEFAULT;
        f.bg_color = DEFAULT;
        f.reset    = 1;
        f.valid    = 1;
        return f;
    }

    // Factory: create empty format (inherits colors from stack)
    static Format empty() {
        Format f;
        f.fg_color = CURRENT;
        f.bg_color = CURRENT;
        f.valid    = 1;
        return f;
    }

    void set_fg(Color c, bool bright = false) {
        fg_color  = c;
        fg_bright = bright;
    }

    void set_bg(Color c, bool bright = false) {
        bg_color  = c;
        bg_bright = bright;
    }

    // Get style flags as bitmask for XOR operations
    uint16_t style_bits() const {
        using namespace syntax::style;
        return (reversed << 0) | (blink << 1) | (bold << 2) | (italic << 3) 
             | (underline << 4) | (overline << 5) | (double_underline << 6) 
             | (strikethrough << 7) | (dim << 8);
    }

    // Set style flags from bitmask
    void set_style_bits(uint16_t bits) {
        reversed         = (bits >> 0) & 1;
        blink            = (bits >> 1) & 1;
        bold             = (bits >> 2) & 1;
        italic           = (bits >> 3) & 1;
        underline        = (bits >> 4) & 1;
        overline         = (bits >> 5) & 1;
        double_underline = (bits >> 6) & 1;
        strikethrough    = (bits >> 7) & 1;
        dim              = (bits >> 8) & 1;
    }

    // Convert to ANSI escape sequence
    std::string to_ansi() const {
        assert(valid && reset);

        std::string result;
        result.reserve(ansi::MAX_SEQ_LEN);
        result = ansi::ESC_START;
        result += std::to_string(ansi::RESET);

        auto append_sgr = [&result](ansi::SGR code) {
            result += ansi::SEP;
            result += std::to_string(code);
        };

        if (bold)             append_sgr(ansi::BOLD);
        if (dim)              append_sgr(ansi::DIM);
        if (italic)           append_sgr(ansi::ITALIC);
        if (underline)        append_sgr(ansi::UNDERLINE);
        if (blink)            append_sgr(ansi::BLINK);
        if (reversed)         append_sgr(ansi::REVERSED);
        if (strikethrough)    append_sgr(ansi::STRIKETHROUGH);
        if (double_underline) append_sgr(ansi::DOUBLE_UNDERLINE);
        if (overline)         append_sgr(ansi::OVERLINE);

        // FG color: 30-37 or 90-97 (bright)
        result += ansi::SEP;
        result += std::to_string(ansi::FG_BASE + fg_color + (fg_bright ? ansi::BRIGHT_OFFSET : 0));

        // BG color: 40-47 or 100-107 (bright)
        result += ansi::SEP;
        result += std::to_string(ansi::BG_BASE + bg_color + (bg_bright ? ansi::BRIGHT_OFFSET : 0));

        result += ansi::ESC_END;
        return result;
    }
};
