#include "ui/ai_report_card.h"
#include "ai/ai_client.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

#include <QDate>
#include <QEnterEvent>
#include <QFile>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// 极简 HTML 转义：AI 报告正文的富文本展示需要。
QString escapeHtml(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default:  out += ch;      break;
        }
    }
    return out;
}

// 周几中文（1=周一 … 7=周日）。
QString weekdayCn(int dayOfWeek)
{
    static const QString kNames[] = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("日")
    };
    return (dayOfWeek >= 1 && dayOfWeek <= 7) ? kNames[dayOfWeek - 1] : QString();
}

// 渐变卡上的白色文字（各级透明度）。
QString whiteText(int alpha)
{
    return QString("color: rgba(255,255,255,%1); background: transparent;")
        .arg(alpha);
}

} // namespace

/// Apple 风格总结卡内容区：透明容器，承载日期、8 字总结大字、
/// 「AI 今日总结」小字与右下角展开图标。背景（渐变/光晕）由外层
/// AiReportCard 整卡自绘，避免出现「卡中卡」。
class SummaryCard : public QWidget
{
    Q_OBJECT
public:
    explicit SummaryCard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 6, 0, 0);
        layout->setSpacing(5);

        m_dateLabel = new QLabel(this);
        m_dateLabel->setAlignment(Qt::AlignCenter);
        m_dateLabel->setFont(DesignTokens::eyebrowFont(11));
        m_dateLabel->setStyleSheet(whiteText(210));
        layout->addWidget(m_dateLabel);

        m_phraseLabel = new QLabel(this);
        m_phraseLabel->setAlignment(Qt::AlignCenter);
        m_phraseLabel->setFont(DesignTokens::appFont(25, QFont::DemiBold));
        m_phraseLabel->setStyleSheet(QStringLiteral("color: white; background: transparent;"));
        layout->addWidget(m_phraseLabel, 1);

        auto *bottomRow = new QHBoxLayout();
        bottomRow->setContentsMargins(0, 0, 0, 0);
        bottomRow->setSpacing(8);

        m_captionLabel = new QLabel(this);
        m_captionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_captionLabel->setFont(DesignTokens::appFont(11));
        m_captionLabel->setStyleSheet(whiteText(200));
        bottomRow->addWidget(m_captionLabel);
        bottomRow->addStretch();

        // 刷新报告：重新生成今日报告（位于展开图标左侧）。
        const QString roundBtnStyle = QString(
            "QPushButton { border: none; border-radius: 17px;"
            " background: rgba(255,255,255,45); color: white; font-size: 16px; }"
            "QPushButton:hover { background: rgba(255,255,255,110); }");
        m_refreshBtn = new QPushButton(QStringLiteral("⟳"), this);
        m_refreshBtn->setCursor(Qt::PointingHandCursor);
        m_refreshBtn->setFixedSize(34, 34);
        m_refreshBtn->setToolTip(QStringLiteral("刷新报告"));
        m_refreshBtn->setStyleSheet(roundBtnStyle);
        connect(m_refreshBtn, &QPushButton::clicked,
                this, &SummaryCard::refreshRequested);
        bottomRow->addWidget(m_refreshBtn, 0, Qt::AlignBottom);

        m_detailBtn = new QPushButton(QStringLiteral("↗"), this);
        m_detailBtn->setCursor(Qt::PointingHandCursor);
        m_detailBtn->setFixedSize(34, 34);
        m_detailBtn->setToolTip(QStringLiteral("查看完整报告"));
        m_detailBtn->setStyleSheet(roundBtnStyle);
        connect(m_detailBtn, &QPushButton::clicked,
                this, &SummaryCard::detailClicked);
        bottomRow->addWidget(m_detailBtn, 0, Qt::AlignBottom);
        layout->addLayout(bottomRow);
    }

    void setDateText(const QString &text) { m_dateLabel->setText(text); }
    void setPhrase(const QString &phrase) { m_phraseLabel->setText(phrase); }
    void setCaption(const QString &caption) { m_captionLabel->setText(caption); }
    void setDetailVisible(bool visible)
    {
        m_refreshBtn->setVisible(visible);
        m_detailBtn->setVisible(visible);
    }

signals:
    void detailClicked();
    void refreshRequested();

private:
    QLabel *m_dateLabel = nullptr;
    QLabel *m_phraseLabel = nullptr;
    QLabel *m_captionLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_detailBtn = nullptr;
};

AiReportCard::AiReportCard(AiClient *ai, QWidget *parent)
    : CardFrame(QStringLiteral("AI 使用报告"), parent)
    , m_ai(ai)
{
    // 小窗口下收紧卡片上下留白，为总结大字与按钮留出空间。
    contentLayout()->setContentsMargins(20, 14, 20, 16);
    contentLayout()->setSpacing(8);
    // 整卡为圆角渐变（paintEvent 自绘），标题与提示均为白色系。
    titleLabel()->setStyleSheet(QStringLiteral("color: white; background: transparent;"));

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setFont(DesignTokens::appFont(11));
    m_hintLabel->setTextFormat(Qt::PlainText);
    m_hintLabel->setStyleSheet(whiteText(220));
    contentLayout()->addWidget(m_hintLabel);

    m_summaryCard = new SummaryCard(this);
    connect(m_summaryCard, &SummaryCard::detailClicked,
            this, &AiReportCard::openDetailDialog);
    // 刷新：重新生成今日报告（与弹窗内「生成报告」同一链路）。
    connect(m_summaryCard, &SummaryCard::refreshRequested, this, [this]() {
        setLoading(true);
        emit generateRequested();
    });
    contentLayout()->addWidget(m_summaryCard, 1);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        // 后于 CardFrame 的 themeChanged 触发，保证标题恒为白色。
        titleLabel()->setStyleSheet(
            QStringLiteral("color: white; background: transparent;"));
        refreshContent();
    });

    reloadState();
}

void AiReportCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal radius = 16.0;
    const QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    // 柔和渐变底（顶部亮 → 底部深，随主题取色）。
    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.0, DesignTokens::kChartGradientTop());
    bg.setColorAt(1.0, DesignTokens::kChartGradientBottom());
    painter.fillPath(path, bg);

    // 顶部光晕：玻璃质感高光，hover 时增亮。
    QLinearGradient gloss(0, 0, 0, height() * 0.55);
    gloss.setColorAt(0.0, QColor(255, 255, 255, m_glossAlpha));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillPath(path, gloss);

    // 细边框：浅色主题下与背景区分。
    painter.setPen(QPen(DesignTokens::kCardBorder(), 1.0));
    painter.drawPath(path);

    QWidget::paintEvent(event);
}

void AiReportCard::enterEvent(QEnterEvent *event)
{
    m_glossAlpha = 95;
    update();
    QWidget::enterEvent(event);
}

void AiReportCard::leaveEvent(QEvent *event)
{
    m_glossAlpha = 70;
    update();
    QWidget::leaveEvent(event);
}

void AiReportCard::setWeeklyReportPath(const QString &path)
{
    // 路径为空或文件已不存在时视为未生成（弹窗内按钮据此禁用）。
    m_weeklyReportPath =
        (!path.isEmpty() && QFile::exists(path)) ? path : QString();
}

void AiReportCard::reloadState()
{
    m_reportText = m_ai->cachedReport(AiPeriod::daily());
    refreshContent();
}

void AiReportCard::setReport(const QString &text)
{
    m_reportText = text;
    m_error.clear();
    m_loading = false;
    refreshContent();
}

void AiReportCard::setLoading(bool loading)
{
    m_loading = loading;
    refreshContent();
}

void AiReportCard::showError(const QString &error)
{
    m_error = error;
    m_loading = false;
    refreshContent();
}

void AiReportCard::openDetailDialog()
{
    // 无报告时也允许打开：弹窗内可「生成报告」（生成入口集中在弹窗）。
    // 生成失败时把错误文案传入弹窗，正文区优先展示具体原因。
    ReportDetailDialog dialog(QStringLiteral("每日报告 · ") + todayText(),
                              m_reportText, m_weeklyReportPath, m_error, this);
    // 弹窗内的报告操作转发到既有信号链（main 端接线不变）。
    connect(&dialog, &ReportDetailDialog::generateRequested, this, [this]() {
        setLoading(true);        // 弹窗关闭后卡片显示生成中
        emit generateRequested(); // 触发 main 端真实生成请求
    });
    connect(&dialog, &ReportDetailDialog::weeklyReportOpenRequested, this,
            &AiReportCard::weeklyReportOpenRequested);
    connect(&dialog, &ReportDetailDialog::weeklyReportGenerateRequested, this,
            &AiReportCard::weeklyReportGenerateRequested);
    dialog.exec();
}

void AiReportCard::refreshContent()
{
    const bool hasReport = !m_reportText.isEmpty();

    // 整卡始终显示（渐变卡即组件本体），状态差异体现在大字与图标上。
    m_summaryCard->setDateText(todayText());

    if (m_loading) {
        m_hintLabel->setText(QStringLiteral("正在生成报告…"));
        m_summaryCard->setPhrase(QStringLiteral("正在生成报告…"));
        m_summaryCard->setCaption(QStringLiteral("AI 正在分析今日使用数据"));
        m_summaryCard->setDetailVisible(false);
        return;
    }

    if (!m_error.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("生成失败，可以重试"));
        m_hintLabel->setStyleSheet(
            QString("color: white; background: transparent; font-weight: 600;"));
        // 完整错误展示在弹窗内（大字区只保留简短重试引导），
        // 同时追加写入错误日志，便于排查「一直失败」。
        m_summaryCard->setPhrase(QStringLiteral("生成失败，可以重试"));
        m_summaryCard->setCaption(
            QStringLiteral("点击右下角 ↗ 查看原因并重试"));
        m_summaryCard->setDetailVisible(true);
        return;
    }
    m_hintLabel->setStyleSheet(whiteText(220));

    if (!m_ai->isConfigured()) {
        m_reportText.clear();
        m_hintLabel->setText(QStringLiteral("⚙ AI 智能尚未配置"));
        m_summaryCard->setPhrase(QStringLiteral("AI 智能尚未配置"));
        m_summaryCard->setCaption(
            QStringLiteral("在「设置 → AI 智能」中启用并填写 API 信息"));
        m_summaryCard->setDetailVisible(false);
        return;
    }

    if (hasReport) {
        const QString cachedDate = m_ai->cachedReportDate(AiPeriod::daily());
        const bool fresh = (cachedDate == QDate::currentDate().toString(Qt::ISODate));
        m_hintLabel->setText(
            fresh ? QStringLiteral("✔ 报告已更新（%1）").arg(todayText())
                  : QStringLiteral("⏳ 报告缓存于 %1，点击右下角 ↗ 查看并更新")
                        .arg(cachedDate));
        m_summaryCard->setCaption(QStringLiteral("AI 今日总结"));
        m_summaryCard->setDetailVisible(true);
        updateSummaryCard();
        return;
    }

    // 无缓存：AI 已配置但今日未生成 → 引导从弹窗生成。
    m_hintLabel->setText(QStringLiteral("点击右下角 ↗ 查看并生成今日报告"));
    m_summaryCard->setPhrase(QStringLiteral("还没有今日报告"));
    m_summaryCard->setCaption(QStringLiteral("点击 ↗ 生成每日分析报告"));
    m_summaryCard->setDetailVisible(true);
}

void AiReportCard::updateSummaryCard()
{
    QString phrase = summaryPhrase();
    if (phrase.isEmpty())
        phrase = QStringLiteral("今日使用情况");
    m_summaryCard->setPhrase(phrase);
}

QString AiReportCard::summaryPhrase() const
{
    if (m_reportText.isEmpty())
        return QString();
    const QStringList lines = m_reportText.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        int pos = -1;
        if (line.startsWith(QStringLiteral("【总结】")))
            pos = 4;
        else if (line.startsWith(QStringLiteral("总结：")))
            pos = 3;
        if (pos < 0)
            continue;
        QString phrase = line.mid(pos).trimmed();
        if (phrase.isEmpty())
            break;
        if (phrase.endsWith(QStringLiteral("。")) || phrase.endsWith(QLatin1Char('.')))
            phrase.chop(1);
        return capPhrase(phrase);
    }
    // 旧缓存报告可能没有【总结】行：回退到概览小节首句。
    return capPhrase(overviewFirstSentence());
}

QString AiReportCard::overviewFirstSentence() const
{
    const QStringList lines = m_reportText.split(QLatin1Char('\n'));
    bool inOverview = false;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("## "))) {
            inOverview = line.contains(QStringLiteral("概览"));
            continue;
        }
        if (inOverview && !line.isEmpty())
            return line;
    }
    // 无概览小节：取首个普通段落。
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty() && !line.startsWith(QStringLiteral("## "))
            && !line.startsWith(QStringLiteral("- ")))
            return line;
    }
    return QString();
}

QString AiReportCard::capPhrase(const QString &phrase) const
{
    if (phrase.size() <= 12)
        return phrase;
    return phrase.left(12) + QStringLiteral("…");
}

QString AiReportCard::todayText() const
{
    const QDate d = QDate::currentDate();
    return QStringLiteral("%1月%2日 · 星期%3")
        .arg(d.month()).arg(d.day()).arg(weekdayCn(d.dayOfWeek()));
}

// SummaryCard 定义在本文件内且含 Q_OBJECT，需显式包含 moc 输出。
#include "ai_report_card.moc"
