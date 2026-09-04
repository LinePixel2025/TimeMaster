#pragma once

#include <QColor>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;
class QPixmap;

/// 主页顶栏的「渐变模糊遮挡」标题栏。
///
/// 作为 central 的子部件以 overlay 方式覆盖在满血滚动区上方：仪表盘内容从底部
/// 滑入本栏。paintEvent 把滚动 viewport 中位于标题栏正下方的那块区域渲染到铺好
/// 不透明底色的快照上，做「强/弱两档缩小再放大」模糊并按垂直渐变 alpha 混合
/// （顶部磨砂最强、向下渐轻），再叠一层垂直渐变膜淡出到窗口底色，形成渐变模糊
/// 遮挡。所有控件作为子部件覆盖其上，保持清晰可点。
///
/// 遵守 AGENTS.md：不使用 WA_TranslucentBackground / DWMWA_SYSTEMBACKDROP_TYPE /
/// DwmExtendFrameIntoClientArea，磨砂完全在既有纯色窗口背景内用 QPainter 伪造。
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QScrollArea *scrollArea, QWidget *parent = nullptr);

    /// 换肤入口：刷新标签/按钮/芯片配色与图标，并重绘磨砂底。由 MainWindow 调用。
    void applyTheme();
    void setDateText(const QString &text);
    /// 更新状态芯片文案与配色（含按 kStatusChipMaxWidth 省略），内部缓存以便换肤后重刷。
    void setStatus(const QString &text, const QColor &fg, const QColor &bg);
    /// 仅重刷已缓存的芯片文案/配色（宽/窄布局切换、data 刷新时调用）。
    void refreshStatus();
    /// 「更多」按钮全局坐标，供菜单定位。
    QPoint moreButtonGlobalPos() const;

    /// 期望占据的总高度 = 顶部留白 + 控制区 + 底部渐变淡出带。
    static int desiredHeight();

    /// 内容滚动 / 换肤 / 改尺寸后由 MainWindow 调用，令下一次 paintEvent 重新抓取磨砂底。
    void invalidateBackdrop();

signals:
    void themeButtonClicked();
    void moreButtonClicked();
    void settingsButtonClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void ensureBackdrop();
    /// 缩小 factor 倍再平滑放大的廉价高斯近似，factor 越大越糊；保留 DPR。
    static QPixmap blurPixmap(const QPixmap &source, int factor);
    /// 两档模糊按垂直 alpha 渐变合成「上浓下淡」的渐变模糊带。
    static QPixmap gradientBlur(const QPixmap &crisp);

    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_dateLabel = nullptr;
    QLabel *m_statusChip = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QPushButton *m_moreBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;

    QString m_statusText;
    QColor m_statusFg;
    QColor m_statusBg;

    bool m_backdropDirty = true;
    QPixmap m_backdropBlur;
    QRect m_backdropRect;
};
