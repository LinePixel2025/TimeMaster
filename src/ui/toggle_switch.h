#pragma once

// 现代胶囊开关：替代 QSS 伪装的 QCheckBox::indicator（QSS 画不出可移动的
// 圆钮，只能整条变色）。自绘轨道 + 白色圆钮，切换带 120ms 缓动；文本在右。
// 继承 QCheckBox，toggled/isChecked/setText 等 API 完全兼容，可直接替换。

#include <QCheckBox>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QVariantAnimation>

#include "ui/design_tokens.h"

class ToggleSwitch : public QCheckBox
{
public:
    explicit ToggleSwitch(const QString &text = QString(), QWidget *parent = nullptr)
        : QCheckBox(text, parent)
    {
        setCursor(Qt::PointingHandCursor);
        m_progress = isChecked() ? 1.0 : 0.0;
        m_anim.setDuration(120);
        m_anim.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_anim, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &v) {
                    m_progress = v.toReal();
                    update();
                });
        connect(this, &QCheckBox::toggled, this, [this](bool on) {
            // 未显示时（初始化回填）直接落位，避免开机动画。
            if (!isVisible()) {
                m_progress = on ? 1.0 : 0.0;
                update();
                return;
            }
            m_anim.stop();
            m_anim.setStartValue(m_progress);
            m_anim.setEndValue(on ? 1.0 : 0.0);
            m_anim.start();
        });
    }

    QSize sizeHint() const override
    {
        const int textW = text().isEmpty() ? 0
                            : QFontMetrics(font()).horizontalAdvance(text());
        const int w = kTrackW + (textW > 0 ? kSpacing + textW : 0);
        return QSize(w, qMax(kTrackH, QFontMetrics(font()).height()) + 6);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool enabled = isEnabled();
        const qreal cy = height() / 2.0;
        const QRectF track(0.0, cy - kTrackH / 2.0, kTrackW, kTrackH);

        QColor trackColor = isChecked() ? DesignTokens::kAccent()
                                        : DesignTokens::kProgressBg();
        if (!enabled)
            trackColor.setAlpha(int(trackColor.alpha() * 0.45));
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(track, kTrackH / 2.0, kTrackH / 2.0);

        // 圆钮：白色实心，随 m_progress 在轨道内滑动。
        const qreal knobD = kTrackH - 6.0;
        const qreal travel = kTrackW - knobD - 6.0;
        const qreal kx = 3.0 + travel * m_progress;
        QColor knobColor(255, 255, 255, enabled ? 255 : 140);
        p.setBrush(knobColor);
        p.drawEllipse(QRectF(track.left() + kx, cy - knobD / 2.0, knobD, knobD));

        if (!text().isEmpty()) {
            p.setPen(enabled ? DesignTokens::kText() : DesignTokens::kTextFaint());
            const int x = kTrackW + kSpacing;
            const int avail = width() - x;
            p.drawText(QRectF(x, 0, avail, height()),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(font()).elidedText(text(), Qt::ElideRight, avail));
        }
    }

private:
    static constexpr int kTrackW = 40;
    static constexpr int kTrackH = 22;
    static constexpr int kSpacing = 10;

    QVariantAnimation m_anim;
    qreal m_progress = 0.0;
};
