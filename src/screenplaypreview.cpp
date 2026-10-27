#include "screenplaypreview.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal Margin = 24.0;       // around the stack of pages, in pixels
constexpr qreal Gap = 20.0;          // between pages, in pixels
constexpr qreal MaxPageWidth = 900.0;
constexpr int TypingPause = 120;     // ms of quiet before the pages are redrawn
}

ScreenplayPreview::ScreenplayPreview(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setOpaquePainting(true);
    Screenplay::registerFonts();
    m_relayoutTimer.setSingleShot(true);
    m_relayoutTimer.setInterval(TypingPause);
    connect(&m_relayoutTimer, &QTimer::timeout, this, &ScreenplayPreview::relayoutNow);
}

void ScreenplayPreview::setText(const QString &text) {
    if (text == m_text)
        return;
    m_text = text;
    m_dirty = true;
    emit textChanged();
    // The first text (opening a file) shows at once; typing waits for a pause.
    if (m_layout.pages.isEmpty() && m_layout.titlePage.isEmpty())
        relayoutNow();
    else
        m_relayoutTimer.start();
}

void ScreenplayPreview::relayoutNow() {
    m_relayoutTimer.stop();
    if (!m_dirty)
        return;
    m_dirty = false;
    m_layout = Screenplay::layout(m_text);
    emit layoutChanged();
    update();
}

void ScreenplayPreview::setContentY(qreal y) {
    if (qFuzzyCompare(y, m_contentY))
        return;
    m_contentY = y;
    emit contentYChanged();
    update();
}

void ScreenplayPreview::setBackgroundColor(const QColor &color) {
    if (color == m_background)
        return;
    m_background = color;
    emit backgroundColorChanged();
    update();
}

void ScreenplayPreview::setEdgeColor(const QColor &color) {
    if (color == m_edge)
        return;
    m_edge = color;
    emit edgeColorChanged();
    update();
}

void ScreenplayPreview::setMarkerColor(const QColor &color) {
    if (color == m_marker)
        return;
    m_marker = color;
    emit markerColorChanged();
    update();
}

void ScreenplayPreview::setCaretPosition(int position) {
    if (position == m_caretPosition)
        return;
    m_caretPosition = position;
    emit caretPositionChanged();
    update();
}

int ScreenplayPreview::sheetCount() const {
    const int pages = m_layout.pages.size() + (m_layout.titlePage.isEmpty() ? 0 : 1);
    return std::max(1, pages);
}

qreal ScreenplayPreview::scale() const {
    const qreal pageWidth = std::clamp(width() - 2 * Margin, 120.0, MaxPageWidth);
    return pageWidth / Screenplay::PageWidth;
}

qreal ScreenplayPreview::pageLeft() const {
    return std::round((width() - Screenplay::PageWidth * scale()) / 2);
}

qreal ScreenplayPreview::sheetPitch() const {
    return Screenplay::PageHeight * scale() + Gap;
}

qreal ScreenplayPreview::contentHeight() const {
    return 2 * Margin + sheetCount() * sheetPitch() - Gap;
}

qreal ScreenplayPreview::yForSourcePosition(int position) const {
    const qreal s = scale();
    return Margin + Screenplay::documentYForSourcePosition(m_layout, position, Gap / s) * s;
}

void ScreenplayPreview::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.width() != oldGeometry.width())
        emit layoutChanged();      // the page scale, and so the content height, moved
    update();
}

void ScreenplayPreview::paint(QPainter *painter) {
    painter->fillRect(QRectF(0, 0, width(), height()), m_background);

    const qreal s = scale();
    const qreal left = pageLeft();
    const qreal pageHeight = Screenplay::PageHeight * s;
    const qreal pageWidth = Screenplay::PageWidth * s;
    const bool hasTitlePage = !m_layout.titlePage.isEmpty();

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);

    // Which formatted line the caret is on, marked in the page's left margin.
    int caretSheet = -1;
    int caretRow = -1;
    if (m_caretPosition >= 0)
        Screenplay::locateSourcePosition(m_layout, m_caretPosition, &caretSheet, &caretRow);

    for (int sheet = 0; sheet < sheetCount(); ++sheet) {
        const qreal top = Margin + sheet * sheetPitch() - m_contentY;
        if (top > height() || top + pageHeight < 0)
            continue;

        const QRectF paper(left, top, pageWidth, pageHeight);
        painter->setPen(QPen(m_edge, 1));
        painter->setBrush(Qt::white);
        painter->drawRect(paper.adjusted(-0.5, -0.5, 0.5, 0.5));

        painter->save();
        painter->translate(left, top);
        painter->scale(s, s);
        painter->setPen(Qt::black);
        painter->setBrush(Qt::NoBrush);
        if (hasTitlePage && sheet == 0)
            Screenplay::paintTitlePage(painter, m_layout.titlePage);
        else {
            const int page = sheet - (hasTitlePage ? 1 : 0);
            if (page < m_layout.pages.size())
                Screenplay::paintPage(painter, m_layout.pages.at(page), page + 1);
        }
        painter->restore();

        if (sheet == caretSheet) {
            // Level with the row's lettering: 1in top margin, 12pt rows.
            const qreal rowTop = top + (72.0 + caretRow * 12.0 + 1.0) * s;
            const qreal barWidth = std::max(2.0, 3.0 * s);
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_marker);
            painter->drawRoundedRect(QRectF(left + 0.85 * 72.0 * s, rowTop, barWidth, 11.0 * s),
                                     barWidth / 2, barWidth / 2);
        }
    }
}
