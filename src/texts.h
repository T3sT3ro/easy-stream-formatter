// texts.h - Documentation strings for formatter
// These strings use @SVERSION, @VER, and @HOMEPAGE placeholders
// that are substituted during the build process.

#pragma once

namespace texts {

inline const char* USAGE = R"-(
Usage: %s [options] [strings...]
)-";

inline const char* HELP = R"-(
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

inline const char* LEGEND = R"-(
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

inline const char* DEMO = R"-(
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

// Version string (substituted at build time)
inline const char* VERSION = "@VER";

} // namespace texts
