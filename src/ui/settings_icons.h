#pragma once

// 设置界面线性图标：程序内用 QPainter 按当前主题色实时绘制，无外部资源文件。
// 设计约定（2026-09 重绘）：统一 24px 视觉网格、1.5px 描边、圆头圆角连接、
// 四周留白 ≥2px；几何尽量精简（单闭合形 + 少量点缀），与 DesignTokens 一致：
// 亮/暗主题自动取不同描边色；HiDPI 下按 2x 渲染保证清晰。
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
    Palette,  // 个性化（对比圆：外观主题的极简表达）
    Bell,     // 提醒
    Cloud,    // 云端同步
    Sparkle,  // AI 智能
    Info,     // 关于
    Sun,      // 切换到浅色
    Moon,     // 切换到暗色
    More,     // 顶栏更多菜单
};

namespace detail {

// 在 r（逻辑像素区域）内以描边方式绘制线性图标；画笔已配置好颜色与线宽。
inline void drawIcon(QPainter &p, Kind kind, const QRectF &r)
{
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal s = r.width();
    const QPointF C(cx, cy);
    const QColor base = p.pen().color();

    switch (kind) {
    case Gear: {
        // 齿轮：外圈 + 中心孔 + 6 枚与圈相接的粗短齿。
        // 8 细齿在 18px 下会读成花朵/太阳；6 齿且起笔压在圈上才有齿轮剪影。
        const qreal ring = s * 0.30;
        QPen toothPen = p.pen();
        toothPen.setWidthF(2.0);
        p.drawEllipse(C, ring, ring);
        p.drawEllipse(C, ring * 0.34, ring * 0.34);
        for (int i = 0; i < 6; ++i) {
            const qreal a = qDegreesToRadians(60.0 * i - 90.0);
            const QPointF d(qCos(a), qSin(a));
            p.setPen(toothPen);
            p.drawLine(C + d * ring, C + d * (s * 0.465));
        }
        p.setPen(toothPen); // 保持线宽一致由调用方重设，这里恢复描边宽度
        break;
    }
    case Apps: {
        // 四宫格：2x2 圆角方块，等距留缝
        const qreal cell = s * 0.33;
        const qreal gap = s * 0.15;
        const qreal x0 = r.left() + (s - cell * 2.0 - gap) / 2.0;
        const qreal y0 = r.top() + (s - cell * 2.0 - gap) / 2.0;
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                const QRectF rc(x0 + col * (cell + gap),
                                y0 + row * (cell + gap), cell, cell);
                p.drawRoundedRect(rc, 2.2, 2.2);
            }
        }
        break;
    }
    case Timer: {
        // 秒表：顶部横键 + 表冠 + 表盘 + 指针
        p.drawLine(QPointF(cx - s * 0.14, r.top() + 1.4),
                   QPointF(cx + s * 0.14, r.top() + 1.4));
        p.drawLine(QPointF(cx, r.top() + 1.4),
                   QPointF(cx, r.top() + 1.4 + s * 0.10));
        const QRectF dial(cx - s * 0.30, r.top() + 1.4 + s * 0.10,
                          s * 0.60, s * 0.60);
        p.drawEllipse(dial);
        p.drawLine(dial.center(),
                   QPointF(dial.center().x() + dial.width() * 0.20,
                           dial.center().y() - dial.height() * 0.16));
        break;
    }
    case Palette: {
        // 对比圆：整圈描边 + 右半实心，极简表达「外观/主题」
        const qreal rad = s * 0.34;
        p.drawEllipse(C, rad, rad);
        QPainterPath half;
        half.moveTo(cx, cy - rad);
        half.arcTo(QRectF(cx - rad, cy - rad, rad * 2, rad * 2), 90, -180);
        half.closeSubpath();
        p.fillPath(half, base);
        break;
    }
    case Bell: {
        // 铃铛：圆顶 + 外撇底沿 + 下方摆锤弧，顶部小钮
        QPainterPath bell;
        const QRectF dome(cx - s * 0.25, r.top() + s * 0.16, s * 0.50, s * 0.50);
        bell.moveTo(dome.left(), r.top() + s * 0.60);
        bell.arcTo(dome, 180, 180);
        bell.lineTo(r.right() - s * 0.25, r.top() + s * 0.60);
        bell.lineTo(r.right() - s * 0.17, r.top() + s * 0.68);
        bell.lineTo(r.left() + s * 0.17, r.top() + s * 0.68);
        bell.closeSubpath();
        p.drawPath(bell);
        p.drawEllipse(QPointF(cx, r.top() + s * 0.13), 1.1, 1.1);
        p.drawArc(QRectF(cx - s * 0.09, r.top() + s * 0.62, s * 0.18, s * 0.16),
                  200 * 16, 140 * 16);
        break;
    }
    case Cloud: {
        // 云朵：底部圆角横条与顶部圆 union 出平滑外轮廓
        QPainterPath cloud;
        cloud.addRoundedRect(QRectF(r.left() + s * 0.12, r.top() + s * 0.46,
                                    s * 0.76, s * 0.28), 2.6, 2.6);
        QPainterPath puff;
        puff.addEllipse(QRectF(r.left() + s * 0.22, r.top() + s * 0.10,
                               s * 0.56, s * 0.56));
        cloud = cloud.united(puff);
        p.drawPath(cloud);
        break;
    }
    case Sparkle: {
        // AI 四芒星：四条凹弧收腰 + 右上角小伴星
        const qreal tip = s * 0.42;
        QPainterPath star;
        star.moveTo(cx, cy - tip);
        star.quadTo(C, QPointF(cx + tip, cy));
        star.quadTo(C, QPointF(cx, cy + tip));
        star.quadTo(C, QPointF(cx - tip, cy));
        star.quadTo(C, QPointF(cx, cy - tip));
        p.drawPath(star);
        p.setBrush(base);
        p.drawEllipse(QPointF(r.right() - s * 0.08, r.top() + s * 0.12), 1.1, 1.1);
        p.setBrush(Qt::NoBrush);
        break;
    }
    case Info: {
        // 信息：圆圈 + 顶部圆点 + 竖线
        p.drawEllipse(C, s * 0.38, s * 0.38);
        p.setBrush(base);
        p.drawEllipse(QPointF(cx, cy - s * 0.20), 1.0, 1.0);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(cx, cy - s * 0.06), QPointF(cx, cy + s * 0.24));
        break;
    }
    case Sun: {
        // 太阳：实心感小核 + 8 道分离短射线（射线留缝，与齿轮区分）
        p.drawEllipse(C, s * 0.19, s * 0.19);
        for (int i = 0; i < 8; ++i) {
            const qreal a = qDegreesToRadians(45.0 * i);
            const QPointF d(qCos(a), qSin(a));
            p.drawLine(C + d * (s * 0.30), C + d * (s * 0.42));
        }
        break;
    }
    case Moon: {
        // 月亮：双圆相离的开口弯月（切圆右移量小一些，保证月牙厚度）
        QPainterPath moon;
        moon.addEllipse(QRectF(cx - s * 0.26, cy - s * 0.30, s * 0.52, s * 0.60));
        QPainterPath cut;
        cut.addEllipse(QRectF(cx - s * 0.02, cy - s * 0.30, s * 0.42, s * 0.54));
        p.drawPath(moon.subtracted(cut));
        break;
    }
    case More: {
        // 更多：水平三点
        p.setBrush(base);
        for (int i = -1; i <= 1; ++i)
            p.drawEllipse(QPointF(cx + i * s * 0.20, cy), 1.4, 1.4);
        p.setBrush(Qt::NoBrush);
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
        p.setPen(QPen(pen, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
