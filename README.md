## `fix-html`: Fix HTML like a browser does

[![License: BSD 3 Clause](https://img.shields.io/badge/License-BSD_3--Clause-yellow.svg)](https://opensource.org/licenses/BSD-3-Clause)

`fix-html` is a Linux CLI tool that reads HTML input and produces clean, well-formed
HTML5 output.

### Why?
Browsers are remarkably forgiving of malformed HTML, but most HTML parsers in programming
ecosystems like Python or Go are not. They reject broken input outright, making data
extraction from real-world HTML unnecessarily difficult. `fix-html` bridges this gap: feed it
broken HTML from a file or STDIN, and get back valid HTML5 on STDOUT. There is also an option
to modify a file in-place.

### How it works
The input is processed in 3 stages:
1. The entire input is read into memory, with invalid or disallowed UTF-8 sequences replaced
by `�` (U+FFFD);
2. The Gumbo parser builds a DOM tree from the cleaned input, tolerating a wide range of
markup errors;
3. The DOM tree is serialised into proper HTML5 with correct escaping and structure.

### Limitations
- Only syntax errors are corrected — the tool does not validate semantics or content;
- Input must be UTF-8 encoded; no conversion is performed;
- Some corrections may lose data. The guiding principle is that original and processed
documents should render identically in a browser.
- The parser sometimes inserts new tags as required by the HTML5 standard (see example below)
to make sure the output is always a complete HTML5 document. Depending on the particular
situation this may or may not be desirable.

### Invocation
```console
▶ fix-html --help
Usage:
  fix-html [HTML-FILE]
  fix-html -i HTML-FILE
  fix-html [-hv]

Fix broken HTML files.

Options:
  -i         Modify input file in-place.
  -h/--help  Show this help and exit.
  -v         Show version and exit.
```

#### Example
```console
▶ fix-html <<< '<p>Broken <b>bold <i>italic</b> text'
<!DOCTYPE html>
<html><head></head><body><p>Broken <b>bold <i>italic</i></b><i> text
</i></p></body></html>
```

### Setup
#### Prerequisites
- C11 compiler (GCC or Clang) on Linux
- [Gumbo parser](https://codeberg.org/grisha/gumbo-parser) library (`sudo apt install libgumbo-dev`)
- GNU Make

#### Installation
Run `make` from the project root, then either `sudo make install` or copy the `fix-html` executable
to any suitable location. There is also an example Dockerfile that builds a fully static executable
for use in containers.

### License
BSD-3-Clause
