# Markdown-like formatter for terminal

Format and colorize text in the terminal using simple markdown-like tags. The formatter translates tags into ANSI escape sequences and works as a streaming processor, making it suitable for pipelines, debugging, and scripting.

Formatting state is stored as a stack of bitmask toggles, allowing easy nesting and XOR-based enabling/disabling of styles.

## Features

* Much more readable than raw ANSI escapes
  (`^[[1;31mSTUFF^[[0m` vs `{*r--STUFF--}`)
* Markdown-like, mnemonic syntax
* Works well for `printf` debugging and shell scripts
* Can be used directly from the command line (no recompilation needed)
* Stack-based formatting state with `{<style>--` … `--}`
* Automatic sanitization at EOF (disable with `--no-sanitize`)
* XOR-based style toggles (repeat a style to disable it)
* Stream editor — safe for pipelines and interactive input
* Optional trimming of leading/trailing whitespace
* C-like escape sequences with `-e`
* Strip mode (`-s`) to remove formatting while preserving raw text

![demo](https://i.imgur.com/mc4RorK.png)

## Installation

### Pre-built binary (recommended)

Download the latest binary for your platform from [Releases](https://github.com/T3sT3ro/easy-stream-formatter/releases):

```bash
# Download (replace VERSION and PLATFORM as needed)
curl -LO https://github.com/T3sT3ro/easy-stream-formatter/releases/download/vVERSION/formatter-VERSION-linux-x86_64

# Make executable and move to PATH
chmod +x formatter-*
sudo mv formatter-* /usr/local/bin/formatter
```

### Build from source

Requires a C++ compiler with C++20 support and `make`:

```bash
git clone https://github.com/T3sT3ro/easy-stream-formatter.git
cd easy-stream-formatter
make install clean
```

This installs to `/usr/local/bin`. Aliasing to `f` is recommended for convenience.

You can then run:

```bash
formatter --demo
formatter --legend
```

## Basic usage

```bash
# Help
formatter -h

# Read from STDIN
echo "{*yR--bold yellow on bright red--}" | formatter

# Or via arguments (beware shell splitting)
formatter "{/R--italic red--}" "{_--underlined--}"

# Nest styles and toggle in the middle
formatter "{*--bold {r--RED {*--not bold--}--}--}"

# Colors: [FG, BG]; use ';' to keep current FG
formatter "
{*g--green FG--}
{rg--red FG, green BG--}
{;g--green BG only--}
{r--red FG only {;b--blue BG--} red FG only--}
"

# Trim padding
formatter "{#--    trimmed text    --}"

# Strip formatting
formatter -s "{*g--formatted text--}"

# Enable C-like escapes
formatter -e "{r--1--}\t{y--2--}\n"

# Disable EOF sanitization
formatter -S "{r--unclosed red" > file.txt
```

### Strip mode

Option `-s` removes *valid* formatting while leaving plain text intact. This is useful when you want formatted terminal output and clean file output:

```bash
cat file.in | tee >(formatter -s > file.out) | formatter
```

## Syntax legend

* Styles are toggled (XOR)
* Colors may appear at most twice: first = FG, second = BG
* `;` refers to the current color on the stack
* Capital letters mean bright variants

Supported styles and colors are listed via:

    $ formatter -l
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃ ┏━[ANSI format]━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ ┃
    ┃ ┃ '{<format>--text--}' e.g. {%Yc*_--foo--} ┃ ┃
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

(Terminals may not support all ANSI features.)

## How it works

1. Input is processed greedily using a simple state machine
2. Opening tags push a format (colors + style bitmask) onto a stack
3. Style flags are XOR-ed with the current state
4. ANSI escapes for the new state are emitted immediately
5. Closing tags pop the last format and update the state
6. Unbalanced tags are printed verbatim

The formatter never errors out—it either applies formatting or prints the input as-is.

## Example

```text
{r*--
  red bold
  {;g~--
    red on green, bold, striked
    {~--
      strike toggled off
    --}
  --}
--}
```

Style resolution (XOR-based):

| Tag      | Stack state      | Resulting style             |
| -------- | ---------------- | --------------------------- |
| `{r*--`  | `[r,*]`          | red, bold                   |
| `{~;g--` | `[r,*][~,;g]`    | red on green, bold, striked |
| `{~--`   | `[r,*][~,;g][~]` | strike disabled             |
| `--}`    | `[r,*][~,;g]`    | strike enabled again        |
| `--}`    | `[r,*]`          | red, bold                   |

## Notes

* Styles propagate through the stack until toggled off
* RESET (`0`) and TRIM (`#`) do not propagate
* As a stream editor, the formatter does not wait for balanced brackets
* Buffering can affect interactivity (`stdbuf -i0 -o0 formatter` may help)

## TODO

* Custom tag delimiters
* Decouple parser from character set
* Improve trim behavior
* AWK-based v2 for comparison
* Alternative syntax variants

## Known issues

Some terminals may delay coloring blank lines when large inputs are redirected. This appears to be a terminal-side issue.

![laggy terminal](https://i.imgur.com/3W8XCJh.png)

> **Note**
> Repository extracted from the previous monorepo using `git-filter-repo`
