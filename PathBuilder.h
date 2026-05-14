#ifndef PATHBUILDER_H
#define PATHBUILDER_H

#include <QPainterPath>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QPointF>
#include <vector>
#include "DataMapper.h"

/**
 * @brief QPainterPath waveform builder — pixel coords → smooth vector path.
 *
 * Takes pre-mapped pixel coordinates (from DataMapper::map()) and builds
 * a QPainterPath via moveTo/lineTo, then draws it onto a QPainter.
 *
 * Dependencies: QtGui + DataMapper.h (for pixel types)
 *
 * Usage:
 * @code
 *   PathBuilder builder;
 *   builder.setLineColor(Qt::green, 1.5f);
 *   builder.draw(painter, mappedPoints);  // single path
 * @endcode
 */

class PathBuilder
{
public:
    using PixelPoint = DataMapper::PixelPoint;

    PathBuilder()
        : m_lineColor(Qt::green)
        , m_lineWidth(1.5f)
    {}

    void setLineColor(const QColor& color, float width = 1.5f) {
        m_lineColor = color;
        m_lineWidth = width;
    }
    void setLineColor(const QColor& color) { m_lineColor = color; }
    void setLineWidth(float width)         { m_lineWidth = width; }

    const QColor& lineColor() const { return m_lineColor; }
    float lineWidth() const { return m_lineWidth; }

    // ── Single draw ──

    /**
     * @brief Build a QPainterPath from pixel points and draw it.
     * @param painter  Target QPainter
     * @param pts      Mapped pixel coordinates (from DataMapper)
     * @param alpha    Optional transparency override [0, 1]
     */
    void draw(QPainter& painter,
              const std::vector<PixelPoint>& pts,
              float alpha = 1.0f) const
    {
        if (pts.empty()) return;
        QPainterPath path = buildPath(pts);
        if (path.isEmpty()) return;

        QColor color = m_lineColor;
        if (alpha < 1.0f) color.setAlphaF(alpha);

        QPen pen(color, m_lineWidth);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.drawPath(path);
    }

    /**
     * @brief Build and return the QPainterPath without drawing.
     * @param pts  Mapped pixel coordinates
     * @return     QPainterPath (empty if no points)
     */
    static QPainterPath buildPath(const std::vector<PixelPoint>& pts) {
        QPainterPath path;
        if (pts.empty()) return path;
        path.moveTo(pts[0].x, pts[0].y);
        for (size_t i = 1; i < pts.size(); ++i)
            path.lineTo(pts[i].x, pts[i].y);
        return path;
    }

    /** Overload: build from float arrays (x, y separate). */
    static QPainterPath buildPath(const float* xs, const float* ys, int count) {
        QPainterPath path;
        if (count <= 0) return path;
        path.moveTo(xs[0], ys[0]);
        for (int i = 1; i < count; ++i)
            path.lineTo(xs[i], ys[i]);
        return path;
    }

    /** Overload: build from QVector<QPointF>. */
    static QPainterPath buildPath(const QVector<QPointF>& pts) {
        QPainterPath path;
        if (pts.isEmpty()) return path;
        path.moveTo(pts[0]);
        for (int i = 1; i < pts.size(); ++i)
            path.lineTo(pts[i]);
        return path;
    }

private:
    QColor m_lineColor;
    float  m_lineWidth;
};

#endif // PATHBUILDER_H
