#pragma once

// 设置界面线性图标：程序内用 QPainter 按当前主题色实时绘制，无外部资源文件。
// 与 DesignTokens 保持一致：亮/暗主题自动取不同描边色；HiDPI 下按 2x 渲染保证清晰。
// 新增图标只需在 enum 与 detail::drawIcon 中扩展 switch 分支。

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

#include "ui/design_tokens.h"

namespace SettingsIcons {

enum Kind {
    Gear,     // 设置入口（主窗口）
    Apps,     // 应用管理
    Timer,    // 追踪设置
    Palette,  // 个性化
    Bell,     // 提醒
    Cloud,    // 云端同步
    Sparkle,  // AI 智能
    Info,     // 关于
};

namespace detail {

// 在 r（逻辑像素区域）内以描边方式绘制线性图标；画笔已配置好颜色与线宽。
inline void drawIcon(QPainter &p, Kind kind, const QRectF &r)
{
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal s = r.width();

    switch (kind) {
    case Gear: {
        // 双圆 + 6 条齿线，简化的齿轮轮廓
        p.drawEllipse(QPointF(cx, cy), s * 0.36, s * 0.36);
        p.drawEllipse(QPointF(cx, cy), s * 0.13, s * 0.13);
        for (int i = 0; i < 6; ++i) {
            const qreal a = qDegreesToRadians(60.0 * i);
            const QPointF d(qCos(a), qSin(a));
            p.drawLine(QPointF(cx, cy) + d * (s * 0.33),
                       QPointF(cx, cy) + d * (s * 0.46));
        }
        break;
    }
    case Apps: {
        // 四宫格：2x2 圆角方块
        const qreal cell = s * 0.34;
        const qreal gap = s * 0.16;
        const qreal x0 = r.left() + (s - cell * 2.0 - gap) / 2.0;
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                const QRectF rc(x0 + col * (cell + gap),
                                x0 + row * (cell + gap), cell, cell);
                p.drawRoundedRect(rc, 1.8, 1.8);
            }
        }
        break;
    }
    case Timer: {
        // 秒表：顶部按钮 + 表盘 + 指针
        p.drawLine(QPointF(cx - s * 0.18, r.top() + 1.0),
                   QPointF(cx - s * 0.06, r.top() + 1.0));
        p.drawLine(QPointF(cx + s * 0.06, r.top() + 1.0),
                   QPointF(cx + s * 0.18, r.top() + 1.0));
        p.drawLine(QPointF(cx, r.top() + 1.0),
                   QPointF(cx, r.top() + 1.0 + s * 0.06 + 1.8));
        const QPointF center(cx, r.top() + 1.0 + s * 0.56);
        const qreal radius = s * 0.32;
        p.drawEllipse(center, radius, radius);
        p.drawLine(center, QPointF(center.x() + radius * 0.72,
                                   center.y() - radius * 0.55));
        break;
    }
    case Palette: {
        // 调色板：右下切角的圆 + 拇指孔 + 3 个颜料点
        const QRectF circle(r.left() + s * 0.14, r.top() + s * 0.14,
                            s * 0.72, s * 0.72);
        QPainterPath path;
        path.arcMoveTo(circle, 45);
        path.arcTo(circle, 45, 270);
        path.lineTo(cx + s * 0.20, cy + s * 0.06);
        path.lineTo(cx + s * 0.28, cy + s * 0.30);
        path.closeSubpath();
        p.drawPath(path);
        p.drawEllipse(QPointF(cx - s * 0.10, cy - s * 0.06),
                      s * 0.12, s * 0.12);
        const QColor base = p.pen().color();
        p.setBrush(base);
        const QPointF dots[] = {
            QPointF(cx - s * 0.30, cy - s * 0.22),
            QPointF(cx + s * 0.24, cy - s * 0.30),
            QPointF(cx + s * 0.02, cy + s * 0.36),
        };
        for (const QPointF &d : dots)
            p.drawEllipse(d, 1.0, 1.0);
        p.setBrush(Qt::NoBrush);
        break;
    }
    case Bell: {
        // 铃铛：上半圆弧 + 两侧竖线 + 底部横线，顶部圆钮
        QPainterPath path;
        const QRectF body(r.left() + s * 0.12, r.top() + s * 0.08,
                          s * 0.76, s * 0.62);
        path.arcMoveTo(body, 180);
        path.arcTo(body, 180, 180);
        path.lineTo(r.right() - s * 0.12, r.bottom() - s * 0.18);
        path.lineTo(r.left() + s * 0.12, r.bottom() - s * 0.18);
        path.closeSubpath();
        p.drawPath(path);
        p.drawEllipse(QPointF(cx, r.top() + s * 0.08), 1.2, 1.2);
        break;
    }
    case Cloud: {
        // 云朵：底部圆角矩形与顶部圆 union 出平滑外轮廓
        QPainterPath cloud;
        cloud.addRoundedRect(QRectF(r.left() + s * 0.14, r.top() + s * 0.46,
                                    s * 0.72, s * 0.30), 2.4, 2.4);
        QPainterPath puff;
        puff.addEllipse(QRectF(r.left() + s * 0.24, r.top() + s * 0.08,
                               s * 0.52, s * 0.52));
        cloud = cloud.united(puff);
        p.drawPath(cloud);
        break;
    }
    case Sparkle: {
        // 四角星（菱形星）
        QPainterPath path;
        path.moveTo(cx, r.top() + 1.2);
        path.lineTo(cx + s * 0.11, cy - s * 0.09);
        path.lineTo(r.right() - 1.2, cy);
        path.lineTo(cx + s * 0.11, cy + s * 0.09);
        path.lineTo(cx, r.bottom() - 1.2);
        path.lineTo(cx - s * 0.11, cy + s * 0.09);
        path.lineTo(r.left() + 1.2, cy);
        path.lineTo(cx - s * 0.11, cy - s * 0.09);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Info: {
        // 信息：圆圈 + 顶部圆点 + 竖线
        p.drawEllipse(QPointF(cx, cy), s * 0.40, s * 0.40);
        const QColor base = p.pen().color();
        p.setBrush(base);
        p.drawEllipse(QPointF(cx, cy - s * 0.24), 1.0, 1.0);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(cx, cy - s * 0.10), QPointF(cx, cy + s * 0.26));
        break;
    }
    }
}

} // namespace detail

// 生成指定逻辑尺寸的线性图标。color 为空时按当前主题取 kTextMute()。
// Normal 状态用指定颜色，Disabled 状态用淡色（配合开关联动置灰）。
inline QIcon icon(Kind kind, int size = 18, const QColor &color = QColor())
{
    const QColor c = color.isValid() ? color : DesignTokens::kTextMute();
    const int px = size * 2;

    auto render = [&](const QColor &pen) {
        QPixmap pm(px, px);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(pen, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        detail::drawIcon(p, kind, QRectF(1.2, 1.2, size - 2.4, size - 2.4));
        p.end();
        return pm;
    };

    QIcon ic;
    ic.addPixmap(render(c));
    QColor faded = c;
    faded.setAlpha(120);
    ic.addPixmap(render(faded), QIcon::Disabled);
    return ic;
}

// 侧边栏导航图标：选中态用主题色，未选中态用次要文字色。
inline QIcon navIcon(Kind kind, bool selected)
{
    return icon(kind, 17, selected ? DesignTokens::kAccent() : DesignTokens::kTextMute());
}

} // namespace SettingsIcons
