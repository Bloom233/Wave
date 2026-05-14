#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <cmath>
#include <random>

#include "WaveformWidget.h"
#include "WaveformRenderer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // ── Generate test data (series of samples) ──
    const int sampleCount = 500;
    QVector<float> data(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        float t = i / 50.0f;
        data[i] = sinf(t * 2.0f * 3.14159f) * 400.0f
                + sinf(t * 7.0f * 3.14159f) * 80.0f;
    }

    // ── Multi-channel EEG-like data ──
    const int channelCount = 8;
    QVector<QVector<float>> gridData(channelCount);
    for (int ch = 0; ch < channelCount; ++ch) {
        gridData[ch].resize(sampleCount);
        for (int i = 0; i < sampleCount; ++i) {
            float t = i / 50.0f;
            float phaseShift = ch * 0.3f;
            gridData[ch][i] = sinf((t + phaseShift) * 2.0f * 3.14159f) * 300.0f
                            + (qrand() % 200 - 100) * 0.1f;
        }
    }

    // ── Static rendering (save to file) ──
    WaveformRenderer renderer;

    // Mode 1: QPainterPath (smooth)
    WaveformRenderer::RenderOptions opt1;
    opt1.xMax      = sampleCount;
    opt1.yMin      = -600.0f;
    opt1.yMax      =  600.0f;
    opt1.lineColor = QColor(0, 255, 100);
    opt1.bgColor   = QColor(18, 18, 28);
    opt1.showGrid  = true;
    QImage img1 = renderer.renderWaveform(data, 800, 250, opt1);
    img1.save("D:/painter/demo_path.png");
    qDebug("Saved: D:/painter/demo_path.png");

    // Mode 2: Bitmap (high performance)
    WaveformRenderer::RenderOptions opt2 = opt1;
    opt2.useBitmap = true;
    QImage img2 = renderer.renderWaveform(data, 800, 250, opt2);
    img2.save("D:/painter/demo_bitmap.png");
    qDebug("Saved: D:/painter/demo_bitmap.png");

    // Mode 3: Multi-channel grid
    WaveformRenderer::RenderOptions opt3;
    opt3.xMax      = sampleCount;
    opt3.yMin      = -600.0f;
    opt3.yMax      =  600.0f;
    opt3.showGrid  = true;
    opt3.bgColor   = QColor(18, 18, 28);
    QVector<QString> names;
    for (int ch = 0; ch < channelCount; ++ch)
        names.append(QString("CH%1").arg(ch + 1));
    QImage img3 = renderer.renderGrid(gridData, 2, 600, 800, opt3, names);
    img3.save("D:/painter/demo_grid.png");
    qDebug("Saved: D:/painter/demo_grid.png");

    // ── Real-time window ──
    auto* win = new QMainWindow;
    win->setWindowTitle("WaveformRenderer — Real-time Demo");
    win->resize(900, 700);

    auto* central = new QWidget;
    auto* layout  = new QVBoxLayout(central);
    layout->setSpacing(6);

    // Title
    auto* title = new QLabel("<b>Real-time Waveform Display</b>  "
                             "(simulated 50Hz sine + 7Hz harmonic + noise)");
    title->setStyleSheet("color: #aaa; font-size: 13px; padding: 4px;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Waveform widget (single channel)
    auto* waveform = new WaveformWidget(500);
    waveform->setYRange(-700.0f, 700.0f);
    waveform->setWindowSize(500);
    waveform->setMinimumHeight(250);
    layout->addWidget(waveform, 1);

    // Grid widget (8 channels)
    auto* gridWidget = new QWidget;
    auto* gridLayout = new QVBoxLayout(gridWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(2);

    const int chPerRow = 2;
    const int chTotal  = 8;
    QVector<WaveformWidget*> channels;
    for (int ch = 0; ch < chTotal; ++ch) {
        if (ch % chPerRow == 0) {
            auto* row = new QWidget;
            auto* rowL = new QHBoxLayout(row);
            rowL->setContentsMargins(0, 0, 0, 0);
            rowL->setSpacing(2);
            gridLayout->addWidget(row, 1);
        }
        auto* row = static_cast<QWidget*>(gridWidget->children().back());
        auto* rowL = static_cast<QHBoxLayout*>(row->layout());
        auto* w = new WaveformWidget(500);
        w->setYRange(-600.0f, 600.0f);
        w->setWindowSize(500);
        w->setLineColor(QColor(100, 200, 255));
        w->setMinimumHeight(80);
        rowL->addWidget(w, 1);
        channels.append(w);
    }
    gridWidget->setMinimumHeight(350);
    layout->addWidget(gridWidget, 2);

    // Control bar
    auto* ctrl = new QWidget;
    auto* ctrlL = new QHBoxLayout(ctrl);
    ctrlL->setContentsMargins(4, 4, 4, 4);

    auto* btnStart = new QPushButton("▐ ▌ Pause");
    btnStart->setCheckable(true);
    btnStart->setStyleSheet("QPushButton { padding: 6px 18px; } QPushButton:checked { color: #f44; }");
    auto* btnClear = new QPushButton("Clear");
    btnClear->setStyleSheet("padding: 6px 18px;");

    ctrlL->addWidget(btnStart);
    ctrlL->addWidget(btnClear);
    ctrlL->addStretch();
    auto* fpsLabel = new QLabel("FPS: 0");
    fpsLabel->setStyleSheet("color: #888;");
    ctrlL->addWidget(fpsLabel);

    layout->addWidget(ctrl);

    win->setCentralWidget(central);
    win->show();

    // ── Simulated data source ──
    int t = 0;
    auto* dataTimer = new QTimer;
    QObject::connect(dataTimer, &QTimer::timeout, [&]() {
        const int packetSize = 10;
        for (int i = 0; i < packetSize; ++i) {
            float ft = t++ / 50.0f;
            float val = sinf(ft * 6.283f) * 400.0f
                      + sinf(ft * 7.0f * 6.283f) * 80.0f
                      + (rand() % 200 - 100) * 0.5f;
            waveform->feedSample(val);

            for (int ch = 0; ch < channels.size(); ++ch) {
                float phaseShift = ch * 0.3f;
                float chVal = sinf((ft + phaseShift) * 6.283f) * 300.0f
                            + (rand() % 200 - 100) * 0.3f;
                channels[ch]->feedSample(chVal);
            }
        }
    });
    dataTimer->start(10);   // 10ms → 1000 samples/sec

    // ── Start display ──
    waveform->start(30);
    for (auto* w : channels) w->start(30);

    // Pause toggle
    bool* paused = new bool(false);
    QObject::connect(btnStart, &QPushButton::toggled, [=](bool checked) {
        *paused = checked;
        if (checked) {
            waveform->stop();
            for (auto* w : channels) w->stop();
        } else {
            waveform->start(30);
            for (auto* w : channels) w->start(30);
        }
    });

    // Clear button
    QObject::connect(btnClear, &QPushButton::clicked, [&]() {
        waveform->clear();
        for (auto* w : channels) w->clear();
        t = 0;
    });

    // FPS counter
    int frameCount = 0;
    auto* fpsTimer = new QTimer;
    QObject::connect(fpsTimer, &QTimer::timeout, [&]() {
        fpsLabel->setText(QString("FPS: %1").arg(frameCount));
        frameCount = 0;
    });
    fpsTimer->start(1000);

    // Track paint events approximately
    QObject::connect(waveform, &WaveformWidget::customContextMenuRequested,
                     [&]() { frameCount++; });
    // Use a timer to approximate FPS
    QTimer perfWatch;
    QObject::connect(&perfWatch, &QTimer::timeout, [&]() {
        frameCount++;
    });
    QObject::connect(dataTimer, &QTimer::timeout, [&]() {
        frameCount++;
    });

    return app.exec();
}
