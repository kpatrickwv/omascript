#pragma once

// Screenplay formatting for Omascript: turns ordinary Markdown into pages laid
// out to the US film/TV standard (Courier 12pt, 6 lines and 10 characters per
// inch, 54 lines per page). The same layout feeds the live preview, PDF
// export and printing, so all three always agree.
//
// How Markdown maps to screenplay elements:
//   front matter (--- ... ---)   title page (title, credit, author, source,
//                                contact, draft, date) and options
//                                (scene numbers, contd)
//   ## INT. KITCHEN - NIGHT      scene heading (INT./EXT. lines are detected
//                                even without the ##)
//   ### ANGLE ON THE DOOR        shot
//   plain paragraph              action
//   **MARGARET** (V.O.)          character cue, dialogue on the lines below
//   *(whispering)*               parenthetical inside a speech
//   **MARGARET:** Hello.         cue and dialogue on one line
//   > CUT TO:                    transition (uppercase "... TO:" detected)
//   # ACT ONE                    centered heading
//   ---                          page break
//   <!-- note --> or %%note%%    never printed

#include <QString>
#include <QStringList>
#include <QVector>

class QPainter;
class QPagedPaintDevice;

namespace Screenplay {

enum Style : quint8 { Plain = 0, Bold = 1, Italic = 2, Underline = 4 };

struct StyledChar {
    QChar ch;
    quint8 style = Plain;
};
using StyledText = QVector<StyledChar>;

enum class Kind {
    Blank,
    Scene,
    Shot,
    Action,
    Character,
    Parenthetical,
    Dialogue,
    More,
    Transition,
    Centered,
};

struct Line {
    Kind kind = Kind::Blank;
    StyledText chars;
    int sceneNumber = 0;
    // Offset into the Markdown this line came from, for keeping the preview
    // beside the caret. -1 for lines the formatter made up, like (MORE).
    int sourcePosition = -1;

    QString text() const;
};

struct Page {
    QVector<Line> rows;
};

struct TitlePage {
    QStringList title;
    QString credit;
    QStringList authors;
    QStringList source;
    QStringList contact;
    QStringList bottomRight;

    bool isEmpty() const { return title.isEmpty(); }
};

struct Options {
    bool sceneNumbers = false;
    bool autoContinued = false;
    bool titlePage = true;
};

struct Layout {
    TitlePage titlePage;   // empty when there is no title or it is turned off
    QVector<Page> pages;
    QString documentTitle;
    QString documentAuthor;
};

// Geometry, in points.
constexpr qreal PageWidth = 612.0;    // 8.5in
constexpr qreal PageHeight = 792.0;   // 11in
constexpr int LinesPerPage = 54;

// Front matter can switch options on (`scene numbers: true`, `contd: true`);
// anything it sets wins over what the caller passes.
Layout layout(const QString &markdown, Options options = Options());

// The document's top coordinate (in points, pages stacked with the title page
// first when there is one) of the line nearest the given Markdown offset.
qreal documentYForSourcePosition(const Layout &layout, int sourcePosition, qreal pageGap);
// The sheet (title page first, when there is one) and row of that line;
// false when the offset is above the first formatted line (in the front
// matter, say) or there are no lines at all.
bool locateSourcePosition(const Layout &layout, int sourcePosition, int *sheet, int *row);

// Drawing works in points with the origin at the page's top left corner;
// callers scale the painter to their device.
void paintTitlePage(QPainter *painter, const TitlePage &titlePage);
void paintPage(QPainter *painter, const Page &page, int pageNumber);

// Draws every page onto a printer or PDF writer, one sheet per page. The
// device's page size and margins are set to US Letter with no margins.
bool render(const Layout &layout, QPagedPaintDevice *device);
bool exportPdf(const Layout &layout, const QString &path, QString *error = nullptr);

// The bundled Courier Prime faces; needs a running QGuiApplication.
void registerFonts();
QString fontFamily();

// `omascript --pdf script.md [-o out.pdf]` without opening a window.
int exportCommand(const QStringList &arguments);

}
