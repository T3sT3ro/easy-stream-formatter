// formatter.cpp - Markdown-like terminal formatter
// Homepage: @HOMEPAGE
// Version: @SVERSION

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <stack>
#include <string>
#include <string_view>
#include <unistd.h>

// ============================================================================
// Constants
// ============================================================================

namespace ansi {
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

    constexpr std::string_view ESC_START = "\e[";
    constexpr char             ESC_END   = 'm';
    constexpr char             SEP       = ';';

    // Max ANSI sequence: "\e[" + ~12 codes×3 chars + "m" ≈ 48 bytes
    constexpr size_t MAX_SEQ_LEN = 48;
} // namespace ansi

// Tag syntax constants
namespace syntax {
    constexpr char             TAG_OPEN      = '{';
    constexpr char             TAG_CLOSE     = '}';
    constexpr char             DELIM_CHAR    = '-';
    constexpr std::string_view DELIM_START   = "--";  // after format spec
    constexpr std::string_view DELIM_END     = "--}"; // closing tag
    constexpr char             ESCAPE_CHAR   = '\\';
    constexpr char             TRIM_ESCAPE   = '#';
    constexpr char             COLOR_CURRENT = ';';
    constexpr char             COLOR_DEFAULT = 'd';
    constexpr char             RESET_CHAR    = '0';

    // Style characters and their bit positions
    namespace style {
        constexpr char REVERSED         = '%';
        constexpr char BLINK            = '!';
        constexpr char BOLD             = '*';
        constexpr char ITALIC           = '/';
        constexpr char UNDERLINE        = '_';
        constexpr char OVERLINE         = '^';
        constexpr char DOUBLE_UNDERLINE = '=';
        constexpr char STRIKETHROUGH    = '~';
        constexpr char DIM              = '.';

        // Bit positions for style_bits()
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

enum Color : uint8_t {
    BLACK   = 0,
    RED     = 1,
    GREEN   = 2,
    YELLOW  = 3,
    BLUE    = 4,
    MAGENTA = 5,
    CYAN    = 6,
    WHITE   = 7,
    DEFAULT = 9, // terminal's default color
    CURRENT = 10, // inherit from stack
};

// Format specifier character mappings
constexpr std::string_view COLOR_CHARS_LOWER = "krgybmcw"; // 0-7
constexpr std::string_view COLOR_CHARS_UPPER = "KRGYBMCW"; // bright variants

// ============================================================================
// Format representation using bitfields
// ============================================================================

struct Format {
    // Colors (5 bits each: 4 for color + 1 for bright)
    uint8_t fg_color : 4  = DEFAULT;
    uint8_t fg_bright : 1 = 0;
    uint8_t bg_color : 4  = DEFAULT;
    uint8_t bg_bright : 1 = 0;

    // Style flags
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

    // Create initial/reset format
    static Format initial() {
        Format f;
        f.fg_color = DEFAULT;
        f.bg_color = DEFAULT;
        f.reset    = 1;
        f.valid    = 1;
        return f;
    }

    // Create empty format (inherits colors)
    static Format empty() {
        Format f;
        f.fg_color = CURRENT;
        f.bg_color = CURRENT;
        f.valid    = 1;
        return f;
    }

    // Set foreground color
    void set_fg(Color c, bool bright = false) {
        fg_color  = c;
        fg_bright = bright;
    }

    // Set background color
    void set_bg(Color c, bool bright = false) {
        bg_color  = c;
        bg_bright = bright;
    }

    // Get style flags as bitmask for XOR operations
    uint16_t style_bits() const {
        return (reversed << 0) | (blink << 1) | (bold << 2) | (italic << 3) | (underline << 4) | (overline << 5)
             | (double_underline << 6) | (strikethrough << 7) | (dim << 8);
    }

    // Set style flags from bitmask
    void set_style_bits(const uint16_t bits) {
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

// ============================================================================
// Documentation strings
// ============================================================================

const char* USAGE = R"-(
Usage: %s [options] [strings...]
)-";

const char* HELP = R"-(
Markdown-like Formatter by Tooster, @SVERSION
Homepage: @HOMEPAGE

Description:
    Translates liquid-like tags '{<format>--' and '--}' into ANSI formatting.
    When no arguments are passed, input is read from STDIN. Otherwise, each
    argument is translated separately. Remember to quote arguments when they
    contain spaces or special characters.

Options:
    -v --version            print version string
    -l --legend             show formatting legend
    -s --strip              strip formatting tags from input
    -e --escape             enable C-like escape sequences (\a\b\r\n\f\t\v\#)
    -S --no-sanitize        do not insert format reset on EOF
       --demo               show demo
    -h --help               display this help and exit

Overview:
    Formatting uses a stack, but tags do not have to be balanced. The formatter
    is greedy: whenever a tag is encountered, it is immediately translated
    into ANSI escapes. As a result, missing closing tags may leave formatting
    active in the terminal.

    The formatter follows a "best-effort" approach: it parses whatever it can,
    never crashes, and prints unrecognized or invalid formatting verbatim.

    This tool is designed for use in pipelines and in programs that do not
    want to depend on formatting libraries. Existing code only needs to
    modify printed strings to include '{<format>--' and '--}' tags.

    Strip mode is useful when you want formatted output in the terminal while
    preserving unformatted text in files. Note that tags are currently
    hardcoded, which may conflict with inputs containing literal '{--' or
    '--}'. Future versions may allow custom tag definitions.

Escape sequences (-e mode):
    Standard C escapes: \\ \a \b \r \n \f \t \v
    Whitespace trim:    \# (greedily consumes following whitespace)

    The \# escape provides bounded-memory whitespace trimming. It consumes
    all whitespace characters immediately following it until the first
    non-whitespace character is encountered.

    Example: "Hello\#     World" → "HelloWorld"

Formatting and examples:
    A compact legend describing all formats and rules is available via '-l'.

    (Below, 'f' is used as an alias for 'formatter'.)

    To set the foreground color, use a single color letter. Use uppercase for
    the bright variant:
     $ f "{r--red--} {R--BRIGHT RED--}"

    The first color sets the foreground, the second sets the background:
     $ f "{RB*--BRIGHT RED on BRIGHT BLUE and bold--}"
    They do not have to be adjacent:
     $ f "{R*_B--this also works--}"

    To set only the background color, use ';' (current color) as foreground:
     $ f "{r--red text {;y--on yellow--} and more red--}"

    The ';' (current) color refers to the topmost color on the stack.
    The 'd' (default) color refers to the terminal's default color, which may
    differ from white and sometimes equals BRIGHT WHITE ('W').

    Formatting options behave differently than colors. They are XOR-ed against
    the current state and therefore toggle on repeated use. Conceptually,
    odd occurrences enable a style, even occurrences disable it:
     $ f "{/*--bold, italic {*--bold toggled off--} bold and italic again--}"

    Trim whitespace (requires -e):
     $ f -e "no padding:\#    <trimmed>\#    :end"

Remarks:
    - Some terminals may not support bright colors (ANSI codes >= 90).
    - Use non-standard styles (blink, overline, double underline,
      strikethrough) with care; terminal support varies.
    - Programs in pipelines use system-default buffering, which may cause
      interactive output to appear frozen. Tools like 'stdbuf' or 'unbuffer'
      can help.
    - The primary use case is piping and printf-style debugging; argument-based
      usage exists for convenience. Always quote arguments containing spaces.
)-";

const char* LEGEND = R"-(
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ ┏━[ANSI format]━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ┃
┃ ┃  {<format>--text--}  e.g. {%Yc*_--foo--} ┃ ┃
┃ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
┃ ┏━[Colors]━━━━━┳━[Options]━━━━━━━━━━━━━━━━━┓ ┃
┃ ┃  blac[k]     ┃  [Color]{0,2}             ┃1┃
┃ ┃  [r]ed       ┃  [%] Reversed             ┃ ┃
┃ ┃  [g]reen     ┃  [!] Blink                ┃2┃
┃ ┃  [y]ellow    ┃  [*] Bold                 ┃ ┃
┃ ┃  [b]lue      ┃  [/] Italic               ┃ ┃
┃ ┃  [m]agenta   ┃  [_] Underline            ┃ ┃
┃ ┃  [c]yan      ┃  [^] Overline             ┃2┃
┃ ┃  [w]hite     ┃  [=] Double underline     ┃2┃
┃ ┃  [;] current ┃  [~] Strikethrough        ┃2┃
┃ ┃  [d] default ┃  [.] Dim                  ┃ ┃
┃ ┃              ┃  [0] Reset all formatting ┃ ┃
┃ ┃  CAPS=BRIGHT ┃                           ┃ ┃
┃ ┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
┃ ┏━[Escapes (-e mode)]━━━━━━━━━━━━━━━━━━━━━━┓ ┃
┃ ┃  \# Trim following whitespace (greedy)   ┃ ┃
┃ ┃  \\ \a \b \r \n \f \t \v (C escapes)     ┃ ┃
┃ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
┃ ┏━[Control]━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ┃
┃1┃  1st encountered color is fg, 2nd is bg  ┃ ┃
┃1┃  Current color [;] is top color on stack ┃ ┃
┃1┃  Capitalize fg/bg for brighter colors    ┃ ┃
┃1┃  Default color [d] is terminal's default ┃ ┃
┃ ┃  Options are toggled (XORed) with last   ┃ ┃
┃ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
┃ ┏━[Remarks]━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ┃
┃ ┃  Formats are stored on a stack           ┃ ┃
┃ ┃  It's a greedy stream processor:         ┃ ┃
┃ ┃   - doesn't wait for balanced bracket    ┃ ┃
┃ ┃   - doesn't crash on invalid format      ┃ ┃
┃2┃  Terminals may lack support for some ops ┃ ┃
┃ ┃  Buffering may break interactiveness     ┃ ┃
┃ ┃   - `stdbuf -i0 -o0 formatter` may help  ┃ ┃
┃ ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
)-";

const char* DEMO = R"-(
┃ {kw--blac[k]     FG--} ┃ {Kw--blac[K]     LFG--} ┃ {;k--blac[k]     BG--} ┃ {;K--blac[K]     LBG--} ┃
┃ {r;--[r]ed       FG--} ┃ {R;--[R]ed       LFG--} ┃ {kr--[r]ed       BG--} ┃ {kR--[R]ed       LBG--} ┃
┃ {g;--[g]reen     FG--} ┃ {G;--[G]reen     LFG--} ┃ {kg--[g]reen     BG--} ┃ {kG--[G]reen     LBG--} ┃
┃ {y;--[y]ellow    FG--} ┃ {Y;--[Y]ellow    LFG--} ┃ {ky--[y]ellow    BG--} ┃ {kY--[Y]ellow    LBG--} ┃
┃ {b;--[b]lue      FG--} ┃ {B;--[B]lue      LFG--} ┃ {kb--[b]lue      BG--} ┃ {kB--[B]lue      LBG--} ┃
┃ {m;--[m]agenta   FG--} ┃ {M;--[M]agenta   LFG--} ┃ {km--[m]agenta   BG--} ┃ {kM--[M]agenta   LBG--} ┃
┃ {c;--[c]yan      FG--} ┃ {C;--[C]yan      LFG--} ┃ {kc--[c]yan      BG--} ┃ {kC--[C]yan      LBG--} ┃
┃ {w;--[w]hite     FG--} ┃ {W;--[W]hite     LFG--} ┃ {kw--[w]hite     BG--} ┃ {kW--[W]hite     LBG--} ┃

┃ {rk--red context       {;w*--[white BG with current FG color and bold]--}      red context--} ┃
┃ {rk--red context       {w;*--[current BG with white FG color and bold]--}      red context--} ┃

┃ {ry--red context       {dd--[default FG+BG color]--}       red context--} ┃

┃ [^] {^--overline                   --} ┃
┃ [_] {_--Underline                  --} ┃
┃ [*] {*--Bold                       --} ┃
┃ [/] {/--Italic                     --} ┃
┃ [.] {.--dim                        --} ┃
┃ [%] {%--reversed                   --} ┃
┃ [!] {!--blink                      --} ┃
┃ [=] {=--double underline           --} ┃
┃ [~] {~--Strikethrough              --} ┃
┃ [0] {0--reset all formatting       --} ┃

┃ {--common formatting stack test
┃ {--normal {*--bold {/--italic {_--underline {.--dim {%--reverse {~--crossed {!--blink-BLNK--}-CRS--}-RST--}-DIM--}-UND--}-ITA--}-BLD--}-NORM--}
┃ {_--+under{_---under{_--+under--}--}--}

┃ SOME UTF-8:
┃ 你好，世界
  {%--你好，{*--世界--}--}
┃ ∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i), ∀x∈ ℝ : ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β)
  {r--∮ E⋅da = Q,  {b--n → ∞--}, ∑ f(i) = ∏ g(i), {g--∀x∈ ℝ :--} ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = {y--¬({r--¬α ∨ β--})--}--}

┃ -- BELOW IS ONLY RELEVANT WITH -e OPTION --
┃ escape sequences test. If you see [X] then escape doesn't work - it should be [O] ┃
┃ IF YOU SEE THIS MESSAGE, THIS SECTION IS IRRELEVANT\r
┃ \\\\ bracket escape: \\
┃ \\n newline escape \n^^^^^^^^^^^^^^^^^^^
┃ \\a alert escape (will be heard on supported terminals) \a
┃ \\b backspace escape (moves cursor back 1 char): [X\bO]
┃ \\r: [X] carriage return test (moves cursor back to col 0)\r┃ \\r: [O]
┃ \\f: form feed \\\f\ should be continous diagonal line spanning 2 lines
┃ \\t: horizontal tab: a\tb\tc\td
┃ \\v: vertical tab \\\v\ should be continous diagnoal line spannig 2 lines
┃ \\# trim test: [\#          trimmed          \#]
)-";

// ============================================================================
// Formatter State Machine
// ============================================================================

class FormatterAutomaton {
    const bool strip_; // strip formatting instead of emitting ANSI
    const bool escape_; // enable escape sequences
    const bool sanitize_; // emit reset on destruction

    enum class State {
        DEFAULT,
        PARSE_ESCAPE,
        PARSE_OPENING_BRACKET,
        SKIP_WHITESPACE, // for \# escape
    };

    State state_ = State::DEFAULT;
    std::string buffer_;
    std::stack<Format> format_stack_;
    Format bracket_format_  = Format::empty();
    int parsed_colors_      = 0;
    uint16_t parsed_styles_ = 0; // style flags already set in current bracket

    void emit_ansi(const std::string& ansi) const {
        if (!strip_) {
            fputs(ansi.c_str(), stdout);
        }
    }

    static void emit_char(int c) {
        putchar(c);
    }

    void buffer_char(int c) {
        buffer_ += static_cast<char>(c);
    }

    void flush_buffer() {
        if (!buffer_.empty()) {
            fputs(buffer_.c_str(), stdout);
            buffer_.clear();
        }
    }

    void clear_buffer() {
        buffer_.clear();
    }

    // Push format onto stack and return ANSI sequence
    std::string push_format(const Format& mask) {
        assert(mask.valid);

        Format format = format_stack_.top();

        // Apply reset if requested
        if (mask.reset) {
            format = Format::initial();
        }

        // XOR style bits
        format.set_style_bits(format.style_bits() ^ mask.style_bits());

        // Override colors if not CURRENT
        if (mask.fg_color != CURRENT) {
            format.fg_color  = mask.fg_color;
            format.fg_bright = mask.fg_bright;
        }
        if (mask.bg_color != CURRENT) {
            format.bg_color  = mask.bg_color;
            format.bg_bright = mask.bg_bright;
        }

        format_stack_.push(format);
        return format.to_ansi();
    }

    // Pop format from stack and return ANSI sequence
    std::string pop_format() {
        if (format_stack_.size() > 1) {
            format_stack_.pop();
        }
        return format_stack_.top().to_ansi();
    }

    void finish_bracket_parse(bool success) {
        if (success) {
            clear_buffer();
        } else {
            flush_buffer();
        }
        parsed_colors_  = 0;
        parsed_styles_  = 0;
        bracket_format_ = Format::empty();
        state_          = State::DEFAULT;
    }

    // Try to parse a color character, returns true if recognized
    bool try_parse_color(int c) {
        Color color;
        bool bright = false;

        // Check lowercase colors
        auto pos = COLOR_CHARS_LOWER.find(static_cast<char>(c));
        if (pos != std::string_view::npos) {
            color = static_cast<Color>(pos);
        }
        // Check uppercase (bright) colors
        else if ((pos = COLOR_CHARS_UPPER.find(static_cast<char>(c))) != std::string_view::npos) {
            color  = static_cast<Color>(pos);
            bright = true;
        }
        // Check special colors
        else if (c == syntax::COLOR_DEFAULT) {
            color = DEFAULT;
        } else if (c == syntax::COLOR_CURRENT) {
            color = CURRENT;
        } else {
            return false;
        }

        if (parsed_colors_ >= 2) {
            return false; // too many colors
        }

        if (parsed_colors_ == 0) {
            bracket_format_.set_fg(color, bright);
        } else {
            bracket_format_.set_bg(color, bright);
        }
        parsed_colors_++;
        return true;
    }

    // Try to parse a style character, returns true if recognized
    bool try_parse_style(int c) {
        using namespace syntax::style;
        uint16_t style_bit = 0;

        switch (c) {
        case REVERSED:         style_bit = REVERSED_BIT;         break;
        case BLINK:            style_bit = BLINK_BIT;            break;
        case BOLD:             style_bit = BOLD_BIT;             break;
        case ITALIC:           style_bit = ITALIC_BIT;           break;
        case UNDERLINE:        style_bit = UNDERLINE_BIT;        break;
        case OVERLINE:         style_bit = OVERLINE_BIT;         break;
        case DOUBLE_UNDERLINE: style_bit = DOUBLE_UNDERLINE_BIT; break;
        case STRIKETHROUGH:    style_bit = STRIKETHROUGH_BIT;    break;
        case DIM:              style_bit = DIM_BIT;              break;
        case syntax::RESET_CHAR:
            bracket_format_.reset = 1;
            return true;
        default:
            return false;
        }

        // Check for duplicate style in same bracket
        if (parsed_styles_ & style_bit) {
            return false;
        }

        parsed_styles_ |= style_bit;
        bracket_format_.set_style_bits(bracket_format_.style_bits() | style_bit);
        return true;
    }

    // Check if buffer ends with given suffix
    bool buffer_ends_with(std::string_view suffix) const {
        if (buffer_.size() < suffix.size()) {
            return false;
        }
        return buffer_.compare(buffer_.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // Remove suffix from buffer
    void buffer_remove_suffix(const size_t len) {
        if (len <= buffer_.size()) {
            buffer_.resize(buffer_.size() - len);
        }
    }

public:
    FormatterAutomaton(bool strip, bool escape, bool sanitize) : strip_(strip), escape_(escape), sanitize_(sanitize) {
        format_stack_.push(Format::initial());
        emit_ansi(format_stack_.top().to_ansi());
    }

    ~FormatterAutomaton() {
        flush_buffer();
        if (sanitize_) {
            emit_ansi(Format::initial().to_ansi());
        }
    }

    void accept(int c) {
        // Handle escape sequences
        if (state_ == State::PARSE_ESCAPE) {
            clear_buffer();
            switch (c) {
            case syntax::ESCAPE_CHAR:
                emit_char(syntax::ESCAPE_CHAR);
                break;
            case 'a':
                emit_char('\a');
                break;
            case 'b':
                emit_char('\b');
                break;
            case 'r':
                emit_char('\r');
                break;
            case 'n':
                emit_char('\n');
                break;
            case 'f':
                emit_char('\f');
                break;
            case 't':
                emit_char('\t');
                break;
            case 'v':
                emit_char('\v');
                break;
            case syntax::TRIM_ESCAPE:
                // Enter whitespace-skipping mode
                state_ = State::SKIP_WHITESPACE;
                return;
            default:
                // Invalid escape - print backslash and continue with current char
                emit_char(syntax::ESCAPE_CHAR);
                buffer_char(c);
                flush_buffer();
                break;
            }
            state_ = State::DEFAULT;
            return;
        }

        // Start escape sequence
        if (escape_ && c == syntax::ESCAPE_CHAR) {
            flush_buffer();
            buffer_char(c);
            state_ = State::PARSE_ESCAPE;
            return;
        }

        // Skip whitespace mode (for \#)
        if (state_ == State::SKIP_WHITESPACE) {
            if (std::isspace(c)) {
                return; // consume whitespace
            }
            // Non-whitespace: exit skip mode and process character
            state_ = State::DEFAULT;
            // Fall through to process this character
        }

        // Start bracket parsing
        if (c == syntax::TAG_OPEN) {
            flush_buffer();
            buffer_char(c);
            bracket_format_ = Format::empty();
            parsed_colors_  = 0;
            parsed_styles_  = 0;
            state_          = State::PARSE_OPENING_BRACKET;
            return;
        }

        // Parse bracket format or terminator
        if (c == syntax::DELIM_CHAR) {
            buffer_char(c);
            if (state_ == State::PARSE_OPENING_BRACKET && buffer_ends_with(syntax::DELIM_START)) {
                // Successfully parsed opening bracket
                emit_ansi(push_format(bracket_format_));
                finish_bracket_parse(true);
                return;
            }
            return;
        }

        // Parse format specifiers inside bracket
        if (state_ == State::PARSE_OPENING_BRACKET) {
            buffer_char(c);
            if (try_parse_color(c) || try_parse_style(c)) {
                return; // valid format char
            }
            // Invalid character in bracket
            finish_bracket_parse(false);
            return;
        }

        // Closing bracket
        if (c == syntax::TAG_CLOSE) {
            buffer_char(c);
            if (buffer_ends_with(syntax::DELIM_END)) {
                if (format_stack_.size() > 1) {
                    buffer_remove_suffix(syntax::DELIM_END.size());
                    flush_buffer();
                    emit_ansi(pop_format());
                    state_ = State::DEFAULT;
                    return;
                }
            }
            flush_buffer();
            state_ = State::DEFAULT;
            return;
        }

        // Normal character
        buffer_char(c);
        flush_buffer();
        state_ = State::DEFAULT;
    }
};

// ============================================================================
// Main
// ============================================================================

static int f_strip       = 0;
static int f_escape      = 0;
static int f_no_sanitize = 0;

static struct option long_options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, 'v'},
    {"legend", no_argument, nullptr, 'l'},
    {"strip", no_argument, &f_strip, 's'},
    {"escape", no_argument, &f_escape, 'e'},
    {"no-sanitize", no_argument, &f_no_sanitize, 'S'},
    {"demo", no_argument, nullptr, 0},
    {nullptr, 0, nullptr, 0},
};

int main(int argc, char* argv[]) {
    int opt;
    int opt_idx;
    FILE* istream = stdin;

    while ((opt = getopt_long(argc, argv, "?hvlseS", long_options, &opt_idx)) != -1) {
        switch (opt) {
        case 0:
            if (std::strcmp(long_options[opt_idx].name, "demo") == 0) {
                istream = fmemopen(const_cast<char*>(DEMO), std::strlen(DEMO), "r");
                break;
            }
            [[fallthrough]];
        case '?':
            std::fprintf(stderr, USAGE + 1, argv[0]);
            std::fprintf(stderr, "(try using -h or --help for more info)\n");
            return EXIT_FAILURE;

        case 'h':
            std::printf(USAGE + 1, argv[0]);
            std::printf("\n%s", HELP + 1);
            return EXIT_SUCCESS;

        case 'v':
            std::puts("@VER");
            return EXIT_SUCCESS;

        case 'l':
            std::printf("%s", LEGEND + 1);
            return EXIT_SUCCESS;

        case 'e':
            f_escape = 1;
            break;
        case 's':
            f_strip = 1;
            break;
        case 'S':
            f_no_sanitize = 1;
            break;
        }
    }

    // Process positional arguments
    if (optind < argc) {
        const char* separator = "";
        while (optind < argc) {
            std::printf("%s", separator);
            FormatterAutomaton automaton(f_strip, f_escape, !f_no_sanitize);
            for (const char* p = argv[optind]; *p; ++p) {
                automaton.accept(*p);
            }
            separator = " ";
            optind++;
        }
    } else {
        // Read from stdin
        FormatterAutomaton automaton(f_strip, f_escape, !f_no_sanitize);
        int c;
        while ((c = std::getc(istream)) != EOF) {
            automaton.accept(c);
        }
    }

    return EXIT_SUCCESS;
}
