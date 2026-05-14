#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QMutex>
#include <QVector>
#include <deque>
#include "WaveformRenderer.h"

/**
 * @brief Real-time waveform display widget.
 *
 * Continuously renders incoming data as a scrolling waveform.
 * Thread-safe: call feedData() from any thread; rendering runs on UI thread.
 *
 * Usage:
 * @code
 *   auto* w = new WaveformWidget(500, this);   // 500-point window
 *   w->setYRange(-600, 600);
 *   w->start(30);                                // 30ms refresh
 *
 *   // from data acquisition thread:
 *   w->feedData(samples, count);
 * @endcode
 */
class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    /**
     * @param windowSize  Number of visible samples (history depth)
     * @param parent      Qt parent widget
     */
    explicit WaveformWidget(int windowSize = 500, QWidget* parent = nullptr)
        : QWidget(parent), m_windowSize(windowSize)
    {
        setMinimumSize(200, 80);
        setMouseTracking(true);

        m_renderOpt.xMax      = m_windowSize;
        m_renderOpt.yMin      = -1.0f;
        m_renderOpt.yMax      =  1.0f;
        m_renderOpt.lineColor = QColor(0, 255, 100);
        m_renderOpt.bgColor   = QColor(18, 18, 28);
        m_renderOpt.gridColor = QColor(40, 40, 50);
        m_renderOpt.showGrid  = true;
        m_renderOpt.gridXCount = 8;
        m_renderOpt.gridYCount = 4;
        m_renderOpt.lineWidth = 1.5f;

        m_refreshTimer.setSingleShot(true);
        connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
            if (isVisible()) update();
        });
    }

    /** Start auto-refresh with given interval in ms (typical: 30-50). */
    void start(int intervalMs = 30) {
        m_intervalMs = intervalMs;
        m_running = true;
        update();
    }
    void stop()  { m_running = false; }

    // ── Configuration ──
    void setWindowSize(int samples) { m_windowSize = samples; m_renderOpt.xMax = samples; }
    void setYRange(float min, float max) { m_renderOpt.yMin = min; m_renderOpt.yMax = max; }
    void setLineColor(const QColor& c)   { m_renderOpt.lineColor = c; }
    void setBgColor(const QColor& c)     { m_renderOpt.bgColor = c; }
    void setGridVisible(bool v)          { m_renderOpt.showGrid = v; }
    void setLineWidth(float w)           { m_renderOpt.lineWidth = w; }
    void setUseBitmap(bool b)            { m_renderOpt.useBitmap = b; }

    /** Feed new samples into the buffer. Thread-safe. */
    void feedData(const float* data, int len) {
        QMutexLocker lock(&m_mutex);
        for (int i = 0; i < len; ++i)
            m_buffer.push_back(data[i]);
        // Trim to window size
        while (static_cast<int>(m_buffer.size()) > m_windowSize * 2)
            m_buffer.pop_front();
    }

    /** Feed a single sample. Thread-safe. */
    void feedSample(float value) {
        QMutexLocker lock(&m_mutex);
        m_buffer.push_back(value);
        while (static_cast<int>(m_buffer.size()) > m_windowSize * 2)
            m_buffer.pop_front();
    }

    /** Clear all buffered data. */
    void clear() { QMutexLocker lock(&m_mutex); m_buffer.clear(); }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_running) return;

        // Grab latest window-sized slice
        QVector<float> slice;
        {
            QMutexLocker lock(&m_mutex);
            int sz = static_cast<int>(m_buffer.size());
            int start = std::max(0, sz - m_windowSize);
            slice.resize(m_windowSize);
            for (int i = 0; i < m_windowSize; ++i) {
                int srcIdx = start + i;
                slice[i] = (srcIdx < sz) ? m_buffer[srcIdx] : 0.0f;
            }
        }

        // Render to image at widget size
        int w = width();
        int h = height();
        if (w <= 0 || h <= 0) return;

        WaveformRenderer::RenderOptions opt = m_renderOpt;
        // Adjust xMax to match actual window size for display
        opt.xMax = m_windowSize;

        QImage frame = m_renderer.renderWaveform(slice, w, h, opt);

        // Draw
        QPainter painter(this);
        painter.drawImage(rect(), frame);

        // Schedule next refresh
        m_refreshTimer.start(m_intervalMs);
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        m_refreshTimer.start(m_intervalMs);
    }

    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        if (m_running) m_refreshTimer.start(m_intervalMs);
    }

    void hideEvent(QHideEvent* e) override {
        QWidget::hideEvent(e);
        m_refreshTimer.stop();
    }

private:
    WaveformRenderer m_renderer;
    WaveformRenderer::RenderOptions m_renderOpt;

    int  m_windowSize = 500;
    int  m_intervalMs = 30;
    bool m_running = false;

    // Thread-safe circular buffer
    QMutex m_mutex;
    std::deque<float> m_buffer;

    QTimer m_refreshTimer;
};

#endif // WAVEFORMWIDGET_H
