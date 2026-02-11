// formatter.cpp - Markdown-like terminal formatter
// Homepage: @HOMEPAGE
// Version: @SVERSION

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <memory>

#include "automaton.h"
#include "tag_syntax.h"
#include "texts.h"

// ============================================================================
// Command-line interface
// ============================================================================

namespace {

int f_strip       = 0;
int f_escape      = 0;
int f_no_sanitize = 0;
const TagSyntax* f_syntax = &TagSyntax::CLASSIC;
std::unique_ptr<TagSyntax> f_custom_syntax;

struct option long_options[] = {
    {"help",        no_argument,       nullptr,        'h'},
    {"version",     no_argument,       nullptr,        'v'},
    {"legend",      no_argument,       nullptr,        'l'},
    {"strip",       no_argument,       &f_strip,       's'},
    {"escape",      no_argument,       &f_escape,      'e'},
    {"no-sanitize", no_argument,       &f_no_sanitize, 'S'},
    {"syntax",      required_argument, nullptr,        'x'},
    {"custom",      no_argument,       nullptr,        'c'},
    {"demo",        no_argument,       nullptr,        0  },
    {nullptr,       0,                 nullptr,        0  },
};

void print_usage(const char* program, FILE* stream) {
    std::fprintf(stream, texts::USAGE + 1, program);
}

int handle_help(const char* program) {
    print_usage(program, stdout);
    std::printf("\n%s", texts::HELP + 1);
    return EXIT_SUCCESS;
}

int handle_version() {
    std::puts(texts::VERSION);
    return EXIT_SUCCESS;
}

int handle_legend() {
    std::printf("%s", texts::LEGEND + 1);
    return EXIT_SUCCESS;
}

int handle_syntax_option(const char* optarg) {
    f_syntax = TagSyntax::find(optarg);
    if (!f_syntax) {
        std::fprintf(stderr, "Unknown syntax: %s\n", optarg);
        std::fprintf(stderr, "Available: classic, bracket, xml (or use -c for custom)\n");
        return EXIT_FAILURE;
    }
    return -1; // continue processing
}

int handle_custom_syntax(int argc, char* argv[]) {
    if (optind + 2 >= argc) {
        std::fprintf(stderr, "Custom syntax requires 3 arguments: OPEN SEP CLOSE\n");
        std::fprintf(stderr, "Example: -c '(' ')' ')'\n");
        return EXIT_FAILURE;
    }
    
    f_custom_syntax = TagSyntax::from_args(argv[optind], argv[optind + 1], argv[optind + 2]);
    if (!f_custom_syntax) {
        std::fprintf(stderr, "Invalid custom syntax: '%s' '%s' '%s'\n",
                     argv[optind], argv[optind + 1], argv[optind + 2]);
        std::fprintf(stderr, "All three arguments must be non-empty strings\n");
        return EXIT_FAILURE;
    }
    
    optind += 3;
    f_syntax = f_custom_syntax.get();
    return -1; // continue processing
}

void process_arguments(int argc, char* argv[]) {
    const char* separator = "";
    while (optind < argc) {
        std::printf("%s", separator);
        FormatterAutomaton automaton(f_strip, f_escape, !f_no_sanitize, *f_syntax);
        for (const char* p = argv[optind]; *p; ++p) {
            automaton.accept(*p);
        }
        separator = " ";
        optind++;
    }
}

void process_stream(FILE* stream) {
    FormatterAutomaton automaton(f_strip, f_escape, !f_no_sanitize, *f_syntax);
    int c;
    while ((c = std::getc(stream)) != EOF) {
        automaton.accept(c);
    }
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int opt;
    int opt_idx;
    FILE* istream = stdin;

    while ((opt = getopt_long(argc, argv, "?hvlseSx:c", long_options, &opt_idx)) != -1) {
        int result;
        
        switch (opt) {
        case 0:
            if (std::strcmp(long_options[opt_idx].name, "demo") == 0) {
                istream = fmemopen(const_cast<char*>(texts::DEMO), std::strlen(texts::DEMO), "r");
                break;
            }
            [[fallthrough]];
            
        case '?':
            print_usage(argv[0], stderr);
            std::fprintf(stderr, "(try using -h or --help for more info)\n");
            return EXIT_FAILURE;

        case 'h': return handle_help(argv[0]);
        case 'v': return handle_version();
        case 'l': return handle_legend();
        case 'e': f_escape = 1;      break;
        case 's': f_strip = 1;       break;
        case 'S': f_no_sanitize = 1; break;
        
        case 'x':
            result = handle_syntax_option(optarg);
            if (result != -1) return result;
            break;
            
        case 'c':
            result = handle_custom_syntax(argc, argv);
            if (result != -1) return result;
            break;
        }
    }

    if (optind < argc) {
        process_arguments(argc, argv);
    } else {
        process_stream(istream);
    }

    return EXIT_SUCCESS;
}
