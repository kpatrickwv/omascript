#include "screenplay.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QPageLayout>
#include <QPageSize>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <memory>
#include <optional>

namespace Screenplay {

QString Line::text() const {
    QString out;
    out.reserve(chars.size());
    for (const StyledChar &c : chars)
        out += c.ch;
    return out;
}

namespace {

constexpr qreal Inch = 72.0;
constexpr qreal LineHeight = 12.0;
constexpr qreal CharWidth = 7.2;     // Courier at 12pt is 10 characters per inch
constexpr qreal TopMargin = Inch;
constexpr qreal TextRight = 7.5 * Inch;
constexpr qreal TextCenter = 4.5 * Inch;

// ---------------------------------------------------------------------------
// Backslash escapes are swapped for private-use characters while parsing, so
// an escaped * or _ never starts a style, then swapped back for display.
// ---------------------------------------------------------------------------
const QString EscapableCharacters = QStringLiteral("*_\\<>#%");

QChar escapeFor(QChar c) {
    return QChar(char16_t(0xE000 + EscapableCharacters.indexOf(c)));
}

QChar unescape(QChar c) {
    const int index = c.unicode() - 0xE000;
    return index >= 0 && index < EscapableCharacters.size() ? EscapableCharacters.at(index) : c;
}

QString protect(const QString &text) {
    if (!text.contains(QLatin1Char('\\')))
        return text;
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\\') && i + 1 < text.size()
            && EscapableCharacters.contains(text.at(i + 1))) {
            out += escapeFor(text.at(i + 1));
            ++i;
        } else {
            out += text.at(i);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Inline Markdown: **bold**, *italic*, _italic_, ***both***, <u>underline</u>
// ---------------------------------------------------------------------------
struct InlinePattern {
    QRegularExpression expression;
    quint8 style;
};

const QVector<InlinePattern> &inlinePatterns() {
    static const QVector<InlinePattern> patterns = {
        {QRegularExpression(QStringLiteral(R"(\*\*\*(.+?)\*\*\*)")), Bold | Italic},
        {QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")), Bold},
        {QRegularExpression(QStringLiteral(R"(__(.+?)__)")), Bold},
        {QRegularExpression(QStringLiteral(R"(<u>(.+?)</u>)"),
                            QRegularExpression::CaseInsensitiveOption), Underline},
        {QRegularExpression(QStringLiteral(R"(\*(.+?)\*)")), Italic},
        {QRegularExpression(QStringLiteral(R"((?<!\w)_(.+?)_(?!\w))"),
                            QRegularExpression::UseUnicodePropertiesOption), Italic},
    };
    return patterns;
}

void appendPlain(StyledText &out, QStringView text, quint8 style) {
    for (QChar c : text)
        out.append({unescape(c), style});
}

void parseInline(const QString &text, quint8 style, StyledText &out) {
    int position = 0;
    while (position < text.size()) {
        QRegularExpressionMatch best;
        quint8 bestStyle = 0;
        for (const InlinePattern &pattern : inlinePatterns()) {
            const QRegularExpressionMatch match = pattern.expression.match(text, position);
            if (match.hasMatch()
                && (!best.hasMatch() || match.capturedStart() < best.capturedStart())) {
                best = match;
                bestStyle = pattern.style;
            }
        }
        if (!best.hasMatch()) {
            appendPlain(out, QStringView(text).mid(position), style);
            return;
        }
        appendPlain(out, QStringView(text).mid(position, best.capturedStart() - position), style);
        parseInline(best.captured(1), style | bestStyle, out);
        position = best.capturedEnd();
    }
}

StyledText parseInline(const QString &text) {
    StyledText out;
    out.reserve(text.size());
    parseInline(text, Plain, out);
    return out;
}

QString plain(const QString &protectedText) {
    QString out;
    for (const StyledChar &c : parseInline(protectedText))
        out += c.ch;
    return out;
}

// Word wrap; '\n' forces a break. Later lines may use a different width,
// which is how a parenthetical gets its hanging indent.
QVector<StyledText> wrap(const StyledText &chars, int width, int restWidth) {
    QVector<StyledText> lines;
    QVector<StyledText> paragraphs(1);
    for (const StyledChar &c : chars) {
        if (c.ch == QLatin1Char('\n'))
            paragraphs.append(StyledText());
        else
            paragraphs.last().append(c);
    }

    for (const StyledText &paragraph : paragraphs) {
        QVector<StyledText> words;
        StyledText word;
        for (const StyledChar &c : paragraph) {
            if (c.ch.isSpace()) {
                if (!word.isEmpty()) {
                    words.append(word);
                    word.clear();
                }
            } else {
                word.append(c);
            }
        }
        if (!word.isEmpty())
            words.append(word);

        StyledText line;
        for (StyledText w : words) {
            int limit = lines.isEmpty() ? width : restWidth;
            while (w.size() > limit) {            // one enormous word
                if (!line.isEmpty()) {
                    lines.append(line);
                    line.clear();
                }
                lines.append(w.mid(0, limit));
                w = w.mid(limit);
                limit = restWidth;
            }
            if (w.isEmpty())
                continue;
            if (line.isEmpty()) {
                line = w;
            } else if (line.size() + 1 + w.size() <= limit) {
                const quint8 shared = line.last().style & w.first().style & Underline;
                line.append({QLatin1Char(' '), shared});
                line += w;
            } else {
                lines.append(line);
                line = w;
            }
        }
        lines.append(line);
    }
    return lines;
}

// ---------------------------------------------------------------------------
// Element specs: left edge (inches), width (characters), blank lines above
// ---------------------------------------------------------------------------
enum class Align { Left, Right, Center };

struct Spec {
    qreal x;
    int width;
    int before;
    bool upper;
    Align align;
};

Spec spec(Kind kind) {
    switch (kind) {
    case Kind::Scene:         return {1.5, 60, 1, true, Align::Left};
    case Kind::Shot:          return {1.5, 60, 1, true, Align::Left};
    case Kind::Action:        return {1.5, 60, 1, false, Align::Left};
    case Kind::Character:     return {3.7, 38, 1, true, Align::Left};
    case Kind::Parenthetical: return {3.1, 25, 0, false, Align::Left};
    case Kind::Dialogue:      return {2.5, 35, 0, false, Align::Left};
    case Kind::More:          return {3.7, 38, 0, false, Align::Left};
    case Kind::Transition:    return {0, 60, 1, true, Align::Right};
    case Kind::Centered:      return {0, 60, 1, true, Align::Center};
    case Kind::Blank:         break;
    }
    return {1.5, 60, 0, false, Align::Left};
}

// ---------------------------------------------------------------------------
// Parsing Markdown into elements
// ---------------------------------------------------------------------------
struct BodyPart {
    Kind kind;          // Parenthetical or Dialogue
    QString text;
    int position;
};

struct Element {
    enum Type { Action, Scene, Shot, Centered, Transition, Dialogue, PageBreak } type;
    QString text;
    int position = 0;
    QString cue;
    QVector<BodyPart> body;
};

using Meta = QHash<QString, QStringList>;

const QRegularExpression &sceneExpression() {
    static const QRegularExpression e(
        QStringLiteral(R"(^(INT|EXT|EST|INT\.?/EXT|EXT\.?/INT|I/E)[.\s])"),
        QRegularExpression::CaseInsensitiveOption);
    return e;
}

const QRegularExpression &transitionExpression() {
    static const QRegularExpression e(QStringLiteral(
        R"(^[A-Z0-9 .'\-]+ TO:$|^(FADE OUT|FADE TO BLACK|CUT TO BLACK|SMASH CUT|THE END)[.:]?$)"));
    return e;
}

bool looksLikeName(QString name) {
    name = name.trimmed();
    static const QRegularExpression exclaim(QStringLiteral("[!?]"));
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    return !name.isEmpty() && name.size() <= 38
        && name.split(spaces, Qt::SkipEmptyParts).size() <= 5
        && !name.contains(exclaim)
        && !name.endsWith(QLatin1Char('.')) && !name.endsWith(QLatin1Char(','));
}

QString stripQuotes(QString value) {
    const QString quotes = QStringLiteral("\"'");
    while (!value.isEmpty() && quotes.contains(value.front()))
        value.remove(0, 1);
    while (!value.isEmpty() && quotes.contains(value.back()))
        value.chop(1);
    return value;
}

Meta parseFrontMatter(const QStringList &lines) {
    static const QRegularExpression item(QStringLiteral(R"(^\s+-\s+(.*)$)"));
    static const QRegularExpression continuation(QStringLiteral(R"(^\s+(\S.*)$)"));
    static const QRegularExpression keyValue(QStringLiteral(R"(^([A-Za-z][\w \-']*):\s*(.*)$)"));

    Meta meta;
    QString key;
    for (const QString &raw : lines) {
        if (raw.trimmed().isEmpty())
            continue;
        const auto itemMatch = item.match(raw);
        const auto keyMatch = keyValue.match(raw);
        const auto continued = continuation.match(raw);
        if (itemMatch.hasMatch() && !key.isEmpty()) {
            meta[key].append(stripQuotes(itemMatch.captured(1).trimmed()));
        } else if (keyMatch.hasMatch()) {
            key = keyMatch.captured(1).trimmed().toLower();
            const QString value = keyMatch.captured(2).trimmed();
            if (value.isEmpty() || value == QLatin1String("|") || value == QLatin1String(">")) {
                meta[key] = QStringList();
            } else if (value.startsWith(QLatin1Char('[')) && value.endsWith(QLatin1Char(']'))) {
                QStringList values;
                for (const QString &v : value.mid(1, value.size() - 2).split(QLatin1Char(',')))
                    values.append(stripQuotes(v.trimmed()));
                meta[key] = values;
            } else {
                meta[key] = QStringList{stripQuotes(value)};
            }
        } else if (continued.hasMatch() && !key.isEmpty()) {
            meta[key].append(continued.captured(1).trimmed());
        }
    }
    return meta;
}

// Markdown hard line break: two trailing spaces or a trailing backslash.
QString hardBreak(const QString &line) {
    const bool hard = line.endsWith(QLatin1String("  "))
        || [&] { QString r = line; while (!r.isEmpty() && r.back().isSpace()) r.chop(1);
                 return r.endsWith(QLatin1Char('\\')); }();
    return hard ? QStringLiteral("\n") : QString();
}

QString joinLines(const QStringList &lines) {
    QString out;
    for (QString l : lines) {
        const bool hard = l.endsWith(QLatin1Char('\n'));
        while (!l.isEmpty() && (l.back() == QLatin1Char('\n') || l.back().isSpace()))
            l.chop(1);
        if (l.endsWith(QLatin1Char('\\'))) {
            l.chop(1);
            while (!l.isEmpty() && l.back().isSpace())
                l.chop(1);
        }
        if (hard)
            l += QLatin1Char('\n');
        if (out.isEmpty() || out.endsWith(QLatin1Char('\n')))
            out += l;
        else
            out += QLatin1Char(' ') + l;
    }
    while (out.endsWith(QLatin1Char('\n')))
        out.chop(1);
    return out;
}

// Removes comments, remembering where each remaining character came from.
QString stripComments(const QString &text, int base, QVector<int> *origin) {
    static const QRegularExpression comments(QStringLiteral("<!--.*?-->|%%.*?%%"),
                                             QRegularExpression::DotMatchesEverythingOption);
    QString out;
    out.reserve(text.size());
    origin->clear();
    origin->reserve(text.size() + 1);
    int position = 0;
    auto it = comments.globalMatch(text);
    const auto keep = [&](int from, int to) {
        for (int i = from; i < to; ++i) {
            out += text.at(i);
            origin->append(base + i);
        }
    };
    while (it.hasNext()) {
        const auto match = it.next();
        keep(position, match.capturedStart());
        position = match.capturedEnd();
    }
    keep(position, text.size());
    origin->append(base + text.size());
    return out;
}

bool truthy(const Meta &meta, std::initializer_list<const char *> keys, bool fallback) {
    for (const char *key : keys) {
        const auto it = meta.constFind(QString::fromLatin1(key));
        if (it == meta.constEnd() || it->isEmpty())
            continue;
        const QString v = it->first().trimmed().toLower();
        return v == QLatin1String("true") || v == QLatin1String("yes")
            || v == QLatin1String("on") || v == QLatin1String("1");
    }
    return fallback;
}

QVector<Element> parse(const QString &input, Meta *meta) {
    QString text = input;
    text.remove(QLatin1Char('\r'));

    // Front matter: a --- block on the very first line.
    int bodyStart = 0;
    if (text.startsWith(QLatin1String("---"))) {
        const int firstEnd = text.indexOf(QLatin1Char('\n'));
        if (firstEnd >= 0 && text.left(firstEnd).trimmed() == QLatin1String("---")) {
            QStringList frontLines;
            int lineStart = firstEnd + 1;
            while (lineStart <= text.size()) {
                int lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
                if (lineEnd < 0)
                    lineEnd = text.size();
                const QString line = text.mid(lineStart, lineEnd - lineStart);
                if (line.trimmed() == QLatin1String("---") || line.trimmed() == QLatin1String("...")) {
                    *meta = parseFrontMatter(frontLines);
                    bodyStart = std::min<int>(lineEnd + 1, text.size());
                    break;
                }
                frontLines.append(line);
                if (lineEnd >= text.size())
                    break;
                lineStart = lineEnd + 1;
            }
        }
    }

    QVector<int> origin;
    const QString body = stripComments(text.mid(bodyStart), bodyStart, &origin);

    static const QRegularExpression parenExpression(QStringLiteral(R"(^[*_]?\s*(\(.*\))\s*[*_]?$)"));
    static const QRegularExpression pageBreak(QStringLiteral(R"(^\s*(-{3,}|\*{3,}|_{3,}|={3,})\s*$)"));
    static const QRegularExpression heading(QStringLiteral(R"(^(#{1,6})\s+(.*?)\s*#*$)"));
    static const QRegularExpression cueOnly(QStringLiteral(R"(^\*\*([^*]+?)\*\*\s*(\([^)]*\))?\s*$)"));
    static const QRegularExpression cueInline(
        QStringLiteral(R"(^\*\*([^*]+?):\*\*\s*(.+)$|^\*\*([^*]+?)\*\*\s*:\s*(.+)$)"));
    static const QRegularExpression inlineParen(QStringLiteral(R"(^[*_]?(\([^)]*\))[*_]?\s*(.*)$)"));
    static const QRegularExpression parenGroups(QStringLiteral(R"(\(.*?\))"));

    QVector<Element> elements;
    QStringList para;
    int paraPosition = 0;

    struct PendingPart {
        Kind kind;
        QStringList lines;
        int position;
    };
    bool inDialogue = false;
    Element dialogue;
    QString dialogueRaw;
    QVector<PendingPart> parts;

    const auto flush = [&]() {
        if (!para.isEmpty()) {
            elements.append({Element::Action, joinLines(para), paraPosition, {}, {}});
            para.clear();
        }
        if (inDialogue) {
            if (!parts.isEmpty()) {
                for (const PendingPart &part : parts) {
                    dialogue.body.append({part.kind,
                                          part.kind == Kind::Parenthetical ? part.lines.first()
                                                                            : joinLines(part.lines),
                                          part.position});
                }
                elements.append(dialogue);
            } else {      // a bold line with nothing under it is just action
                elements.append({Element::Action, dialogueRaw, dialogue.position, {}, {}});
            }
            inDialogue = false;
            parts.clear();
        }
    };

    const auto startDialogue = [&](const QString &cue, const QString &raw, int position) {
        flush();
        inDialogue = true;
        dialogue = Element{Element::Dialogue, QString(), position, cue.trimmed(), {}};
        dialogueRaw = raw;
    };

    int lineStart = 0;
    while (lineStart <= body.size()) {
        int lineEnd = body.indexOf(QLatin1Char('\n'), lineStart);
        if (lineEnd < 0)
            lineEnd = body.size();
        const QString line = protect(body.mid(lineStart, lineEnd - lineStart));
        const int position = origin.value(lineStart, bodyStart);
        const QString stripped = line.trimmed();
        const auto nextLine = [&]() { lineStart = lineEnd + 1; };

        if (stripped.isEmpty()) {
            flush();
            nextLine();
            continue;
        }

        if (stripped.startsWith(QLatin1Char('\\'))) {           // forced action
            if (inDialogue)
                flush();
            if (para.isEmpty())
                paraPosition = position;
            para.append(stripped.mid(1).trimmed() + hardBreak(line));
            nextLine();
            continue;
        }

        if (inDialogue) {
            const auto paren = parenExpression.match(stripped);
            if (paren.hasMatch())
                parts.append({Kind::Parenthetical, {paren.captured(1)}, position});
            else if (!parts.isEmpty() && parts.last().kind == Kind::Dialogue)
                parts.last().lines.append(stripped + hardBreak(line));
            else
                parts.append({Kind::Dialogue, {stripped + hardBreak(line)}, position});
            nextLine();
            continue;
        }

        if (para.isEmpty() && pageBreak.match(stripped).hasMatch()) {
            flush();
            elements.append({Element::PageBreak, {}, position, {}, {}});
            nextLine();
            continue;
        }

        const auto headingMatch = heading.match(stripped);
        if (headingMatch.hasMatch()) {
            flush();
            const int level = headingMatch.captured(1).size();
            const Element::Type type = level == 1 ? Element::Centered
                                     : level == 2 ? Element::Scene : Element::Shot;
            elements.append({type, headingMatch.captured(2), position, {}, {}});
            nextLine();
            continue;
        }

        if (stripped.startsWith(QLatin1Char('>'))) {
            flush();
            QString transition = stripped;
            while (!transition.isEmpty()
                   && (transition.front() == QLatin1Char('>') || transition.front() == QLatin1Char(' ')))
                transition.remove(0, 1);
            elements.append({Element::Transition, transition.trimmed(), position, {}, {}});
            nextLine();
            continue;
        }

        if (para.isEmpty()) {
            const QString visible = plain(stripped);
            if (sceneExpression().match(visible).hasMatch()) {
                flush();
                elements.append({Element::Scene, stripped, position, {}, {}});
                nextLine();
                continue;
            }
            if (transitionExpression().match(visible).hasMatch()) {
                flush();
                elements.append({Element::Transition, stripped, position, {}, {}});
                nextLine();
                continue;
            }
        }

        const auto cue = cueOnly.match(stripped);
        if (cue.hasMatch() && looksLikeName(cue.captured(1))) {
            QString name = cue.captured(1).trimmed();
            if (!cue.captured(2).isEmpty())
                name += QLatin1Char(' ') + cue.captured(2);
            startDialogue(name, stripped, position);
            nextLine();
            continue;
        }

        const auto oneLiner = cueInline.match(stripped);
        if (oneLiner.hasMatch()) {
            const QString name = (oneLiner.captured(1).isEmpty() ? oneLiner.captured(3)
                                                                 : oneLiner.captured(1)).trimmed();
            QString rest = (oneLiner.captured(2).isEmpty() ? oneLiner.captured(4)
                                                           : oneLiner.captured(2)).trimmed();
            QString bareName = name;
            bareName.remove(parenGroups);
            if (looksLikeName(bareName)) {
                startDialogue(name, stripped, position);
                const auto paren = inlineParen.match(rest);
                if (paren.hasMatch()) {
                    parts.append({Kind::Parenthetical, {paren.captured(1)}, position});
                    rest = paren.captured(2);
                }
                if (!rest.isEmpty())
                    parts.append({Kind::Dialogue, {rest + hardBreak(line)}, position});
                nextLine();
                continue;
            }
        }

        if (para.isEmpty())
            paraPosition = position;
        para.append(stripped + hardBreak(line));
        nextLine();
    }
    flush();
    return elements;
}

void addAutoContinued(QVector<Element> &elements) {
    static const QRegularExpression extension(QStringLiteral(R"(\s*\(.*?\))"));
    QString last;
    for (Element &element : elements) {
        switch (element.type) {
        case Element::Scene:
        case Element::Centered:
        case Element::PageBreak:
            last.clear();
            break;
        case Element::Dialogue: {
            QString base = plain(element.cue);
            base.remove(extension);
            base = base.trimmed().toUpper();
            if (base == last && !element.cue.toUpper().contains(QLatin1String("CONT")))
                element.cue += QStringLiteral(" (CONT'D)");
            last = base;
            break;
        }
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Layout: elements -> blocks of lines -> pages
// ---------------------------------------------------------------------------
struct Block {
    enum Type { PageBreak, Single, Action, Dialogue } type;
    QVector<Line> lines;
    int before = 0;
    bool keepWithNext = false;
    QString cue;
    Kind kind = Kind::Blank;
};

QVector<Line> makeLines(Kind kind, const QString &text, int position) {
    const Spec s = spec(kind);
    StyledText chars = parseInline(text);
    if (s.upper) {
        for (StyledChar &c : chars)
            c.ch = c.ch.toUpper();
    }

    QVector<StyledText> wrapped;
    if (kind == Kind::Parenthetical) {
        wrapped = wrap(chars, s.width, s.width - 1);
        for (int i = 1; i < wrapped.size(); ++i)
            wrapped[i].prepend({QLatin1Char(' '), Plain});
    } else {
        wrapped = wrap(chars, s.width, s.width);
    }

    QVector<Line> lines;
    int consumed = 0;
    for (const StyledText &w : wrapped) {
        Line line;
        line.kind = kind;
        line.chars = w;
        line.sourcePosition = position + consumed;
        consumed += w.size() + 1;
        lines.append(line);
    }
    return lines;
}

StyledText plainChars(const QString &text) {
    StyledText out;
    for (QChar c : text)
        out.append({c, Plain});
    return out;
}

QVector<Block> buildBlocks(const QVector<Element> &elements, bool sceneNumbers) {
    QVector<Block> blocks;
    int sceneNumber = 0;
    for (const Element &element : elements) {
        Block block;
        switch (element.type) {
        case Element::PageBreak:
            block.type = Block::PageBreak;
            break;
        case Element::Dialogue: {
            block.type = Block::Dialogue;
            block.kind = Kind::Character;
            QVector<Line> cue = makeLines(Kind::Character, element.cue, element.position);
            cue.resize(1);
            block.cue = cue.first().text();
            block.lines.append(cue.first());
            for (const BodyPart &part : element.body)
                block.lines += makeLines(part.kind, part.text, part.position);
            block.before = spec(Kind::Character).before;
            break;
        }
        default: {
            Kind kind = Kind::Action;
            switch (element.type) {
            case Element::Scene: kind = Kind::Scene; break;
            case Element::Shot: kind = Kind::Shot; break;
            case Element::Centered: kind = Kind::Centered; break;
            case Element::Transition: kind = Kind::Transition; break;
            default: break;
            }
            block.type = kind == Kind::Action ? Block::Action : Block::Single;
            block.kind = kind;
            block.lines = makeLines(kind, element.text, element.position);
            if (kind == Kind::Scene) {
                ++sceneNumber;
                if (sceneNumbers && !block.lines.isEmpty())
                    block.lines.first().sceneNumber = sceneNumber;
            }
            block.before = spec(kind).before;
            block.keepWithNext = kind == Kind::Scene || kind == Kind::Shot;
            break;
        }
        }
        blocks.append(block);
    }
    return blocks;
}

bool endsSentence(const Line &line) {
    static const QRegularExpression e(
        QStringLiteral("[.?!…:;—\\-]['\")”]*\\s*$"));
    return e.match(line.text()).hasMatch();
}

class Pager {
public:
    QVector<Page> pages;
    QVector<Line> current;

    int room() const { return LinesPerPage - current.size(); }
    int gap(int before) const { return current.isEmpty() ? 0 : before; }

    void put(const QVector<Line> &lines, int before) {
        if (!current.isEmpty()) {
            for (int i = 0; i < before; ++i)
                current.append(Line());
        }
        current += lines;
    }

    void newPage() {
        if (!current.isEmpty())
            pages.append(Page{current});
        current.clear();
    }

    QVector<Page> finish() {
        newPage();
        return pages;
    }
};

int minimumStart(const Block &block) {
    switch (block.type) {
    case Block::PageBreak:
        return 0;
    case Block::Action:
        return block.before + std::min<int>(2, block.lines.size());
    case Block::Dialogue: {
        const int body = block.lines.size() - 1;
        return block.before + 1 + (body <= 3 ? body : 3);    // two lines and (MORE)
    }
    case Block::Single:
        break;
    }
    return block.before + block.lines.size();
}

std::optional<int> splitAction(const QVector<Line> &lines, int available) {
    const int high = std::min<int>(available, lines.size() - 2);
    if (high < 2)
        return std::nullopt;
    for (int k = high; k > 1; --k) {
        if (endsSentence(lines.at(k - 1)))
            return k;
    }
    return high;
}

std::optional<int> splitDialogue(const QVector<Line> &body, int available) {
    const int high = std::min<int>(available, body.size() - 2);
    if (high < 2)
        return std::nullopt;
    std::optional<int> fallback;
    for (int k = high; k > 1; --k) {
        const Line &previous = body.at(k - 1);
        if (previous.kind == Kind::Parenthetical)
            continue;
        if (endsSentence(previous))
            return k;
        if (!fallback)
            fallback = k;
    }
    return fallback;
}

QVector<Page> paginate(const QVector<Block> &blocks) {
    Pager pager;
    Line more;
    more.kind = Kind::More;
    more.chars = plainChars(QStringLiteral("(MORE)"));

    for (int i = 0; i < blocks.size(); ++i) {
        const Block &block = blocks.at(i);
        if (block.type == Block::PageBreak) {
            pager.newPage();
            continue;
        }

        // A transition should never sit alone at the top of a page.
        int tail = 0;
        if (i + 1 < blocks.size() && blocks.at(i + 1).kind == Kind::Transition)
            tail = blocks.at(i + 1).before + blocks.at(i + 1).lines.size();

        if (block.type == Block::Action) {
            QVector<Line> lines = block.lines;
            while (!lines.isEmpty()) {
                const int gap = pager.gap(block.before);
                const bool fits = gap + lines.size() <= pager.room();
                if (fits && (pager.current.isEmpty() || gap + lines.size() + tail <= pager.room())) {
                    pager.put(lines, block.before);
                    break;
                }
                if (fits) {     // fits, but would strand the transition after it
                    const auto k = lines.size() >= 4 ? splitAction(lines, lines.size() - 2)
                                                     : std::nullopt;
                    if (k) {
                        pager.put(lines.mid(0, *k), block.before);
                        lines = lines.mid(*k);
                    }
                    pager.newPage();
                    continue;
                }
                auto k = splitAction(lines, pager.room() - gap);
                if (k) {
                    pager.put(lines.mid(0, *k), block.before);
                } else if (!pager.current.isEmpty()) {
                    pager.newPage();
                    continue;
                } else {
                    k = LinesPerPage;
                    pager.put(lines.mid(0, *k), 0);
                }
                pager.newPage();
                lines = lines.mid(*k);
            }
            continue;
        }

        if (block.type == Block::Dialogue) {
            Line head = block.lines.first();
            QVector<Line> body = block.lines.mid(1);
            Line continued;
            continued.kind = Kind::Character;
            continued.chars = plainChars(block.cue.contains(QLatin1String("CONT'D"))
                                             ? block.cue
                                             : block.cue + QStringLiteral(" (CONT'D)"));
            while (true) {
                const int gap = pager.gap(block.before);
                const bool fits = gap + 1 + body.size() <= pager.room();
                if (fits && (pager.current.isEmpty()
                             || gap + 1 + body.size() + tail <= pager.room())) {
                    pager.put(QVector<Line>{head} + body, block.before);
                    break;
                }
                if (fits) {     // fits, but would strand the transition after it
                    const auto k = body.size() >= 4 ? splitDialogue(body, body.size() - 2)
                                                    : std::nullopt;
                    if (k) {
                        pager.put(QVector<Line>{head} + body.mid(0, *k) + QVector<Line>{more},
                                  block.before);
                        head = continued;
                        body = body.mid(*k);
                    }
                    pager.newPage();
                    continue;
                }
                auto k = splitDialogue(body, pager.room() - gap - 2);
                if (!k) {
                    if (!pager.current.isEmpty()) {
                        pager.newPage();
                        continue;
                    }
                    k = std::max(1, LinesPerPage - 2);
                }
                pager.put(QVector<Line>{head} + body.mid(0, *k) + QVector<Line>{more}, block.before);
                pager.newPage();
                head = continued;
                body = body.mid(*k);
            }
            continue;
        }

        // Scene headings, shots, transitions, centered text: never split.
        int need = pager.gap(block.before) + block.lines.size();
        if (block.keepWithNext && i + 1 < blocks.size())
            need += minimumStart(blocks.at(i + 1));
        if (need > pager.room() && !pager.current.isEmpty())
            pager.newPage();
        pager.put(block.lines, block.before);
    }
    return pager.finish();
}

QStringList metaValues(const Meta &meta, std::initializer_list<const char *> keys) {
    for (const char *key : keys) {
        const QStringList values = meta.value(QString::fromLatin1(key));
        if (!values.isEmpty())
            return values;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
QString &registeredFamily() {
    static QString family;
    return family;
}

QFont fontFor(quint8 style) {
    QFont font(fontFamily());
    font.setStyleHint(QFont::TypeWriter);
    font.setPixelSize(12);
    font.setBold(style & Bold);
    font.setItalic(style & Italic);
    font.setKerning(false);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

qreal baseline(int row) {
    return TopMargin + row * LineHeight + 9.0;
}

void drawChars(QPainter *painter, const StyledText &chars, qreal x, qreal y) {
    int start = 0;
    while (start < chars.size()) {
        const quint8 style = chars.at(start).style;
        int end = start;
        QString run;
        while (end < chars.size() && chars.at(end).style == style) {
            run += chars.at(end).ch;
            ++end;
        }
        const qreal runX = x + start * CharWidth;
        painter->setFont(fontFor(style));
        painter->drawText(QPointF(runX, y), run);
        if (style & Underline) {
            painter->save();
            painter->setPen(QPen(painter->pen().color(), 0.6));
            painter->drawLine(QPointF(runX, y + 1.5), QPointF(runX + run.size() * CharWidth, y + 1.5));
            painter->restore();
        }
        start = end;
    }
}

void drawAligned(QPainter *painter, const StyledText &chars, qreal y, Align align, qreal leftInches) {
    const qreal width = chars.size() * CharWidth;
    qreal x = leftInches * Inch;
    if (align == Align::Right)
        x = TextRight - width;
    else if (align == Align::Center)
        x = TextCenter - width / 2;
    drawChars(painter, chars, x, y);
}

StyledText inlineChars(const QString &text, bool upper = false) {
    StyledText chars = parseInline(protect(text));
    if (upper) {
        for (StyledChar &c : chars)
            c.ch = c.ch.toUpper();
    }
    return chars;
}

} // namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
Layout layout(const QString &markdown, Options options) {
    Meta meta;
    QVector<Element> elements = parse(markdown, &meta);

    options.sceneNumbers = truthy(meta, {"scene numbers", "scene-numbers", "scene_numbers"},
                                  options.sceneNumbers);
    options.autoContinued = truthy(meta, {"contd", "cont'd", "auto contd", "auto-contd"},
                                   options.autoContinued);
    options.titlePage = truthy(meta, {"title page", "title-page", "title_page"}, options.titlePage);

    if (options.autoContinued)
        addAutoContinued(elements);

    Layout result;
    result.pages = paginate(buildBlocks(elements, options.sceneNumbers));

    const QStringList title = metaValues(meta, {"title"});
    result.documentTitle = title.join(QLatin1Char(' '));
    const QStringList authors = metaValues(meta, {"author", "authors", "written by", "writer", "writers"});
    result.documentAuthor = authors.join(QStringLiteral(", "));

    if (options.titlePage && !title.isEmpty()) {
        TitlePage &page = result.titlePage;
        page.title = title;
        page.authors = authors;
        const QStringList credit = metaValues(meta, {"credit"});
        page.credit = credit.isEmpty() ? QStringLiteral("Written by") : credit.first();
        page.source = metaValues(meta, {"source", "based on"});
        page.contact = metaValues(meta, {"contact"});
        page.bottomRight = metaValues(meta, {"draft", "draft date"}) + metaValues(meta, {"date"});
    }
    return result;
}

bool locateSourcePosition(const Layout &layout, int sourcePosition, int *sheet, int *row) {
    const int firstPage = layout.titlePage.isEmpty() ? 0 : 1;
    bool found = false;
    for (int p = 0; p < layout.pages.size(); ++p) {
        const QVector<Line> &rows = layout.pages.at(p).rows;
        for (int r = 0; r < rows.size(); ++r) {
            const int position = rows.at(r).sourcePosition;
            if (position < 0)
                continue;
            if (position > sourcePosition)
                return found;      // before the first line: the front matter
            *sheet = firstPage + p;
            *row = r;
            found = true;
        }
    }
    return found;
}

qreal documentYForSourcePosition(const Layout &layout, int sourcePosition, qreal pageGap) {
    int sheet = 0;
    int row = 0;
    if (!locateSourcePosition(layout, sourcePosition, &sheet, &row))
        return 0;
    return sheet * (PageHeight + pageGap) + TopMargin + row * LineHeight;
}

void paintTitlePage(QPainter *painter, const TitlePage &titlePage) {
    const auto center = [&](const QString &text, int row, bool upper) {
        drawAligned(painter, inlineChars(text, upper), baseline(row), Align::Center, 0);
    };

    int row = 18;
    for (const QString &line : titlePage.title)
        center(line, row++, true);
    if (!titlePage.authors.isEmpty()) {
        row += 3;
        center(titlePage.credit, row, false);
        row += 2;
        center(titlePage.authors.join(QStringLiteral(" & ")), row++, false);
    }
    if (!titlePage.source.isEmpty()) {
        row += 3;
        for (const QString &line : titlePage.source)
            center(line, row++, false);
    }

    for (int n = 0; n < titlePage.contact.size(); ++n) {
        const int r = LinesPerPage - titlePage.contact.size() + n;
        drawChars(painter, inlineChars(titlePage.contact.at(n)), 1.5 * Inch, baseline(r));
    }
    for (int n = 0; n < titlePage.bottomRight.size(); ++n) {
        const int r = LinesPerPage - titlePage.bottomRight.size() + n;
        drawAligned(painter, inlineChars(titlePage.bottomRight.at(n)), baseline(r), Align::Right, 0);
    }
}

void paintPage(QPainter *painter, const Page &page, int pageNumber) {
    if (pageNumber > 1) {
        const StyledText number = plainChars(QStringLiteral("%1.").arg(pageNumber));
        drawChars(painter, number, TextRight - number.size() * CharWidth, 0.5 * Inch + 9.0);
    }
    for (int row = 0; row < page.rows.size(); ++row) {
        const Line &line = page.rows.at(row);
        if (line.kind == Kind::Blank)
            continue;
        const Spec s = spec(line.kind);
        drawAligned(painter, line.chars, baseline(row), s.align, s.x);
        if (line.sceneNumber > 0) {
            const StyledText number = plainChars(QString::number(line.sceneNumber));
            drawChars(painter, number, 1.25 * Inch - number.size() * CharWidth, baseline(row));
            drawChars(painter, number, 7.6 * Inch, baseline(row));
        }
    }
}

bool render(const Layout &layout, QPagedPaintDevice *device) {
    device->setPageLayout(QPageLayout(QPageSize(QPageSize::Letter), QPageLayout::Portrait,
                                      QMarginsF(0, 0, 0, 0), QPageLayout::Point));
    QPainter painter;
    if (!painter.begin(device))
        return false;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(Qt::black);

    const qreal sx = device->logicalDpiX() / Inch;
    const qreal sy = device->logicalDpiY() / Inch;
    bool first = true;
    const auto sheet = [&](const auto &draw) {
        if (!first)
            device->newPage();
        first = false;
        painter.save();
        painter.scale(sx, sy);
        draw();
        painter.restore();
    };

    if (!layout.titlePage.isEmpty())
        sheet([&] { paintTitlePage(&painter, layout.titlePage); });
    for (int p = 0; p < layout.pages.size(); ++p)
        sheet([&] { paintPage(&painter, layout.pages.at(p), p + 1); });
    if (first)          // an empty document still makes a (blank) page
        sheet([] {});
    return painter.end();
}

bool exportPdf(const Layout &layout, const QString &path, QString *error) {
    {
        QFile probe(path);
        if (!probe.open(QIODevice::WriteOnly)) {
            if (error)
                *error = probe.errorString();
            return false;
        }
    }
    QPdfWriter writer(path);
    writer.setResolution(1200);
    writer.setCreator(QStringLiteral("Omascript"));
    writer.setTitle(layout.documentTitle.isEmpty() ? QFileInfo(path).completeBaseName()
                                                   : layout.documentTitle);
    if (!render(layout, &writer)) {
        if (error)
            *error = QStringLiteral("Could not draw the pages.");
        return false;
    }
    return true;
}

void registerFonts() {
    if (!registeredFamily().isEmpty())
        return;
    for (const char *face : {"Regular", "Bold", "Italic", "BoldItalic"}) {
        const int id = QFontDatabase::addApplicationFont(
            QStringLiteral(":/fonts/courier-prime/CourierPrime-%1.ttf").arg(QLatin1String(face)));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            registeredFamily() = families.first();
    }
}

QString fontFamily() {
    return registeredFamily().isEmpty() ? QStringLiteral("Courier Prime") : registeredFamily();
}

int exportCommand(const QStringList &arguments) {
    QTextStream out(stdout);
    QTextStream err(stderr);
    QString input;
    QString output;
    Options options;
    for (int i = 2; i < arguments.size(); ++i) {
        const QString &a = arguments.at(i);
        if (a == QLatin1String("-o") || a == QLatin1String("--output")) {
            if (++i >= arguments.size()) {
                err << "omascript: " << a << " needs a file name\n";
                return 2;
            }
            output = arguments.at(i);
        } else if (a == QLatin1String("--scene-numbers")) {
            options.sceneNumbers = true;
        } else if (a == QLatin1String("--contd")) {
            options.autoContinued = true;
        } else if (a == QLatin1String("--no-title-page")) {
            options.titlePage = false;
        } else if (input.isEmpty()) {
            input = a;
        } else {
            err << "omascript: unexpected argument " << a << "\n";
            return 2;
        }
    }
    if (input.isEmpty()) {
        err << "Usage: omascript --pdf SCRIPT.md [-o OUT.pdf] [--scene-numbers] [--contd]"
               " [--no-title-page]\n";
        return 2;
    }

    QFile file(input);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "omascript: can't open " << input << ": " << file.errorString() << "\n";
        return 1;
    }
    const QString markdown = QString::fromUtf8(file.readAll());
    if (output.isEmpty()) {
        const QFileInfo info(input);
        output = info.dir().filePath(info.completeBaseName() + QStringLiteral(".pdf"));
    }

    // Fonts and the PDF writer need an application, though not a screen.
    std::unique_ptr<QGuiApplication> app;
    if (!QGuiApplication::instance()) {
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        static int argc = 1;
        static char name[] = "omascript";
        static char *argv[] = {name, nullptr};
        app = std::make_unique<QGuiApplication>(argc, argv);
    }
    registerFonts();

    const Layout result = layout(markdown, options);
    QString error;
    if (!exportPdf(result, output, &error)) {
        err << "omascript: can't write " << output << ": " << error << "\n";
        return 1;
    }
    const int sheets = result.pages.size();
    out << "Wrote " << output << " (" << sheets << (sheets == 1 ? " page" : " pages") << ")\n";
    return 0;
}

} // namespace Screenplay
