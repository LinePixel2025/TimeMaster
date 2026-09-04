#include "ui/title_bar.h"

#include "ui/design_tokens.h"
#include "ui/settings_icons.h"
#include "ui/ui_utils.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QPainter>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QImage>
#include <QPixmap>
#include <QRegion>

namespace {
/// 底部渐变淡出带高度：卡片滑入标题栏后在此区间淡出到窗口底色。
constexpr int kFadeBand = DesignTokens::kSpacingLg;
} // namespace

TitleBar::TitleBar(QScrollArea *scrollArea, QWidget *parent)
    : QWidget(parent), m_scrollArea(scrollArea)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(DesignTokens::kOuterMargin,
                            DesignTokens::kWindowTopMargin,
                            DesignTokens::kOuterMargin,
                            kFadeBand);
    row->setSpacing(DesignTokens::kHeaderSpacing);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(DesignTokens::kTitleStackSpacing);
    m_titleLabel = new QLabel("Time Master", this);
    m_titleLabel->setFont(DesignTokens::appFont(20, QFont::DemiBold));
    titleColumn->addWidget(m_titleLabel);
    m_dateLabel = new QLabel(QString(), this);
    m_dateLabel->setFont(DesignTokens::appFont(11));
    titleColumn->addWidget(m_dateLabel);
    row->addLayout(titleColumn);
    row->addStretch();

    m_statusChip = new QLabel(QStringLiteral("空闲"), this);
    m_statusChip->setObjectName(QStringLiteral("statusChip"));
    m_statusChip->setFont(DesignTokens::appFont(11, QFont::Medium));
    m_statusChip->setAlignment(Qt::AlignVCenter);
    m_statusChip->setMaximumWidth(DesignTokens::kStatusChipMaxWidth);
    m_statusChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    row->addWidget(m_statusChip);

    m_themeBtn = new QPushButton(this);
    m_themeBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_themeBtn, &QPushButton::clicked, this, &TitleBar::themeButtonClicked);
    row->addWidget(m_themeBtn);

    m_moreBtn = new QPushButton(this);
    m_moreBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setToolTip(QStringLiteral("更多"));
    connect(m_moreBtn, &QPushButton::clicked, this, &TitleBar::moreButtonClicked);
    row->addWidget(m_moreBtn);

    m_settingsBtn = new QPushButton(this);
    m_settingsBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip(QStringLiteral("设置"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &TitleBar::settingsButtonClicked);
    row->addWidget(m_settingsBtn);

    m_statusFg = DesignTokens::kAccent();
    m_statusBg = DesignTokens::kAccentLight();

    // overlay 必须有不透明底：真实窗口里若 paintEvent 的磨砂层被视口内容盖住，
    // 至少底色保证不穿透；autoFillBackground 让 Qt 在 paintEvent 前先铺底色。
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, DesignTokens::kBg());
    setPalette(pal);
}

int TitleBar::desiredHeight()
{
    return DesignTokens::kWindowTopMargin + DesignTokens::kIconButtonSize + kFadeBand;
}

void TitleBar::setDateText(const QString &text)
{
    if (m_dateLabel)
        m_dateLabel->setText(text);
}

void TitleBar::setStatus(const QString &text, const QColor &fg, const QColor &bg)
{
    m_statusText = text;
    m_statusFg = fg;
    m_statusBg = bg;
    refreshStatus();
}

void TitleBar::refreshStatus()
{
    if (!m_statusChip)
        return;
    // 按固定最大宽度上限省略：若按当前 contentsRect 省略，芯片会因 sizeHint
    // 收缩陷入「越省越窄」死循环，最终只剩「追…」。
    const int available = DesignTokens::kStatusChipMaxWidth
        - 2 * DesignTokens::kStatusChipPaddingH;
    const QString displayText = QFontMetrics(m_statusChip->font()).elidedText(
        m_statusText, Qt::ElideRight, available);
    m_statusChip->setToolTip(m_statusText);
    m_statusChip->setText(displayText);
    m_statusChip->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border-radius: %3px;"
                       " padding: %4px %5px; }")
            .arg(m_statusFg.name(), m_statusBg.name(QColor::HexArgb),
                 QString::number(DesignTokens::kRadiusChip),
                 QString::number(DesignTokens::kStatusChipPaddingV),
                 QString::number(DesignTokens::kStatusChipPaddingH)));
}

QPoint TitleBar::moreButtonGlobalPos() const
{
    return m_moreBtn->mapToGlobal(QPoint(0, m_moreBtn->height()));
}

void TitleBar::applyTheme()
{
    m_titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextStrong().name()));
    m_dateLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextMute().name()));

    const QString iconStyle = QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: %2px; font-size: 16px; padding: 0; }"
        "QPushButton:hover { background: %3; border-color: %4; }")
        .arg(DesignTokens::kTextMute().name(),
             QString::number(DesignTokens::kRadiusBtn),
             DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kBorder().name())
        + UiUtils::focusBorderRule();
    m_themeBtn->setStyleSheet(iconStyle);
    m_moreBtn->setStyleSheet(iconStyle);
    m_settingsBtn->setStyleSheet(iconStyle);

    const bool dark = ThemeManager::instance()->isDark();
    m_themeBtn->setIcon(SettingsIcons::icon(
        dark ? SettingsIcons::Sun : SettingsIcons::Moon, 18, DesignTokens::kTextMute()));
    m_themeBtn->setIconSize(QSize(18, 18));
    m_themeBtn->setToolTip(dark ? QStringLiteral("切换到浅色模式")
                                : QStringLiteral("切换到暗色模式"));
    m_moreBtn->setIcon(SettingsIcons::icon(
        SettingsIcons::More, 18, DesignTokens::kTextMute()));
    m_moreBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setIcon(SettingsIcons::icon(
        SettingsIcons::Gear, 18, DesignTokens::kTextMute()));
    m_settingsBtn->setIconSize(QSize(18, 18));

    refreshStatus();
    invalidateBackdrop();
}

void TitleBar::invalidateBackdrop()
{
    m_backdropDirty = true;
    update();
}

QPixmap TitleBar::blurPixmap(const QPixmap &source, int factor)
{
    if (source.isNull())
        return source;
    // 盒式降采样（factor×factor 面积平均）再平滑放大，近似高斯。
    // 不能用 QPixmap::scaled 直接缩小：Qt 缩小不做预滤波，小字号文本
    // 对比度会被保留下来，看起来像没模糊。premultiplied 格式下直接
    // 平均各通道即为正确的面积平均。
    const QImage src = source.toImage()
        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = qMax(1, src.width() / factor);
    const int h = qMax(1, src.height() / factor);
    QImage down(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        QRgb *dst = reinterpret_cast<QRgb *>(down.scanLine(y));
        const int y0 = y * factor;
        const int y1 = qMin(y0 + factor, src.height());
        for (int x = 0; x < w; ++x) {
            const int x0 = x * factor;
            const int x1 = qMin(x0 + factor, src.width());
            int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = y0; sy < y1; ++sy) {
                const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(sy));
                for (int sx = x0; sx < x1; ++sx) {
                    const QRgb p = line[sx];
                    r += qRed(p); g += qGreen(p); b += qBlue(p); a += qAlpha(p);
                    ++n;
                }
            }
            dst[x] = qRgba(r / n, g / n, b / n, a / n);
        }
    }
    QImage up = down.scaled(src.width(), src.height(),
                            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPixmap result = QPixmap::fromImage(up);
    // fromImage/scaled 会把 DPR 归 1，这里还原，避免 HiDPI 下模糊带尺寸失真。
    result.setDevicePixelRatio(source.devicePixelRatio());
    return result;
}

QPixmap TitleBar::gradientBlur(const QPixmap &crisp)
{
    // 两档模糊：顶部强磨砂（标题文字一带糊成玻璃）、下缘轻模糊
    // （卡片刚滑入处保留轮廓），用垂直 alpha 渐变把强档从上往下混出。
    QPixmap soft = blurPixmap(crisp, 3);

    // 遮罩必须在 QImage 上做：全不透明快照转 QPixmap 时 alpha 通道会被丢掉，
    // 此时 DestinationIn 不是降透明度、而是把 RGB 乘向黑色，磨砂底会糊成黑渐变。
    QImage strong = blurPixmap(crisp, 10).toImage()
        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
        QPainter mp(&strong);
        QLinearGradient fade(0, 0, 0, strong.height());
        QColor opaque(Qt::black);
        QColor clear = opaque;
        clear.setAlpha(0);
        fade.setColorAt(0.0, opaque);
        fade.setColorAt(0.85, clear);
        fade.setColorAt(1.0, clear);
        mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        mp.fillRect(strong.rect(), fade);
    }

    QPainter lp(&soft);
    lp.drawImage(0, 0, strong);
    return soft;
}

void TitleBar::ensureBackdrop()
{
    if (!m_backdropDirty)
        return;
    m_backdropDirty = false;
    m_backdropBlur = QPixmap();
    m_backdropRect = QRect();

    if (!m_scrollArea || !m_scrollArea->viewport())
        return;

    QWidget *vp = m_scrollArea->viewport();
    // 不用 vp->mapTo(this)：真实窗口里跨 sibling overlay 的 mapTo 会走
    // native-window 路径返回全局坐标（本例返回 458,190 而非 24,0），
    // 导致磨砂底画到栏外。mapToGlobal 两侧都正确，用差值求相对偏移。
    const QPoint vpGlobal = vp->mapToGlobal(QPoint(0, 0));
    const QPoint barGlobal = mapToGlobal(QPoint(0, 0));
    const QPoint vpTL(vpGlobal.x() - barGlobal.x(), vpGlobal.y() - barGlobal.y());
    const QSize vpSize = vp->size();
    const int stripH = qMin(height(), vpSize.height());
    if (stripH <= 0)
        return;

    // viewport 被样式设为透明（背景靠 central 透出），直接 grab 会把底色解析成
    // 空/黑、污染模糊源。改为：先在不透明 ARGB 快照上铺窗口底色，再把滚动内容
    // 顶部这条带 render 进去，卡片间隙自然呈现底色。快照按物理像素建，DPR 归一
    // 后逻辑尺寸才与 viewport 一致，render 源区域不会被裁掉一块。
    const qreal dpr = devicePixelRatioF();
    QImage base(QSize(vpSize.width(), stripH) * dpr,
                QImage::Format_ARGB32_Premultiplied);
    base.fill(DesignTokens::kBg());
    QPixmap crisp = QPixmap::fromImage(base);
    crisp.setDevicePixelRatio(dpr);
    vp->render(&crisp, QPoint(0, 0),
               QRegion(0, 0, vpSize.width(), stripH));

    m_backdropBlur = gradientBlur(crisp);
    m_backdropRect = QRect(vpTL.x(), vpTL.y(), vpSize.width(), stripH);
}

void TitleBar::paintEvent(QPaintEvent *)
{
    ensureBackdrop();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 1) 画磨砂底：正下方滚动内容的模糊快照。
    // 用 topLeft 重载按 pixmap 自身逻辑尺寸绘制：drawPixmap(rect, pm, pm.rect())
    // 的源矩形是设备像素尺寸，在 DPR≠1 的真实窗口里会越出逻辑边界导致空绘制。
    if (!m_backdropBlur.isNull() && !m_backdropRect.isEmpty())
        painter.drawPixmap(m_backdropRect.topLeft(), m_backdropBlur);

    // 2) 渐变遮挡膜：顶部一层磨砂玻璃，向下渐浓、到底部完全覆盖为窗口底色，
    //    使滑入标题栏的卡片看起来被磨砂玻璃「渐变吞没」。
    QLinearGradient membrane(0, 0, 0, height());
    QColor top = DesignTokens::kBg();
    top.setAlpha(120);
    QColor mid = DesignTokens::kBg();
    mid.setAlpha(200);
    QColor bottom = DesignTokens::kBg();
    bottom.setAlpha(255);
    membrane.setColorAt(0.0, top);
    membrane.setColorAt(0.55, mid);
    membrane.setColorAt(1.0, bottom);
    painter.fillRect(rect(), membrane);
}
