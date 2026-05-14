# WaveformRenderer + WaveformWidget

Standalone Qt modules that render waveform data into images or real-time widgets. Extracted from NeuroSC-UI's `QPainterMultiChannelPlotter::drawChannel`.

## Individual component usage

### DataMapper (no Qt dependency)

```cpp
#include "DataMapper.h"

DataMapper mapper(500, -600.0f, 600.0f, 800, 250);  // xMax, yMin, yMax, pxW, pxH
auto pts = mapper.map(data, 500);                      // → vector<PixelPoint>
auto cols = mapper.aggregateColumns(data, 500);         // → ColSpanResult (per-column y-range)
```

### PathBuilder (QtGui only)

```cpp
#include "PathBuilder.h"
#include <QImage>
#include <QPainter>

QImage img(800, 250, QImage::Format_ARGB32_Premultiplied);
img.fill(Qt::black);
QPainter painter(&img);

auto pts = mapper.map(data, 500);          // from DataMapper
PathBuilder pb;
pb.setLineColor(Qt::green, 1.5f);
pb.draw(painter, pts);
```

### BitmapRenderer (QtGui only)

```cpp
#include "BitmapRenderer.h"

QImage img(800, 250, QImage::Format_ARGB32_Premultiplied);
img.fill(Qt::black);

auto cols = mapper.aggregateColumns(data, 500);   // from DataMapper
BitmapRenderer br;
br.draw(img, cols.spans, Qt::green);              // direct pixel write
```

## Requirements

- Qt 5.12+ (Core + GUI)
- C++17

## Architecture (3 layers + Facade)

```
┌─────────────────────────────────────────────────────────┐
│                   WaveformRenderer.h                     │
│                   (Facade — 向后兼容单次调用 API)          │
├──────────────┬──────────────┬────────────────────────────┤
│ DataMapper.h │ PathBuilder.h│ BitmapRenderer.h           │
│ (纯数学,     │ (QPainterPath│ (QImage::bits 像素直写,    │
│  无Qt依赖)   │  构建与绘制)  │  仅 QImage 依赖)           │
└──────────────┴──────────────┴────────────────────────────┘
```

| File | Dependencies | Description |
|------|-------------|-------------|
| `DataMapper.h` | **none (pure math)** | float[] → 像素坐标，列聚合，可嵌入任何 C++ 项目 |
| `PathBuilder.h` | QtGui (QPainterPath) | 像素坐标 → QPainterPath(moveTo+lineTo) + draw |
| `BitmapRenderer.h` | QtGui (QImage) | 像素坐标 → QImage::bits 直接写，快 8-10x |
| `WaveformRenderer.h` | 组合上面三层 | Facade，保持 `renderWaveform()` / `renderGrid()` API |
| `WaveformWidget.h` | QtWidgets | 实时显示 QWidget，线程安全数据输入 |

## Integration

Just copy the `.h` file(s) into your project and `#include`:

```cpp
// Static rendering only
#include "WaveformRenderer.h"
WaveformRenderer renderer;

// Or real-time widget
#include "WaveformWidget.h"
auto* w = new WaveformWidget(500, parent);
```

---

## 1. WaveformRenderer — Static API

### Single channel

```cpp
QImage renderWaveform(
    const float* data, int dataLen,     // input data
    int imgWidth, int imgHeight,        // output image size
    const RenderOptions& opt = {}       // see below
);
```

### Multi-channel grid

```cpp
QImage renderGrid(
    const QVector<QVector<float>>& channels,
    int gridCols,
    int imgWidth, int imgHeight,
    const RenderOptions& opt = {},
    const QVector<QString>& names = {}
);
```

### RenderOptions

| Field | Default | Description |
|-------|---------|-------------|
| `xMax` | `1000` | X-axis range `[0, xMax)`, maps to image width |
| `yMin` | `-1.0f` | Y-axis minimum (data units) |
| `yMax` | `1.0f` | Y-axis maximum (data units) |
| `lineColor` | `Qt::green` | Waveform line color (`QColor`) |
| `lineWidth` | `1.5f` | Waveform line width (pixels) |
| `bgColor` | `Qt::black` | Background color (`QColor`) |
| `gridColor` | `rgb(40,40,40)` | Grid line color |
| `showGrid` | `false` | Show grid |
| `useBitmap` | `false` | `false` → QPainterPath, `true` → pixel-bitmap (8-10x faster) |
| `gridXCount` | `8` | Grid vertical divisions |
| `gridYCount` | `4` | Grid horizontal divisions |

### Rendering modes

#### Mode 1: QPainterPath (default)

Uses `QPainterPath::moveTo/lineTo` + `QPainter::drawPath`. Suitable for single-channel or low-channel-count rendering where smooth antialiased lines are desired.

Pipeline:
```
float[] → mapToPixel(x, y) → QPainterPath(moveTo+lineTo) → drawPath → QImage
```

#### Mode 2: Bitmap (high performance)

Direct pixel writes per column, 8-10x faster than QPainterPath. Suitable for multi-channel real-time display.

Pipeline:
```
float[] → aggregate per-column yMin/yMax → vertical span fill → QImage::bits
```

---

## 2. WaveformWidget — Real-time Widget

### Architecture

```
Any thread                      UI thread (main loop)
  │                               │
  ├─ feedData(buf, len)           │  QTimer (30ms default)
  ├─ feedSample(val)              │    │
  │   (thread-safe: QMutex)      │    ├─ paintEvent
  │                               │    │   ├─ snapshot buffer → window slice
  │                               │    │   ├─ WaveformRenderer::renderWaveform()
  │                               │    │   └─ drawImage()
  │                               │    └─ restart timer
```

### Constructor

```cpp
WaveformWidget(int windowSize = 500, QWidget* parent = nullptr);
```

- `windowSize`: number of visible samples (history depth)

### Data input (thread-safe)

```cpp
void feedData(const float* data, int len);    // batch samples
void feedSample(float value);                 // single sample
void clear();                                 // clear buffer
```

### Configuration

```cpp
void start(int intervalMs = 30);    // start auto-refresh (visible only)
void stop();                        // stop auto-refresh

void setWindowSize(int samples);    // change visible window (also sets xMax)
void setYRange(float min, float max);
void setLineColor(const QColor& c);
void setBgColor(const QColor& c);
void setGridVisible(bool v);
void setLineWidth(float w);
void setUseBitmap(bool b);          // false=QPainterPath, true=bitmap
```

### Behavior

- **Auto-pause**: `paintEvent` only runs when widget is visible (`showEvent`/`hideEvent` controlled)
- **Resize debounce**: `resizeEvent` triggers a single-shot 30ms timer to avoid excessive re-renders
- **Scroll display**: retains `windowSize * 2` samples internally, always draws the latest `windowSize` points

---

## Examples

### Static: render and save to file

```cpp
#include "WaveformRenderer.h"

// Generate sample data
QVector<float> data(500);
for (int i = 0; i < 500; ++i)
    data[i] = sinf(i / 50.0f * 6.283f) * 400.0f;

// Render
WaveformRenderer renderer;
WaveformRenderer::RenderOptions opt;
opt.xMax = 500;
opt.yMin = -600;
opt.yMax = 600;
opt.lineColor = Qt::green;
opt.bgColor = Qt::black;

QImage img = renderer.renderWaveform(data, 800, 250, opt);
img.save("waveform.png");
```

### Real-time: widget in a window

```cpp
#include "WaveformWidget.h"

auto* w = new WaveformWidget(500, parent);
w->setYRange(-600, 600);
w->setLineColor(QColor(0, 255, 100));
w->start(30);          // 33 FPS refresh

// From any thread:
w->feedData(samples, count);
```

### Real-time: multi-channel grid

```cpp
const int chCount = 8;
QVector<WaveformWidget*> channels;
for (int ch = 0; ch < chCount; ++ch) {
    auto* w = new WaveformWidget(500, gridWidget);
    w->setYRange(-600, 600);
    w->setLineColor(QColor(100, 200, 255));
    w->start(30);
    channels.append(w);
}

// Data callback:
for (int ch = 0; ch < chCount; ++ch)
    channels[ch]->feedSample(sampleValues[ch]);
```

---

## Build

```bash
cd D:/painter
qmake WaveformRendererDemo.pro
make
./WaveformRendererDemo
```

The demo application:
1. Saves three sample images to `D:/painter/`: `demo_path.png`, `demo_bitmap.png`, `demo_grid.png`
2. Opens a real-time window with a single-channel waveform + 8-channel grid + controls


重构完成。D:\painter\ 现在包含 5 个头文件：
组件	依赖	可复用场景
DataMapper.h	无（纯数学）	任何 C++ 项目，嵌入嵌入式/服务端/测试
PathBuilder.h	QPainterPath	需要 Qt 绘制但不想要 Facade 的项目
BitmapRenderer.h	QImage	高性能实时渲染场景
WaveformRenderer.h	组合前三者 + QPainter	保持 renderWaveform(data, w, h) 单次调用
WaveformWidget.h	QWidget + QTimer	实时波形显示控件，线程安全
调用方代码（main.cpp、WaveformWidget.h）完全无需修改 — Facade 的 API 保持不变。如果只需要坐标映射（例如在无 Qt 的嵌入式设备上），单独 #include "DataMapper.h" 即可。


D:\painter/
├── DataMapper.h          (纯数学，零Qt依赖，~150行)
├── PathBuilder.h         (QtGui + DataMapper类型，~110行)  
├── BitmapRenderer.h      (仅QImage依赖，~100行)
├── WaveformRenderer.h    (Facade，组合三层，保持原API，~180行)
├── WaveformWidget.h      (实时显示QWidget，~130行)
├── main.cpp              (demo)
├── WaveformRendererDemo.pro
└── README.md# Wave
# Wave
