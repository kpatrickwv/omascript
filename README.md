# Omascript

Write screenplays in plain Markdown and watch them become a properly formatted script as you type.

Omascript is a fork of [Omawrite](https://github.com/omacom/omawrite), the dead-simple Markdown writing app that ships with Omarchy. You write on the left. On the right, the same text appears as industry-standard screenplay pages: Courier 12pt, standard margins and indents, page numbers, (MORE) and (CONT'D) across page breaks. Export a PDF or print straight from the app.

Nothing new to learn: headings, bold, italics, and blockquotes are all the syntax there is.

```sh
./bin/build
./build/omascript examples/the-last-signal.md
```

---

## Contents

1. [Install](#install)
2. [Using Omascript](#using-omascript)
3. [Screenplay style guide](#screenplay-style-guide)
4. [What the formatter does for you](#what-the-formatter-does-for-you)
5. [Command line](#command-line)
6. [Shortcuts](#shortcuts)
7. [Page format specifications](#page-format-specifications)
8. [Troubleshooting](#troubleshooting)
9. [Known limitations](#known-limitations)
10. [Building and testing](#building-and-testing)
11. [Credits and licenses](#credits-and-licenses)

---

## Install

On Arch Linux or Omarchy, install `base-devel` for the packaging tools, then from this folder:

```sh
./bin/install
```

This builds the app and installs it through pacman as the `omascript` package, with a desktop entry and icon. It may ask for your password.

Omascript is a separate app from Omawrite, with its own name, settings, and draft recovery, so the two install and run side by side.

To try it without installing:

```sh
./bin/build
./build/omascript
```

---

## Using Omascript

- **Write on the left.** It's the Omawrite editor you know, with the same auto-save, draft recovery, find and replace, and text sizing.
- **Read on the right.** The formatted script redraws a moment after you stop typing. It scrolls on its own to keep the line you're writing in view, and a small marker in the page margin shows exactly which formatted line the caret is on. Scroll it with the mouse wheel to look elsewhere.
- **Count pages, not just words.** The footer shows both. At this format, one page is about one minute of screen time.
- **Export or print.** `Ctrl+E` (or the export button in the footer) writes a PDF next to your file, with the same name, and replaces any earlier export. An unsaved document asks where to put it. `Ctrl+P` prints the formatted script.
- **Hide the preview** with `Ctrl+Shift+P`, or the split-pane button in the footer, when you want the full-width writing view. Omascript remembers the choice.

**Enter knows screenplay flow.** Enter normally starts a new paragraph, with a blank line, as in Omawrite. After a character's name or a parenthetical, it starts the next line directly underneath instead, because dialogue belongs right under the name. After a transition (`> CUT TO:`), it starts a fresh paragraph rather than continuing the quote. `Shift+Enter` always inserts a single line break.

The `examples` folder has a sample scene (`the-last-signal.md`) and two templates to copy: `template-film.md` for features and `template-tv.md` for a one-hour episode with a teaser and four acts.

---

## Screenplay style guide

### The one rule that matters most

**Separate elements with blank lines.** A blank line is how Omascript knows one thing has ended and the next has begun. When in doubt, add a blank line.

### Quick reference

| You write | You get |
|---|---|
| `## INT. KITCHEN - NIGHT` | Scene heading |
| `### ANGLE ON THE DOOR` | Shot / sub-heading |
| A plain paragraph | Action |
| `**MARGARET**` on its own line | Character cue |
| `**MARGARET** (V.O.)` | Character cue with extension |
| `*(whispering)*` under a cue | Parenthetical |
| Text under a cue | Dialogue |
| `**MARGARET:** Hello.` | Cue and dialogue on one line |
| `> CUT TO:` | Transition (right-aligned) |
| `# ACT ONE` | Centered heading |
| `---` on its own line | Page break |
| `<!-- note -->` or `%%note%%` | Private note (never printed) |

---

### Title page and options

Put a front matter block at the very top of the file, between two `---` lines. This is the same format Obsidian and most Markdown tools use for properties.

```markdown
---
title: The Last Signal
author: Your Name
credit: Written by
source: Based on the short story by Someone Else
contact: |
  Your Name
  you@example.com
  (304) 555-0100
draft: First Draft
date: October 2026
---
```

| Key | Where it prints | Notes |
|---|---|---|
| `title` | Centered, about a third of the way down | Uppercased automatically. Required for a title page to appear. A list prints each item on its own line, e.g. show name and episode title. |
| `author` | Centered, under the credit line | Also accepts `authors`, `writer`, `writers`, `written by`. Multiple authors are joined with "&". |
| `credit` | Centered, between title and author | Defaults to "Written by". Use "Teleplay by", "Story by", etc. |
| `source` | Centered, below the author | Also accepts `based on`. |
| `contact` | Bottom left | Use `\|` and indented lines for multiple lines. |
| `draft`, `date` | Bottom right | Either or both. |

The `|` after `contact:` means "keep these line breaks." The lines under it must be indented by at least one space.

For multiple values, either form works:

```markdown
author: [Jane Doe, John Smith]
```

```markdown
author:
  - Jane Doe
  - John Smith
```

Front matter can also switch on formatting options for the script, so they apply in the preview, the PDF, and printing alike:

| Key | Effect |
|---|---|
| `scene numbers: true` | Numbers scenes in both margins, as shooting scripts do. |
| `contd: true` | Adds (CONT'D) when a character speaks again after action, within the same scene. |
| `title page: false` | Leaves out the title page even though there's a title. |

If there's no `title`, no title page is printed.

---

### Scene headings

Use a level-two heading:

```markdown
## INT. RADIO SHACK - NIGHT
```

You don't have to type in capitals. `## int. radio shack - night` comes out the same.

**Shortcut:** a paragraph that starts with `INT.`, `EXT.`, `INT./EXT.`, `EXT./INT.`, `I/E`, or `EST.` is recognized as a scene heading even without the `##`.

**Style notes**

- Format is `INT.` or `EXT.`, then the location, then a dash, then the time of day: `INT. PICKUP TRUCK - CONTINUOUS`.
- Keep locations consistent across the script. "MARGARET'S KITCHEN" in scene 3 and "KITCHEN - MARGARET'S HOUSE" in scene 12 look like two different places to a production team.
- Sub-locations go after the main location with a dash: `INT. COMPANY STORE - BACK ROOM - DAY`.

---

### Action

Just write paragraphs. Lines within a paragraph are joined together, the same way Markdown normally works. To start a new action paragraph, leave a blank line.

If you need a line break *inside* a paragraph (for the lines of a sign, say), end the line with two spaces or a backslash:

```markdown
The sign reads:\
CASH ONLY\
NO SCRIP
```

On-screen text that gives context, like a location or date card, is written as action. Screenplays call it a SUPER:

```markdown
SUPER: "Morgantown, West Virginia. 1972."
```

**Style notes**

- Write in present tense. "She opens the door," not "She opened the door."
- Keep paragraphs short: three or four lines on the page at most.
- Capitalize a character's name the first time they appear, with a brief description: `MARGARET HALE (60s), wiry, in a canvas coat two sizes too big`. Type the capitals yourself here, since only you know which mention is the first.
- Only write what can be seen or heard.

---

### Characters and dialogue

Put the character's name in bold on its own line, then the dialogue directly underneath, with no blank line between them:

```markdown
**MARGARET**
That's not possible.
```

The dialogue continues until the next blank line. Several lines of dialogue are joined into one speech. Names don't need to be in capitals.

#### Extensions

Add the extension after the bold name, or inside the bold. Both work:

```markdown
**OLD TOM** (V.O.)
**OLD TOM (V.O.)**
```

| Extension | Meaning |
|---|---|
| `(V.O.)` | Voice-over: the character isn't physically in the scene (narration, a voice on the radio, a recording) |
| `(O.S.)` | Off-screen: the character is in the scene but not visible |
| `(O.C.)` | Off-camera: the TV equivalent of O.S. |
| `(CONT'D)` | Continued: the same character keeps talking after an interruption. `contd: true` in the front matter adds these for you. |

#### Parentheticals

A line wrapped entirely in parentheses, under a character's name, becomes a parenthetical. The italics are optional:

```markdown
**MARGARET**
*(whispering)*
Did you hear that?
(beat)
There it is again.
```

**Style notes:** use parentheticals sparingly, keep them to a few lowercase words, and never end a speech with one.

#### Play-style dialogue

```markdown
**MARGARET:** That's not possible.

**TOM:** *(into the handset)* Is that you?
```

The colon is what makes this dialogue, so it can go inside or outside the bold. Leave a blank line between play-style lines; without one, the next line is read as more of the same character's dialogue.

#### What counts as a character name

So that bold action text isn't mistaken for a character, a bold line is only a character cue if the name is 38 characters or fewer and five words or fewer, doesn't contain `!` or `?`, and doesn't end with a period or comma. So `**BANG!**` on its own line stays bold action, and `**DR. SMITH**` is a character. A bold name with nothing underneath it is also treated as action.

---

### Transitions

Use a blockquote:

```markdown
> CUT TO:
```

**Shortcut:** an all-capitals line ending in `TO:` is recognized automatically, as are `FADE OUT.`, `FADE TO BLACK.`, `CUT TO BLACK.`, `SMASH CUT`, and `THE END`.

`FADE IN:` stays on the left, where it belongs. Type it as a normal line at the start of your script.

Modern scripts use transitions sparingly. A new scene heading already implies a cut, so save them for when the *kind* of cut matters (`SMASH CUT TO:`, `MATCH CUT TO:`, `DISSOLVE TO:`).

---

### Shots, acts, and page breaks

```markdown
### CLOSE ON MARGARET'S HAND
```

```markdown
---

# ACT TWO
```

`###` (through `######`) makes a shot. `#` makes a centered heading for acts and `THE END`. A line with just `---` (or `***`, `___`, `===`) forces a new page; it needs a blank line above it.

---

### Notes, styling, and escapes

Anything in `<!-- -->` or Obsidian's `%% %%` never prints, and can span lines.

| You write | You get |
|---|---|
| `**bold**` or `__bold__` | **bold** |
| `*italic*` or `_italic_` | *italic* |
| `***bold italic***` | ***bold italic*** |
| `<u>underline</u>` | underline |

Start a line with a backslash to force it to be plain action: `\INT. is short for interior.` To print a literal `*`, `_`, `#`, `<`, `>`, `%`, or `\`, put a backslash in front of it.

---

## What the formatter does for you

- **Capitalization** of scene headings, shots, character names, transitions, and centered headings.
- **Margins, indents, and word wrap** at industry-standard positions and widths.
- **Page numbers** top right, from page 2, in the standard `2.` style.
- **(MORE) and (CONT'D)** when a speech runs over a page break, breaking at the end of a sentence where possible and never right after a parenthetical.
- **No orphans.** A scene heading never sits alone at the bottom of a page, a character name always stays with its dialogue, action splits at a sentence with at least two lines on each page, and a transition is never stranded at the top of a page.

---

## Command line

`omascript` opens an empty document and `omascript FILE` opens a Markdown file. A file that doesn't exist yet starts a new document that your first save creates.

`--pdf` formats a script without opening a window, which is handy for scripts and batch exports:

```sh
omascript --pdf pilot.md
omascript --pdf pilot.md -o "Pilot - Draft 2.pdf" --scene-numbers --contd
```

| Option | What it does |
|---|---|
| `--pdf FILE` | Export FILE to PDF and exit. Writes FILE's name with `.pdf` unless `-o` says otherwise. |
| `-o FILE`, `--output FILE` | Where `--pdf` writes the PDF. |
| `--scene-numbers` | Number scenes in both margins. |
| `--contd` | Add (CONT'D) when a character speaks again after action. |
| `--no-title-page` | Leave out the title page. |
| `-h`, `--help` | Show help. |
| `-v`, `--version` | Show the version. |

Options set in a script's front matter win over these flags.

---

## Shortcuts

New in Omascript:

- `Ctrl+E` exports the formatted script as a PDF.
- `Ctrl+P` prints the formatted script.
- `Ctrl+Shift+P` shows or hides the script preview.

Carried over from Omawrite:

- `Ctrl+S` saves; `Ctrl+Shift+S` saves as; `Ctrl+O` opens; `Ctrl+N` opens a new window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` undo and redo.
- `Ctrl+F` finds (`Enter`/`Ctrl+G` next, `Shift+Enter` previous); `Ctrl+H` finds and replaces.
- `Ctrl+B` and `Ctrl+I` toggle bold and italic. `Ctrl+K` inserts a link; `Ctrl+Shift+X` toggles strikethrough.
- `Ctrl+=`, `Ctrl+-`, `Ctrl+0`, and `Ctrl+scroll` change the editor's text size.
- `Ctrl+Shift+C` copies the document as HTML.
- `Super+F` or `F11` toggles fullscreen.
- `Ctrl+?`, `Ctrl+/`, or `F1` shows the shortcut reference.

Saved documents are auto-saved a moment after you stop typing. Untitled drafts are never auto-saved, and unsaved drafts are recovered after a crash.

---

## Page format specifications

US Letter, Courier Prime 12pt (10 characters per inch, 6 lines per inch), 54 lines per page.

| Element | Left edge | Width | Blank lines before |
|---|---|---|---|
| Scene heading | 1.5" | 60 characters | 1 |
| Action | 1.5" | 60 characters | 1 |
| Shot | 1.5" | 60 characters | 1 |
| Character | 3.7" | 38 characters | 1 |
| Parenthetical | 3.1" | 25 characters | 0 |
| Dialogue | 2.5" | 35 characters | 0 |
| Transition | right-aligned to 7.5" | 60 characters | 1 |
| Centered | centered between 1.5" and 7.5" | 60 characters | 1 |

Margins: 1.5" left, 1" right, 1" top, 1" bottom. Page numbers sit 0.5" from the top, right-aligned to 7.5". Scene numbers, when on, sit at 1.25" and 7.6".

---

## Troubleshooting

**Something came out as the wrong element.** Look at it in the preview. The most common cause is a missing blank line: action written directly under dialogue, with no blank line between, is read as more dialogue.

**My character name came out as bold action.** Check the [character name rules](#what-counts-as-a-character-name).

**A normal sentence turned into a scene heading.** It starts with "Int" or "Est" followed by a space or period. Start the line with a backslash: `\Est. 1890, the town...`

**A line of action turned into a transition.** All-capitals lines ending in `TO:` are transitions. Start the line with a backslash to keep it as action.

---

## Known limitations

Omascript 0.1 covers film and TV screenplays only. Not yet supported:

- Dual dialogue (two characters speaking at once, side by side)
- Final Draft (.fdx) export
- A4 paper
- Revision marks and locked page numbers
- Audio drama and stage play formats
- Clicking in the preview to jump to that place in the text

---

## Building and testing

Install Qt 6 (`qt6-base` and `qt6-declarative`), a C++17 compiler such as `gcc`, and `make`. The build uses `qmake6` (or `qmake`) and the tests use QtTest, both part of `qt6-base`.

```sh
./bin/build
./bin/test
```

`bin/test` runs Omawrite's test suite plus Omascript's screenplay tests with the offscreen platform, so it doesn't need a graphical session.

The screenplay code lives in its own files, which keeps merging future Omawrite updates manageable:

- `src/screenplay.h` and `src/screenplay.cpp`: parsing, layout, pagination, drawing, PDF export, and the `--pdf` command.
- `src/screenplaypreview.h` and `src/screenplaypreview.cpp`: the preview pane, drawn with the same code as the PDF.
- `src/ScreenplayPane.qml`: scrolling and caret following for the preview.

The changes to Omawrite's own files are small: the split layout, shortcuts, footer buttons, and Enter key in `Main.qml`; export, printing, and the preview setting in `backend.cpp`; the app name in `main.cpp` and `cli.cpp`.

---

## Credits and licenses

Omascript is a fork of [Omawrite](https://github.com/omacom/omawrite) by David Heinemeier Hansson and contributors, used under the MIT license (see `LICENSE`).

Bundled fonts, both under the SIL Open Font License 1.1:

- IBM Plex Mono, the editor font, copyright IBM Corp. (`fonts/OFL.txt`).
- Courier Prime, the screenplay font, copyright The Courier Prime Project Authors (`fonts/courier-prime/OFL.txt`).
