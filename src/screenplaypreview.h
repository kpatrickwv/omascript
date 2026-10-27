#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QTimer>

#include "screenplay.h"

// The formatted script, drawn page by page beside the editor. It paints only
// the pages in view, at the pane's width, with the same code that writes the
// PDF; a Flickable over it supplies the scrolling through contentY.
class ScreenplayPreview : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY layoutChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY layoutChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor edgeColor READ edgeColor WRITE setEdgeColor NOTIFY edgeColorChanged)
    Q_PROPERTY(QColor markerColor READ markerColor WRITE setMarkerColor NOTIFY markerColorChanged)
    Q_PROPERTY(int caretPosition READ caretPosition WRITE setCaretPosition NOTIFY caretPositionChanged)

public:
    explicit ScreenplayPreview(QQuickItem *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    qreal contentY() const { return m_contentY; }
    void setContentY(qreal y);
    qreal contentHeight() const;
    int pageCount() const { return m_layout.pages.size(); }
    QColor backgroundColor() const { return m_background; }
    void setBackgroundColor(const QColor &color);
    QColor edgeColor() const { return m_edge; }
    void setEdgeColor(const QColor &color);
    QColor markerColor() const { return m_marker; }
    void setMarkerColor(const QColor &color);
    int caretPosition() const { return m_caretPosition; }
    void setCaretPosition(int position);

    // Where, in content pixels, the line nearest a Markdown offset is drawn.
    Q_INVOKABLE qreal yForSourcePosition(int position) const;
    // Lays out any pending text now instead of after the typing pause.
    Q_INVOKABLE void relayoutNow();

    void paint(QPainter *painter) override;

    const Screenplay::Layout &currentLayout() const { return m_layout; }

signals:
    void textChanged();
    void contentYChanged();
    void layoutChanged();
    void backgroundColorChanged();
    void edgeColorChanged();
    void markerColorChanged();
    void caretPositionChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    int sheetCount() const;
    qreal scale() const;
    qreal pageLeft() const;
    qreal sheetPitch() const;   // page height plus gap, in pixels

    QString m_text;
    bool m_dirty = false;
    Screenplay::Layout m_layout;
    QTimer m_relayoutTimer;
    qreal m_contentY = 0;
    QColor m_background = QColor(QStringLiteral("#1e1e1e"));
    QColor m_edge = QColor(0, 0, 0, 60);
    QColor m_marker = QColor(QStringLiteral("#7aa2f7"));
    int m_caretPosition = -1;
};
