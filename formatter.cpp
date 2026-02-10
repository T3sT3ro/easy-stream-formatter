#include <getopt.h>  // unistd might not work
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

using namespace std;

const char* USAGE  = R"-(
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
    -v --version            print version string (since v1.4)
    -l --legend             show formatting legend
    -s --strip              strip formatting tags from input
    -e --escape             enable C-like escape sequences (\a\b\r\n\f\t\v)
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

    The '#' (trim) option removes leading and trailing whitespace inside the
    matched balanced pair:
     $ f "{#--   <no padding before {r--red--} and none after>    --}"

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
┃ ┃              ┃  [#] Trim text paddings   ┃ ┃
┃ ┃  CAPS=BRIGHT ┃  [0] Reset all formatting ┃ ┃
┃ ┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━┛ ┃
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
┃-{#--
      [#] trim text paddings         
                            --}----------┃

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
)-";


/*
format bitmask:     ┃ valid|0|# ┃ .|~|=|^|_|/|*|!|% ┃          ┃ light bg|bg ┃ light fg|fg ┃
bit widths:     MSB ┃   1  |1|1 ┃ 1|1|1|1|1|1|1|1|1 ┃          ┃    1    | 4 ┃    1    | 4 ┃ LSB
                    ┃  ctrl(3b) ┃    fmt(9b)        ┃ pad(10b) ┃     bg(5b)  ┃     fg(5b)  ┃
*/

typedef __uint32_t mask_t;

enum Masks {
    //-COLOR-MASKS----------
    FG_LIGHT = 0x10 << 0,            // mask light bit of fg
    FG_COLOR = 0x0f << 0,            // mask for color code of fg
    FG_MASK  = FG_LIGHT | FG_COLOR,  // mask for whole FG of
    BG_LIGHT = FG_LIGHT << 5,
    BG_COLOR = FG_COLOR << 5,
    BG_MASK  = BG_LIGHT | BG_COLOR,  // mask for all BG

    BLACK   = 0,  // ANSI color base +0
    RED     = 1,
    GREEN   = 2,
    YELLOW  = 3,
    BLUE    = 4,
    MAGENTA = 5,
    CYAN    = 6,
    WHITE   = 7,

    //-COLOR-SPECIAL--------
    DEFAULT_COLOR = 9,   // 0b1001  default terminal color: ANSI color base +9
    CURRENT_COLOR = 10,  // 0b1010  means that last used color should be used.

    //-FORMAT---------------
    FORMAT_MASK = 0x1ff << 20,

    REVERSED         = 1 << 20,
    BLINK            = 1 << 21,
    BOLD             = 1 << 22,
    ITALIC           = 1 << 23,
    UNDERLINE        = 1 << 24,
    OVERLINE         = 1 << 25,
    DOUBLE_UNDERLINE = 1 << 26,
    STRIKETHROUGH    = 1 << 27,
    DIM              = 1 << 28,

    //-CONTROL--------------
    TRIM  = 1 << 29,
    RESET = 1 << 30,
    VALID = 1 << 31,

    //-SPECIAL-MASKS--------
    INITIAL_FORMAT_MASK = VALID | RESET | DEFAULT_COLOR << 5 | DEFAULT_COLOR,
    EMPTY_FORMAT_MASK   = VALID | CURRENT_COLOR << 5 | CURRENT_COLOR,
};

const string formatChars =
    "krgybmcw"      // 0-7
    "-"             // 8
    "d;"            // 9-10
    "KRGYBMCW"      // 11-18
    "%!*/_^=~.#0";  // 19-29

#define GET_FG(mask) (((mask)&FG_COLOR) >> 0)                                             // returns FG on LSBits
#define GET_BG(mask) (((mask)&BG_COLOR) >> 5)                                             // returns BG on LSBits
#define LIGHTER(color) ((color) | (1 << 4))                                               // returns lighter color mask
#define MASK_TO_FG_ANSI(mask) (GET_FG(mask) + 30 + ((mask)&FG_LIGHT ? 60 : 0))            // returns ANSI code for fg color
#define MASK_TO_BG_ANSI(mask) (GET_BG(mask) + 40 + ((mask)&BG_LIGHT ? 60 : 0))            // returns ANSI code for bg color
#define OVERRIDE(target, submask, source) ((target) & ~(submask) | (source) & (submask))  // returns mask with submask from other
// returns mask with set primary(part=0) or secondary(part!=0) color
inline mask_t WITH_COLOR(mask_t mask, int color, int part) {
    return OVERRIDE(mask, (part == 0 ? FG_MASK : BG_MASK), color | color << 5);
}

class FormatterAutomaton {
    const bool strip    = false;    // should strip instead of printing ANSI
    const bool escape   = false;    // should escape special characters
    const bool sanitize = true;     // should print format reset in destructor

    enum STATE {
        DEFAULT_STATE,
        PARSE_ESCAPE_STATE,
        PARSE_OPENING_BRACKET_STATE,
        SKIP_LEADING_PADDING_STATE,
    } state = DEFAULT_STATE;

    string        store            = "";               // stores current buffered input if processing potential parts
    stack<mask_t> formatStack      = stack<mask_t>();  // mask always describes active formatting (absolute format)
    mask_t        bracketMask      = EMPTY_FORMAT_MASK;
    int           parsedColorParts = 0;

#pragma region innards

    void printANSI(string ANSI) {
        if (!strip)
            printf("%s", ANSI.c_str());
    }

    // converts absolute format mask to ANSI string that can be printed
    string formatToAnsi(mask_t format) {
        assert(format & VALID && format & RESET);

        vector<int> codes;
        // ANSI VALUES
        if (format & RESET) codes.push_back(0);
        if (format & BOLD) codes.push_back(1);
        if (format & DIM) codes.push_back(2);
        if (format & ITALIC) codes.push_back(3);
        if (format & UNDERLINE) codes.push_back(4);
        if (format & BLINK) codes.push_back(6);
        if (format & REVERSED) codes.push_back(7);
        if (format & STRIKETHROUGH) codes.push_back(9);
        if (format & DOUBLE_UNDERLINE) codes.push_back(21);
        if (format & OVERLINE) codes.push_back(53);
        codes.push_back(MASK_TO_FG_ANSI(format));
        codes.push_back(MASK_TO_BG_ANSI(format));
        string ANSI = "";
        for (int code : codes) ANSI += ";" + to_string(code);
        assert(ANSI.size() > 0);
        return "\e[" + ANSI.substr(1) + "m";
    }

    // pushes format on stack and returns built ANSI escape sequence
    string pushFormat(mask_t mask) {
        assert(mask & VALID);

        mask_t format = formatStack.top();

        // 1. calculate the absolute format
        if (mask & RESET) format = INITIAL_FORMAT_MASK;                               // RESET to base off default mask
        format = OVERRIDE(format, TRIM, mask);                                        // trim doesn't propagate through stack
        format ^= mask & FORMAT_MASK;                                                 // toggle the formatting specified in `mask`
        if (GET_FG(mask) != CURRENT_COLOR) format = OVERRIDE(format, FG_MASK, mask);  // override fg color from mask
        if (GET_BG(mask) != CURRENT_COLOR) format = OVERRIDE(format, BG_MASK, mask);  // override bg color from mask

        // 2. store absolute format
        formatStack.push(format);

        // 3. build ansi sequence from absolute mask
        return formatToAnsi(format);
    }

    // pops format from stack and returns built ANSI escape sequence
    string popFormat() {
        if (formatStack.size() > 1) formatStack.pop();
        return formatToAnsi(formatStack.top());  // restore previous format
    }

    inline void storeChar(int c) { store += c; }
    inline void clearStore() { store = ""; }
    inline void flushStore() {
        if (!store.empty()) printf("%s", store.c_str());
        clearStore();
    }

    void cleanAfterBracketParse(bool parseSuccess) {
        parseSuccess ? clearStore() : flushStore();
        parsedColorParts = 0;
        state            = (parseSuccess && (bracketMask & TRIM)) ? SKIP_LEADING_PADDING_STATE : DEFAULT_STATE;
        bracketMask      = EMPTY_FORMAT_MASK;
    }

#pragma endregion

   public:
    // strip: should formatting be parsed to ANSI or stripped off
    FormatterAutomaton(bool strip, bool escape, bool sanitize) 
        : strip(strip), escape(escape), sanitize(sanitize) {
        formatStack.push(INITIAL_FORMAT_MASK);
        printANSI(formatToAnsi(formatStack.top()));
    }

    ~FormatterAutomaton() {
        flushStore();
        if(sanitize)
            printANSI(formatToAnsi(INITIAL_FORMAT_MASK));
    }

    void accept(int c) {
        static size_t found = string::npos;


        // parsing escape
        if (state == PARSE_ESCAPE_STATE) {
            switch (c) {
                case '\\': clearStore(); printf("\\"); break; // backslash
                case 'a' : clearStore(); printf("\a"); break; // alert (bell)
                case 'b' : clearStore(); printf("\b"); break; // backspace
                case 'r' : clearStore(); printf("\r"); break; // carraiage return
                case 'n' : clearStore(); printf("\n"); break; // newline (line feed)
                case 'f' : clearStore(); printf("\f"); break; // form feed
                case 't' : clearStore(); printf("\t"); break; // horizontal tab
                case 'v' : clearStore(); printf("\v"); break; // vertical tab
                default:                                      // invalid escape - print as is
                    storeChar(c);
                    flushStore();
                    break;
            }
            state = DEFAULT_STATE;
        } else if (escape == true && c == '\\') {
            flushStore();
            storeChar(c);  // store = "\"
            state = PARSE_ESCAPE_STATE;
        }


        // potential whitespace trimming - increases memory in TRIM mode
        else if (isspace(c)) {
            if (state != SKIP_LEADING_PADDING_STATE || strip)  // skip whitespace if trim mode
                storeChar(c);

            if (!formatStack.top() & TRIM)  // if trimming mode is not currently enabled, we can safely print the whitespace
                flushStore();
        }


        // begin bracket parsing
        else if (c == '{') {
            flushStore();
            storeChar(c);
            bracketMask = EMPTY_FORMAT_MASK;
            state       = PARSE_OPENING_BRACKET_STATE;
        }


        // parsking end of opening bracket
        else if (c == '-') {
            storeChar(c);
            if (state == PARSE_OPENING_BRACKET_STATE) {
                static regex terminatorRegex("--$");
                if (regex_search(store, terminatorRegex)) {  // success parsing bracket
                    // deal with empty format {--
                    printANSI(pushFormat(bracketMask));
                    return cleanAfterBracketParse(true);
                }
            } else {
                state = DEFAULT_STATE;  // to exit eventual trailing whitespace removal mode
            }
        }


        // parsing options of opening bracket; '-' won't be caught anymore so it's good
        else if (state == PARSE_OPENING_BRACKET_STATE && (found = formatChars.find(c)) != string::npos) {
            storeChar(c);

            // dealing with color
            if (found <= 18) {               // a ';' can be passed here
                if (parsedColorParts < 2) {  // fg, bg not set
                    bracketMask = WITH_COLOR(bracketMask, isupper(c) ? LIGHTER(found - 11) : found, parsedColorParts);
                    ++parsedColorParts;
                } else {  // too much color parts
                    return cleanAfterBracketParse(false);
                }

                // dealing with symbols
            } else {
                mask_t opMask;
                switch (c) {  // todo check if valid
                    case '%': opMask = REVERSED; break;
                    case '!': opMask = BLINK; break;
                    case '*': opMask = BOLD; break;
                    case '/': opMask = ITALIC; break;
                    case '_': opMask = UNDERLINE; break;
                    case '^': opMask = OVERLINE; break;
                    case '=': opMask = DOUBLE_UNDERLINE; break;
                    case '~': opMask = STRIKETHROUGH; break;
                    case '.': opMask = DIM; break;
                    case '#': opMask = TRIM; break;
                    case '0': opMask = RESET; break;
                }
                if (bracketMask & opMask)  // operator was already used
                    return cleanAfterBracketParse(false);
                else
                    bracketMask |= opMask;
            }
        }


        // end the formatting. trippy: {--}
        else if (c == '}') {
            storeChar(c);
            static regex closingBracket(R"(--}$)");
            static regex closingPaddingBracket(R"(\s*--}$)");
            if (regex_search(store, closingBracket)) {
                if (formatStack.size() > 1)  // don't truncate unbalanced pairs
                    store = regex_replace(store,
                                          (formatStack.top() & TRIM) && !strip
                                              ? closingPaddingBracket
                                              : closingBracket,
                                          "");

                flushStore();

                printANSI(popFormat());
                state = DEFAULT_STATE;

            } else {  // some giberrish } in text, continue
                flushStore();
                state = DEFAULT_STATE;
            }
        }


        // any normal characters or breaking current context
        else {
            storeChar(c);
            flushStore();
            state = DEFAULT_STATE;
        }
    }
};

// pushing new formatting on stack introduces ANSI entry sequence
// popping formatting from stack:
//   1. outputs RESET sequence
//   2. introduces stack's top ANSI entry sequence
// thanks to that we can always reset on exit sequences

static int f_strip;
static int f_escape;
static int f_no_sanitize;

static struct option longOptions[] = {
    {"help",        no_argument, NULL,              'h'},
    {"version",     no_argument, NULL,              'v'},
    {"legend",      no_argument, NULL,              'l'},
    {"strip",       no_argument, &f_strip,          's'},
    {"escape",      no_argument, &f_escape,         'e'},
    {"no-sanitize", no_argument, &f_no_sanitize,    'S'},
    {"demo"       , no_argument, NULL,               0 },
    {NULL,          0,           NULL,               0 },
};

int main(int argc, char* argv[]) {

    int opt;    // returned char
    int optIdx; // index in long_options of parsed option
    
    FILE* istream = stdin;
    // parse all options
    // https://azrael.digipen.edu/~mmead/www/Courses/CS180/getopt.html
    while ((opt = getopt_long(argc, argv, "?hvlseS", longOptions, &optIdx)) != -1) {
        switch (opt) {
            case 0:
                if (strcmp(longOptions[optIdx].name, "demo") == 0) {
                    istream = fmemopen((void*)DEMO, strlen(DEMO), "r");   
                    break;
                } else goto unrecognizedLong;
            case 'h': 
                printf(USAGE+1, argv[0]);
                printf("\n%s", HELP + 1);
                exit(EXIT_SUCCESS);
            case 'v': puts("@VER");                             exit(EXIT_SUCCESS);
            case 'l': printf("%s", LEGEND + 1);                 exit(EXIT_SUCCESS);
            case 'e': f_escape = 1; break;
            case 's': f_strip = 1; break;
            case 'S': f_no_sanitize = 1; break;
            case '?': 
                unrecognizedLong:
                fprintf(stderr, USAGE + 1, argv[0]);
                fprintf(stderr, "(try using -h or --help for more info)\n");
                exit(EXIT_FAILURE);
        }
    }

    // read from positional arguments
    if(optind < argc) {
        while (optind < argc) {
            static string separator = "";  // to print arguments separated with spaces
            printf("%s", separator.c_str());

            // parse the argument
            FormatterAutomaton automaton = FormatterAutomaton(f_strip, f_escape, !f_no_sanitize);
            for (char* it = argv[optind]; *it; ++it)  // read null terminated c_string
                automaton.accept(*it);

            separator = " ";
            optind++;
        }
    // read from STDIN
    } else {

        FormatterAutomaton automaton = FormatterAutomaton(f_strip, f_escape, !f_no_sanitize);

        int c;
        while ((c = getc(istream)) != EOF)
            automaton.accept(c);
    }

    exit(EXIT_SUCCESS);
}
