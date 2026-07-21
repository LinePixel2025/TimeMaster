# CircularProgress Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign `CircularProgress` ring to Apple Watch Activity Rings style — thick 20px stroke, conical gradient, glowing endpoint, drop shadow, two-line center label.

**Architecture:** Single-method rewrite of `CircularProgress::paintEvent()` in `stats_widget.cpp`. Uses `QConicalGradient` for arc color, `QRadialGradient` for endpoint glow, and `QPainter::save/restore` with `translate` for the drop shadow trick.

**Tech Stack:** Qt 6 Widgets (`QPainter`, `QConicalGradient`, `QRadialGradient`), C++17, Microsoft YaHei font.

## Global Constraints

- Font: `Microsoft YaHei` via local `appFont()` helper
- Background: solid `#F0F2F5`, no transparency/translucency
- Widget size: fixed 140×140, unchanged
- Max value: 43200 seconds (12h), unchanged
- `setValue()` method: unchanged
- Only modify `stats_widget.cpp`, no header changes

---

### Task 1: Rewrite CircularProgress::paintEvent with Apple Watch ring style

**Files:**
- Modify: `src/ui/stats_widget.cpp:5-68`

**Interfaces:**
- Consumes: `CircularProgress::m_value`, `m_maxValue`, `m_hours`, `m_minutes` (existing members)
- Produces: Same visual interface — `paintEvent` renders updated ring

- [ ] **Step 1: Add required includes**

At the top of `stats_widget.cpp`, add `QConicalGradient` and `QRadialGradient` to the existing include block. Change lines 5-7 from:

```cpp
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
```

to:

```cpp
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QRadialGradient>
```

- [ ] **Step 2: Replace the entire paintEvent method (lines 35-68)**

Replace the current `CircularProgress::paintEvent` with:

```cpp
void CircularProgress::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect(10, 10, 120, 120);
    double penWidth = 20;
    double ratio = static_cast<double>(m_value) / m_maxValue;

    // --- Drop shadow under background track ---
    painter.save();
    painter.translate(0, 3);
    QPen shadowPen(QColor(0, 0, 0, 15), penWidth);
    shadowPen.setCapStyle(Qt::RoundCap);
    painter.setPen(shadowPen);
    painter.drawArc(rect, 0, 360 * 16);
    painter.restore();

    // --- Background track ---
    QPen bgPen(QColor("#E5E7EB"), penWidth);
    bgPen.setCapStyle(Qt::RoundCap);
    painter.setPen(bgPen);
    painter.drawArc(rect, 0, 360 * 16);

    // --- Progress arc with conical gradient ---
    QConicalGradient gradient(70, 70, 90);
    gradient.setColorAt(0.0, QColor("#A5B4FC"));
    gradient.setColorAt(0.5, QColor("#818CF8"));
    gradient.setColorAt(1.0, QColor("#6366F1"));
    QPen fgPen(QBrush(gradient), penWidth);
    fgPen.setCapStyle(Qt::RoundCap);
    painter.setPen(fgPen);
    int span = static_cast<int>(-ratio * 360 * 16);
    painter.drawArc(rect, 90 * 16, span);

    // --- Glowing endpoint dot ---
    if (ratio > 0.0) {
        double angleDeg = 90.0 - ratio * 360.0;
        double angleRad = angleDeg * M_PI / 180.0;
        double ex = 70.0 + 60.0 * cos(angleRad);
        double ey = 70.0 - 60.0 * sin(angleRad);

        QRadialGradient glow(ex, ey, 8);
        glow.setColorAt(0.0, QColor("#C7D2FE"));
        glow.setColorAt(1.0, QColor(0xC7, 0xD2, 0xFE, 0));
        painter.setBrush(glow);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(ex, ey), 8, 8);
    }

    // --- Center text: time ---
    QString timeStr;
    if (m_hours > 0)
        timeStr = QString("%1h %2m").arg(m_hours).arg(m_minutes);
    else
        timeStr = QString("%1m").arg(m_minutes);

    painter.setFont(appFont(18, QFont::Medium));
    painter.setPen(QColor("#1F2937"));
    painter.drawText(QRectF(0, 38, 140, 30), Qt::AlignCenter, timeStr);

    // --- Center text: subtitle ---
    painter.setFont(appFont(11));
    painter.setPen(QColor("#6B7280"));
    painter.drawText(QRectF(0, 72, 140, 20), Qt::AlignCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe4\xb8\x93\xe6\xb3\xa8"));
}
```

- [ ] **Step 3: Build**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: BUILD SUCCESS, no errors.

- [ ] **Step 4: Visual verification**

Add `window.show()` to `main.cpp` temporarily, build, run:

```powershell
.\run.ps1
```

Verify visually:
1. Ring stroke is thick (20px)
2. Progress arc has visible color gradient along the curve
3. Arc endpoint has a glowing dot (soft blue circle)
4. Shadow is visible under the background ring (3px offset)
5. Center text is two-line: time on top, "今日专注" on bottom
6. "今日专注" shows correctly (not garbled)

Revert `main.cpp` after verification.

- [ ] **Step 5: Commit**

```bash
git add src/ui/stats_widget.cpp
git commit -m "feat: redesign CircularProgress to Apple Watch ring style"
```
