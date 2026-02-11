// automaton.h - Formatter state machine for parsing and transforming tagged text
#pragma once

#include <cctype>
#include <cstdio>
#include <stack>
#include <string>

#include "format.h"
#include "syntax.h"
#include "tag_syntax.h"

// State machine that processes input character-by-character,
// transforming format tags into ANSI escape sequences
class FormatterAutomaton {
public:
    FormatterAutomaton(bool strip, bool escape, bool sanitize, const TagSyntax& syntax = TagSyntax::CLASSIC)
        : strip_(strip), escape_(escape), sanitize_(sanitize), syntax_(syntax) {
        format_stack_.push(Format::initial());
        emit_ansi(format_stack_.top().to_ansi());
    }

    ~FormatterAutomaton() {
        flush_buffer();
        if (sanitize_) {
            emit_ansi(Format::initial().to_ansi());
        }
    }

    // Process a single input character
    void accept(int c);

private:
    // Configuration
    const bool strip_;            // strip formatting instead of emitting ANSI
    const bool escape_;           // enable escape sequences
    const bool sanitize_;         // emit reset on destruction
    const TagSyntax& syntax_;     // tag syntax configuration

    // Parser states
    enum class State {
        DEFAULT,
        PARSE_ESCAPE,
        PARSE_OPENING_TAG,     // matching multi-char open_tag
        PARSE_OPENING_BRACKET, // parsing format specifier
        PARSE_CLOSING_TAG,     // matching close tag like [/] or </>
        SKIP_WHITESPACE,       // consuming whitespace after \#
    };

    State state_ = State::DEFAULT;
    std::string buffer_;
    std::stack<Format> format_stack_;
    
    // Current bracket parsing state
    Format bracket_format_  = Format::empty();
    int parsed_colors_      = 0;
    uint16_t parsed_styles_ = 0;

    // Output helpers
    void emit_ansi(const std::string& ansi) const {
        if (!strip_) {
            std::fputs(ansi.c_str(), stdout);
        }
    }

    static void emit_char(int c) {
        std::putchar(c);
    }

    // Buffer management
    void buffer_char(int c) { buffer_ += static_cast<char>(c); }
    void clear_buffer() { buffer_.clear(); }
    
    void flush_buffer() {
        if (!buffer_.empty()) {
            std::fputs(buffer_.c_str(), stdout);
            buffer_.clear();
        }
    }

    bool buffer_ends_with(std::string_view suffix) const {
        if (buffer_.size() < suffix.size()) return false;
        return buffer_.compare(buffer_.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    void buffer_remove_suffix(size_t len) {
        if (len <= buffer_.size()) {
            buffer_.resize(buffer_.size() - len);
        }
    }

    // Check if buffer suffix could be a prefix of the given pattern
    bool could_match_prefix(std::string_view pattern) const {
        for (size_t i = 1; i <= pattern.size() && i <= buffer_.size(); ++i) {
            if (buffer_.substr(buffer_.size() - i) == pattern.substr(0, i)) {
                return true;
            }
        }
        return false;
    }

    // Format stack operations
    std::string push_format(const Format& mask) {
        Format format = format_stack_.top();

        if (mask.reset) {
            format = Format::initial();
        }

        // XOR style bits for toggling
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

    std::string pop_format() {
        if (format_stack_.size() > 1) {
            format_stack_.pop();
        }
        return format_stack_.top().to_ansi();
    }

    // Bracket parsing helpers
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

    bool try_parse_color(int c);
    bool try_parse_style(int c);

    // State handlers
    void handle_escape(int c);
    void handle_skip_whitespace(int c);
    void handle_closing_tag(int c);
    void handle_opening_tag(int c);
    void handle_opening_bracket(int c);
    void handle_default(int c);
};

// Implementation

inline bool FormatterAutomaton::try_parse_color(int c) {
    Color color;
    bool bright = false;

    auto pos = COLOR_CHARS_LOWER.find(static_cast<char>(c));
    if (pos != std::string_view::npos) {
        color = static_cast<Color>(pos);
    } else if ((pos = COLOR_CHARS_UPPER.find(static_cast<char>(c))) != std::string_view::npos) {
        color  = static_cast<Color>(pos);
        bright = true;
    } else if (c == syntax::COLOR_DEFAULT) {
        color = DEFAULT;
    } else if (c == syntax::COLOR_CURRENT) {
        color = CURRENT;
    } else {
        return false;
    }

    if (parsed_colors_ >= 2) return false;

    if (parsed_colors_ == 0) {
        bracket_format_.set_fg(color, bright);
    } else {
        bracket_format_.set_bg(color, bright);
    }
    parsed_colors_++;
    return true;
}

inline bool FormatterAutomaton::try_parse_style(int c) {
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

    // Duplicate style in same bracket is invalid
    if (parsed_styles_ & style_bit) return false;

    parsed_styles_ |= style_bit;
    bracket_format_.set_style_bits(bracket_format_.style_bits() | style_bit);
    return true;
}

inline void FormatterAutomaton::handle_escape(int c) {
    clear_buffer();
    switch (c) {
    case syntax::ESCAPE_CHAR: emit_char(syntax::ESCAPE_CHAR); break;
    case 'a': emit_char('\a'); break;
    case 'b': emit_char('\b'); break;
    case 'r': emit_char('\r'); break;
    case 'n': emit_char('\n'); break;
    case 'f': emit_char('\f'); break;
    case 't': emit_char('\t'); break;
    case 'v': emit_char('\v'); break;
    case syntax::TRIM_ESCAPE:
        state_ = State::SKIP_WHITESPACE;
        return;
    default:
        // Invalid escape - output backslash and current char
        emit_char(syntax::ESCAPE_CHAR);
        buffer_char(c);
        flush_buffer();
        break;
    }
    state_ = State::DEFAULT;
}

inline void FormatterAutomaton::handle_skip_whitespace(int c) {
    if (std::isspace(c)) return; // consume whitespace
    state_ = State::DEFAULT;
    accept(c); // reprocess non-whitespace character
}

inline void FormatterAutomaton::handle_closing_tag(int c) {
    buffer_char(c);
    
    if (buffer_ends_with(syntax_.close_tag)) {
        if (format_stack_.size() > 1) {
            buffer_remove_suffix(syntax_.close_tag.size());
            flush_buffer();
            emit_ansi(pop_format());
            state_ = State::DEFAULT;
            return;
        }
    }
    
    // Still potentially a close tag?
    if (syntax_.close_tag.substr(0, buffer_.size()) == std::string_view(buffer_)) {
        return; // continue matching
    }
    
    // Not a close tag
    flush_buffer();
    state_ = State::DEFAULT;
}

inline void FormatterAutomaton::handle_opening_tag(int c) {
    buffer_char(c);
    
    // Completed open_tag?
    if (buffer_ == syntax_.open_tag) {
        // Special case: if close_tag == open_tag and we're inside formatting,
        // this is a close tag, not an open tag
        if (syntax_.close_tag == syntax_.open_tag && format_stack_.size() > 1) {
            clear_buffer();
            emit_ansi(pop_format());
            state_ = State::DEFAULT;
            return;
        }
        
        bracket_format_ = Format::empty();
        parsed_colors_  = 0;
        parsed_styles_  = 0;
        state_          = State::PARSE_OPENING_BRACKET;
        return;
    }
    
    // Still a partial match?
    if (syntax_.open_tag.substr(0, buffer_.size()) == buffer_) {
        return;
    }
    
    // No longer matches - flush and reset
    flush_buffer();
    state_ = State::DEFAULT;
}

inline void FormatterAutomaton::handle_opening_bracket(int c) {
    buffer_char(c);

    // Check for opening tag completion (e.g., "--" in "{r*--")
    if (buffer_ends_with(syntax_.open_end)) {
        emit_ansi(push_format(bracket_format_));
        finish_bracket_parse(true);
        return;
    }

    // Check for close tag that starts with open_tag (e.g., [/] starts with [)
    if (syntax_.close_tag.size() >= syntax_.open_tag.size() &&
        syntax_.close_tag.substr(0, syntax_.open_tag.size()) == syntax_.open_tag) {
        
        size_t close_prefix_len = std::min(buffer_.size(), syntax_.close_tag.size());
        if (close_prefix_len >= syntax_.open_tag.size() + 1 &&
            buffer_.substr(buffer_.size() - close_prefix_len) == syntax_.close_tag.substr(0, close_prefix_len)) {
            
            state_ = State::PARSE_CLOSING_TAG;
            if (buffer_ends_with(syntax_.close_tag) && format_stack_.size() > 1) {
                buffer_remove_suffix(syntax_.close_tag.size());
                flush_buffer();
                emit_ansi(pop_format());
                state_ = State::DEFAULT;
            }
            return;
        }
    }

    // Could be partial open_end delimiter?
    if (could_match_prefix(syntax_.open_end)) {
        return;
    }

    // Try parsing as format specifier
    if (try_parse_color(c) || try_parse_style(c)) {
        return;
    }

    // Invalid character - abort bracket parse
    finish_bracket_parse(false);
}

inline void FormatterAutomaton::handle_default(int c) {
    // Check for closing tag in default mode
    std::string tentative = buffer_ + static_cast<char>(c);
    if (tentative.size() >= syntax_.close_tag.size() &&
        tentative.substr(tentative.size() - syntax_.close_tag.size()) == syntax_.close_tag) {
        if (format_stack_.size() > 1) {
            buffer_ = tentative.substr(0, tentative.size() - syntax_.close_tag.size());
            flush_buffer();
            emit_ansi(pop_format());
            return;
        }
    }

    // Check for opening tag start
    if (c == syntax_.open_tag[0]) {
        flush_buffer();
        buffer_char(c);
        
        if (syntax_.open_tag.size() == 1) {
            // Special case: if close_tag == open_tag and we're inside formatting,
            // this is a close tag, not an open tag
            if (syntax_.close_tag == syntax_.open_tag && format_stack_.size() > 1) {
                clear_buffer();
                emit_ansi(pop_format());
                return;
            }
            
            bracket_format_ = Format::empty();
            parsed_colors_  = 0;
            parsed_styles_  = 0;
            state_          = State::PARSE_OPENING_BRACKET;
        } else {
            state_ = State::PARSE_OPENING_TAG;
        }
        return;
    }

    // Regular character - add to buffer
    buffer_char(c);
    
    // Check for partial close tag match
    if (buffer_ends_with(syntax_.close_tag)) {
        if (format_stack_.size() > 1) {
            buffer_remove_suffix(syntax_.close_tag.size());
            flush_buffer();
            emit_ansi(pop_format());
            return;
        }
    }
    
    if (could_match_prefix(syntax_.close_tag)) {
        return; // wait for more characters
    }

    flush_buffer();
}

inline void FormatterAutomaton::accept(int c) {
    // Escape sequence handling
    if (state_ == State::PARSE_ESCAPE) {
        handle_escape(c);
        return;
    }

    // Start escape sequence?
    if (escape_ && c == syntax::ESCAPE_CHAR) {
        flush_buffer();
        buffer_char(c);
        state_ = State::PARSE_ESCAPE;
        return;
    }

    // Dispatch to state handler
    switch (state_) {
    case State::SKIP_WHITESPACE:
        handle_skip_whitespace(c);
        break;
    case State::PARSE_CLOSING_TAG:
        handle_closing_tag(c);
        break;
    case State::PARSE_OPENING_TAG:
        handle_opening_tag(c);
        break;
    case State::PARSE_OPENING_BRACKET:
        handle_opening_bracket(c);
        break;
    case State::DEFAULT:
    default:
        handle_default(c);
        break;
    }
}
