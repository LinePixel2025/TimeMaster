# CircularProgress Redesign — Apple Watch Style Ring

**Date:** 2026-07-13
**Status:** approved
**Scope:** `src/ui/stats_widget.cpp` — `CircularProgress::paintEvent()` only

## Motivation

The current donut ring (8px stroke, diagonal linear gradient) looks thin and lacks visual impact. Redesign to an Apple Watch Activity Rings style with a thick stroke, conical gradient, glowing endpoint, and centered two-line label.

## Design Spec

### Visual Parameters

| Property | Before | After |
|----------|--------|-------|
| Ring stroke | 8px | **20px** |
| Background track | Solid `#E5E7EB` | `#E5E7EB` + offset drop shadow (draw bg arc again at +3px y offset with `rgba(0,0,0,0.06)`) |
| Progress arc gradient | Diagonal linear `#818CF8` → `#6366F1` | **`QConicalGradient`** centered on ring, angle 90°, `#A5B4FC` (0.0) → `#818CF8` (0.5) → `#6366F1` (1.0) |
| Arc endpoint | RoundCap (8px) | **RoundCap (20px)** + glowing dot: `QRadialGradient` 8px radius, center `#C7D2FE` fading to transparent |
| Center text | `5` (22pt Light) + `30分钟` (14pt) | Top: `5h 30m` (18pt Medium, `#1F2937`), Bottom: `今日专注` (11pt, `#6B7280`) |

### Widget Size

Unchanged: **140×140** fixed size. Compatible with parent `GlassCard`.

### Arc Positioning

- Arc rect: `QRectF(10, 10, 120, 120)` — centered with 10px margin to accommodate 20px stroke
- Start angle: 90° × 16 (12 o'clock position)
- Span: `-ratio * 360 * 16` (clockwise)

### Conical Gradient

```cpp
QConicalGradient gradient(70, 70, 90);  // center (70,70), start angle 90°
gradient.setColorAt(0.0, QColor("#A5B4FC"));
gradient.setColorAt(0.5, QColor("#818CF8"));
gradient.setColorAt(1.0, QColor("#6366F1"));
```

### Glowing Endpoint

After drawing the progress arc, compute the endpoint position using trigonometry and draw a radial gradient circle:

```cpp
// Compute arc endpoint position
double angleRad = (90 - ratio * 360) * M_PI / 180.0;
double ex = 70 + 60 * cos(angleRad);
double ey = 70 - 60 * sin(angleRad);

// Glow dot
QRadialGradient glow(ex, ey, 8);
glow.setColorAt(0.0, QColor("#C7D2FE"));
glow.setColorAt(1.0, QColor(0xC7, 0xD2, 0xFE, 0));
painter.setBrush(glow);
painter.setPen(Qt::NoPen);
painter.drawEllipse(QPointF(ex, ey), 8, 8);
```

### Drop Shadow on Background Track

Draw background track once normally, then a second time at `(0, +3)` offset with low-opacity color:

```cpp
// Shadow pass
painter.save();
painter.translate(0, 3);
QPen shadowPen(QColor(0, 0, 0, 15), penWidth);  // rgba(0,0,0,0.06) ≈ 15/255
shadowPen.setCapStyle(Qt::RoundCap);
painter.setPen(shadowPen);
painter.drawArc(rect, 0, 360 * 16);
painter.restore();
```

### Center Text Layout

```cpp
// Main time: "5h 30m"
painter.setFont(appFont(18, QFont::Medium));
painter.setPen(QColor("#1F2937"));
painter.drawText(QRectF(0, 38, 140, 30), Qt::AlignCenter, timeStr);

// Subtitle: "今日专注"
painter.setFont(appFont(11));
painter.setPen(QColor("#6B7280"));
painter.drawText(QRectF(0, 72, 140, 20), Qt::AlignCenter, "今日专注");
```

## Implementation

- File: `src/ui/stats_widget.cpp`
- Modify: `CircularProgress::paintEvent()` (currently lines 20–68)
- No changes to `stats_widget.h`, `main_window.cpp`, or any other file
- Max value remains 43200 (12h)
- `setValue()` method unchanged

## Verification

1. Build: `cmake --build build`
2. Run: `.\run.ps1` (temporarily add `window.show()` in `main.cpp`)
3. Visual check: ring is thick, conical gradient visible, endpoint glows, shadow under ring, center text is two-line
4. Revert `main.cpp` before commit
