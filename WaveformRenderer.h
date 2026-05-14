#ifndef WAVEFORMRENDERER_H
#define WAVEFORMRENDERER_H

/**
 * @file WaveformRenderer.h
 * @brief Facade that composes DataMapper + PathBuilder + BitmapRenderer.
 *
 * Preserves the original single-call API while delegating to layered components.
 * For more granular control, use the individual components directly.
 */

#include "DataMapper.h"
#include "PathBuilder.h"
#include "BitmapRenderer.h"

#include <QImage>
#include <QPainter>
#include <QVector>
#include <QString>
#include <QColor>
#include <QRectF>

class WaveformRenderer
{
public:
    struct RenderOptions {
        int     xMax;
        float   yMin;
        float   yMax;
        QColor  lineColor;
        float   lineWidth;
        QColor  bgColor;
        QColor  gridColor;
        bool    showGrid;
        bool    useBitmap;
        int     gridXCount;
        int     gridYCount;
        QColor  historyColor;
        float   historyAlpha;

        RenderOptions()
            : xMax(1000), yMin(-1.0f), yMax(1.0f)
            , lineColor(Qt::green), lineWidth(1.5f)
            , bgColor(Qt::black), gridColor(40, 40, 40)
            , showGrid(false), useBitmap(false)
            , gridXCount(8), gridYCount(4)
            , historyColor(0, 255, 0, 60), historyAlpha(0.25f)
        {}
    };

public:
    WaveformRenderer() = default;

    // ── Single-channel ──

    QImage renderWaveform(const float* data, int dataLen,
                          int imgWidth, int imgHeight,
                          const RenderOptions& opt = RenderOptions()) const
    {
        if (imgWidth <= 0 || imgHeight <= 0) return QImage();
        QImage img(imgWidth, imgHeight, QImage::Format_ARGB32_Premultiplied);

        if (opt.useBitmap)
            renderBitmapSingle(img, data, dataLen, opt);
        else
            renderPathSingle(img, data, dataLen, opt);
        return img;
    }

    QImage renderWaveform(const QVector<float>& data,
                          int imgWidth, int imgHeight,
                          const RenderOptions& opt = RenderOptions()) const
    {
        return renderWaveform(data.constData(), data.size(),
                              imgWidth, imgHeight, opt);
    }

    // ── Multi-channel grid ──

    QImage renderGrid(const QVector<QVector<float>>& channels,
                      int gridCols,
                      int imgWidth, int imgHeight,
                      const RenderOptions& opt = RenderOptions(),
                      const QVector<QString>& names = {}) const
    {
        QImage img(imgWidth, imgHeight, QImage::Format_ARGB32_Premultiplied);
        img.fill(opt.bgColor);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int chCount = channels.size();
        const int gridRows = (chCount + gridCols - 1) / gridCols;
        const float margin = 4.0f;
        const float cellW = (imgWidth  - margin * (gridCols + 1)) / gridCols;
        const float cellH = (imgHeight - margin * (gridRows + 1)) / gridRows;

        PathBuilder pathBuilder;
        pathBuilder.setLineColor(opt.lineColor, opt.lineWidth);

        for (int ch = 0; ch < chCount; ++ch) {
            const int row = ch / gridCols;
            const int col = ch % gridCols;
            QRectF cellRect(margin + col * (cellW + margin),
                            margin + row * (cellH + margin),
                            cellW, cellH);

            if (opt.showGrid)
                drawGrid(painter, cellRect, opt);

            if (!names.isEmpty() && ch < names.size())
                drawChannelName(painter, cellRect, names[ch], opt);

            if (channels[ch].isEmpty()) continue;

            float yMin = opt.yMin, yMax = opt.yMax;
            if (yMin >= yMax) {
                auto [dmin, dmax] = findDataRange(channels[ch]);
                float pad = (dmax - dmin) * 0.1f;
                if (pad < 0.001f) pad = 1.0f;
                yMin = dmin - pad;
                yMax = dmax + pad;
            }

            int cellWpx = static_cast<int>(cellRect.width());
            int cellHpx = static_cast<int>(cellRect.height());

            DataMapper mapper(opt.xMax, yMin, yMax,
                              cellWpx, cellHpx,
                              static_cast<float>(cellRect.left()));
            auto pts = mapper.map(channels[ch].constData(), channels[ch].size());
            pathBuilder.draw(painter, pts);
        }
        painter.end();
        return img;
    }

private:
    static std::pair<float, float> findDataRange(const QVector<float>& data) {
        float dmin = std::numeric_limits<float>::max();
        float dmax = std::numeric_limits<float>::lowest();
        for (auto v : data) {
            if (v < dmin) dmin = v;
            if (v > dmax) dmax = v;
        }
        if (dmin > dmax) { dmin = -1.0f; dmax = 1.0f; }
        return {dmin, dmax};
    }

    void renderPathSingle(QImage& img, const float* data, int len,
                          const RenderOptions& opt) const
    {
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        img.fill(opt.bgColor);

        if (opt.showGrid)
            drawGrid(painter, QRectF(0, 0, img.width(), img.height()), opt);

        if (len == 0) return;

        DataMapper mapper(opt.xMax, opt.yMin, opt.yMax,
                          img.width(), img.height());
        auto pts = mapper.map(data, len);

        PathBuilder pathBuilder;
        pathBuilder.setLineColor(opt.lineColor, opt.lineWidth);
        pathBuilder.draw(painter, pts);
        painter.end();
    }

    void renderBitmapSingle(QImage& img, const float* data, int len,
                            const RenderOptions& opt) const
    {
        img.fill(opt.bgColor);
        if (len == 0) return;

        DataMapper mapper(opt.xMax, opt.yMin, opt.yMax,
                          img.width(), img.height());
        auto colResult = mapper.aggregateColumns(data, len);

        BitmapRenderer bitmapRenderer;
        bitmapRenderer.draw(img, colResult.spans, opt.lineColor);
    }

    // ── Grid & labels ──

    void drawGrid(QPainter& painter, const QRectF& rect,
                  const RenderOptions& opt) const
    {
        QPen gridPen(opt.gridColor, 0.5);
        painter.setPen(gridPen);

        float xStep = rect.width()  / opt.gridXCount;
        float yStep = rect.height() / opt.gridYCount;

        for (int i = 1; i < opt.gridXCount; ++i) {
            float x = static_cast<float>(rect.left()) + i * xStep;
            painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (int i = 1; i < opt.gridYCount; ++i) {
            float y = static_cast<float>(rect.top()) + i * yStep;
            painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }

    void drawChannelName(QPainter& painter, const QRectF& cellRect,
                         const QString& name, const RenderOptions& opt) const
    {
        QFont font;
        font.setPointSize(8);
        painter.setFont(font);
        painter.setPen(opt.lineColor);
        painter.drawText(QRectF(cellRect.left() + 2, cellRect.top() + 2,
                                cellRect.width() - 4, 16),
                         Qt::AlignLeft | Qt::AlignTop, name);
    }
};

#endif // WAVEFORMRENDERER_H
