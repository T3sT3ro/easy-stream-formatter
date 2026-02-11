// tag_syntax.h - Configurable tag delimiter syntax
#pragma once

#include <cstring>
#include <memory>
#include <string>
#include <string_view>

// Defines the syntax for opening and closing tags
struct TagSyntax {
    std::string name;
    std::string open_tag;   // Start of opening tag: "{", "[", "<", "@@", etc.
    std::string open_end;   // End of opening tag: "--", "]", ">"
    std::string close_tag;  // Closing tag: "--}", "[/]", "</>"

    TagSyntax() = default;

    TagSyntax(std::string_view n, std::string_view ot, std::string_view oe, std::string_view ct)
        : name(n), open_tag(ot), open_end(oe), close_tag(ct) {}

    // Predefined styles
    static const TagSyntax CLASSIC;
    static const TagSyntax BRACKET;
    static const TagSyntax XML;

    static const TagSyntax* ALL_STYLES[];
    static constexpr size_t NUM_STYLES = 3;

    // Find predefined style by name
    static const TagSyntax* find(std::string_view name) {
        for (size_t i = 0; i < NUM_STYLES; ++i) {
            if (ALL_STYLES[i]->name == name) {
                return ALL_STYLES[i];
            }
        }
        return nullptr;
    }

    // Create custom syntax from 3 arguments
    // All must be non-empty strings
    static std::unique_ptr<TagSyntax> from_args(const char* opening, const char* separator, const char* closing) {
        if (!opening || !separator || !closing) return nullptr;
        if (std::strlen(opening) == 0 || std::strlen(separator) == 0 || std::strlen(closing) == 0) {
            return nullptr;
        }

        auto syntax = std::make_unique<TagSyntax>();
        syntax->name      = "custom";
        syntax->open_tag  = opening;
        syntax->open_end  = separator;
        syntax->close_tag = closing;
        return syntax;
    }
};

// Static predefined styles (defined in tag_syntax.cpp or inline)
inline const TagSyntax TagSyntax::CLASSIC{"classic", "{", "--", "--}"};
inline const TagSyntax TagSyntax::BRACKET{"bracket", "[", "]",  "[/]"};
inline const TagSyntax TagSyntax::XML    {"xml",     "<", ">",  "</>"};

inline const TagSyntax* TagSyntax::ALL_STYLES[] = {
    &TagSyntax::CLASSIC, 
    &TagSyntax::BRACKET, 
    &TagSyntax::XML
};
