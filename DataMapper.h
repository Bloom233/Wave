#ifndef DATAMAPPER_H
#define DATAMAPPER_H

#include <utility>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cmath>

/**
 * @brief Pure math coordinate mapper — float[] → pixel coordinates.
 *
 * Zero Qt dependency. Can be used in any C++ project.
 * Provides the X/Y mapping formula extracted from NeuroSC-UI's drawChannel.
 *
 * Usage:
 * @code
 *   DataMapper mapper(500, -600.0f, 600.0f, 800, 250);
 *   auto pts = mapper.map(data, dataLen);
 *   // pts[i].x, pts[i].y are pixel coordinates
 * @endcode
 */
class DataMapper
{
public:
    /** Mapped pixel point (int for fast pixel ops, float for QPainterPath) */
    struct PixelPoint {
        float x;
        float y;
    };

    /** Per-column Y range aggregation (for bitmap renderer) */
    struct ColSpan {
        int32_t yMin;
        int32_t yMax;
    };

    /**
     * @param dataXMax   X-axis data range [0, dataXMax), maps to output pixel width
     * @param dataYMin   Y-axis data minimum
     * @param dataYMax   Y-axis data maximum
     * @param pixelW     Output pixel width
     * @param pixelH     Output pixel height
     */
    DataMapper(int dataXMax, float dataYMin, float dataYMax,
               int pixelW, int pixelH, float pixelOffsetX = 0.0f)
        : m_dataXMax(dataXMax)
        , m_dataYMin(dataYMin)
        , m_dataYMax(dataYMax)
        , m_pixelW(pixelW)
        , m_pixelH(pixelH)
    {
        recalc(pixelW, pixelH, pixelOffsetX);
    }

    /** Reconfigure for new output dimensions. */
    void resize(int pixelW, int pixelH, float pixelOffsetX = 0.0f) {
        recalc(pixelW, pixelH, pixelOffsetX);
    }

    /** Change Y range. */
    void setYRange(float yMin, float yMax) {
        m_dataYMin = yMin;
        m_dataYMax = yMax;
        recalcY();
    }

    /** Change X data range. */
    void setXMax(int xMax) {
        m_dataXMax = xMax;
        recalcX();
    }

    /** Set Y range automatically from data (with padding). */
    void autoYRange(const float* data, int len, float paddingRatio = 0.1f) {
        if (len == 0) return;
        float dmin = std::numeric_limits<float>::max();
        float dmax = std::numeric_limits<float>::lowest();
        for (int i = 0; i < len; ++i) {
            if (data[i] < dmin) dmin = data[i];
            if (data[i] > dmax) dmax = data[i];
        }
        float pad = (dmax - dmin) * paddingRatio;
        if (pad < 0.001f) pad = 1.0f;
        setYRange(dmin - pad, dmax + pad);
    }

    // ── Accessors ──
    int   dataXMax()   const { return m_dataXMax; }
    float dataYMin()   const { return m_dataYMin; }
    float dataYMax()   const { return m_dataYMax; }
    int   pixelW()     const { return m_pixelW; }
    int   pixelH()     const { return m_pixelH; }
    float xScale()     const { return m_xScale; }
    float yScale()     const { return m_yScale; }

    // ── Mapping ──

    /** Map a single data point to pixel Y (pixel X = dataIndex * xScale). */
    inline float mapY(float dataY) const {
        float py = dataY;
        if (py < m_dataYMin) py = m_dataYMin;
        if (py > m_dataYMax) py = m_dataYMax;
        return m_pixelTop + (m_dataYMax - py) * m_yScale;
    }

    /** Map all data points → PixelPoint vector (for QPainterPath). */
    std::vector<PixelPoint> map(const float* data, int len) const {
        std::vector<PixelPoint> pts;
        pts.reserve(len);
        for (int i = 0; i < len; ++i) {
            float px = m_pixelLeft + static_cast<float>(i) * m_xScale;
            float py = mapY(data[i]);
            pts.push_back({px, py});
        }
        return pts;
    }

    /**
     * @brief Aggregate per-column Y range (for bitmap renderer).
     *
     * Output spans[col].yMin/yMax for col in [0, pixelW).
     * Undefined columns have yMin > yMax (caller checks via ColSpan::valid()).
     */
    struct ColSpanResult {
        std::vector<ColSpan> spans;
        int pixelWidth;
        bool valid(int col) const {
            return col >= 0 && col < pixelWidth && spans[col].yMin <= spans[col].yMax;
        }
    };

    ColSpanResult aggregateColumns(const float* data, int len) const {
        ColSpanResult result;
        result.pixelWidth = m_pixelW;
        result.spans.assign(m_pixelW, {std::numeric_limits<int32_t>::max(),
                                        std::numeric_limits<int32_t>::min()});

        int prevPx = -1, prevY = 0;
        for (int i = 0; i < len; ++i) {
            int px = static_cast<int>(static_cast<float>(i) * m_xScale);
            if (px < 0 || px >= m_pixelW) { prevPx = -1; continue; }

            int yi = static_cast<int>(m_pixelTop + (m_dataYMax - data[i]) * m_yScale);
            if (yi < 0) yi = 0;
            if (yi >= m_pixelH) yi = m_pixelH - 1;

            if (yi < result.spans[px].yMin) result.spans[px].yMin = yi;
            if (yi > result.spans[px].yMax) result.spans[px].yMax = yi;

            // Interpolate gaps
            if (prevPx >= 0 && px > prevPx + 1) {
                int dx = px - prevPx;
                for (int step = 1; step < dx; ++step) {
                    int ix = prevPx + step;
                    int iy = prevY + (yi - prevY) * step / dx;
                    if (iy < result.spans[ix].yMin) result.spans[ix].yMin = iy;
                    if (iy > result.spans[ix].yMax) result.spans[ix].yMax = iy;
                }
            }
            prevPx = px;
            prevY  = yi;
        }
        return result;
    }

private:
    void recalc(int w, int h, float offsetX) {
        m_pixelW = w;
        m_pixelH = h;
        m_pixelLeft = offsetX;
        recalcX();
        recalcY();
    }

    void recalcX() {
        m_xScale = (m_dataXMax > 0) ? static_cast<float>(m_pixelW) / m_dataXMax : 1.0f;
    }

    void recalcY() {
        float yRange = m_dataYMax - m_dataYMin;
        m_yScale = (yRange > 0.0f) ? static_cast<float>(m_pixelH) / yRange : 1.0f;
        m_pixelTop = 0.0f;
    }

    int   m_dataXMax;
    float m_dataYMin, m_dataYMax;
    int   m_pixelW, m_pixelH;
    float m_pixelLeft;
    float m_pixelTop;
    float m_xScale;
    float m_yScale;
};

#endif // DATAMAPPER_H
