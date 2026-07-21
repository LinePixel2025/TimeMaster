#include "insight_card.h"
#include "ui/design_tokens.h"

#include <QHBoxLayout>

InsightCard::InsightCard(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    const QString accentBtnStyle = QString(
        "QPushButton { background-color: %1; color: white; border: none;"
        "  border-radius: %2px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background-color: %3; }"
        "QPushButton:pressed { background-color: %4; }")
        .arg(DesignTokens::kAccent.name())
        .arg(DesignTokens::kRadiusBtn)
        .arg(DesignTokens::kAccentLight.name())
        .arg(DesignTokens::kAccent.name() + "CC");

    const QString linkBtnStyle = QString(
        "QPushButton { color: %1; background: transparent; border: none;"
        "  font-size: 13px; padding: 4px 0; }"
        "QPushButton:hover { color: %2; }")
        .arg(DesignTokens::kAccent.name())
        .arg(DesignTokens::kAccentLight.name());

    m_stack = new QStackedWidget(this);

    // ======== Page 0 — Idle ========
    m_idlePage = new QWidget();
    auto *idleLayout = new QVBoxLayout(m_idlePage);
    idleLayout->setContentsMargins(16, 20, 16, 20);
    idleLayout->setSpacing(16);

    auto *idleTitle = new QLabel(
        QString::fromUtf8("AI \xe4\xbd\xbf\xe7\x94\xa8\xe5\x88\x86\xe6\x9e\x90"),
        m_idlePage);
    idleTitle->setFont(DesignTokens::appFont(14, QFont::Medium));
    idleTitle->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong.name()));

    m_notConfiguredLabel = new QLabel(
        QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe9\x85\x8d\xe7\xbd\xae API"),
        m_idlePage);
    m_notConfiguredLabel->setFont(DesignTokens::appFont(12));
    m_notConfiguredLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute.name()));

    m_analyzeBtn = new QPushButton(
        QString::fromUtf8("\xe7\x94\x9f\xe6\x88\x90\xe5\x88\x86\xe6\x9e\x90"),
        m_idlePage);
    m_analyzeBtn->setCursor(Qt::PointingHandCursor);
    m_analyzeBtn->setStyleSheet(accentBtnStyle);
    connect(m_analyzeBtn, &QPushButton::clicked,
            this, &InsightCard::requestAnalysis);

    idleLayout->addWidget(idleTitle);
    idleLayout->addWidget(m_notConfiguredLabel);
    idleLayout->addWidget(m_analyzeBtn);
    idleLayout->addStretch();
    m_stack->addWidget(m_idlePage);

    // ======== Page 1 — Loading ========
    auto *loadingPage = new QWidget();
    auto *loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setContentsMargins(16, 24, 16, 24);
    loadingLayout->setSpacing(16);
    loadingLayout->setAlignment(Qt::AlignCenter);

    auto *loadingTitle = new QLabel(
        QString::fromUtf8("AI \xe4\xbd\xbf\xe7\x94\xa8\xe5\x88\x86\xe6\x9e\x90"),
        loadingPage);
    loadingTitle->setFont(DesignTokens::appFont(14, QFont::Medium));
    loadingTitle->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong.name()));
    loadingTitle->setAlignment(Qt::AlignCenter);

    auto *progressBar = new QProgressBar(loadingPage);
    progressBar->setRange(0, 0); // indeterminate
    progressBar->setFixedWidth(160);
    progressBar->setFixedHeight(4);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(
        QString(
            "QProgressBar { background: %1; border: none; border-radius: 2px; }"
            "QProgressBar::chunk { background: %2; border-radius: 2px; }")
            .arg(QColor(0, 0, 0, 12).name(QColor::HexArgb))
            .arg(DesignTokens::kAccent.name()));

    auto *loadingLabel = new QLabel(
        QString::fromUtf8("\xe6\xad\xa3\xe5\x9c\xa8\xe5\x88\x86\xe6\x9e\x90..."),
        loadingPage);
    loadingLabel->setFont(DesignTokens::appFont(12));
    loadingLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextMute.name()));
    loadingLabel->setAlignment(Qt::AlignCenter);

    loadingLayout->addWidget(loadingTitle);
    loadingLayout->addWidget(progressBar, 0, Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    m_stack->addWidget(loadingPage);

    // ======== Page 2 — Error ========
    auto *errorPage = new QWidget();
    auto *errorLayout = new QVBoxLayout(errorPage);
    errorLayout->setContentsMargins(16, 24, 16, 24);
    errorLayout->setSpacing(16);
    errorLayout->setAlignment(Qt::AlignCenter);

    auto *errorTitle = new QLabel(
        QString::fromUtf8("AI \xe4\xbd\xbf\xe7\x94\xa8\xe5\x88\x86\xe6\x9e\x90"),
        errorPage);
    errorTitle->setFont(DesignTokens::appFont(14, QFont::Medium));
    errorTitle->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong.name()));
    errorTitle->setAlignment(Qt::AlignCenter);

    m_errorLabel = new QLabel(errorPage);
    m_errorLabel->setFont(DesignTokens::appFont(12));
    m_errorLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kError.name()));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);

    m_retryBtn = new QPushButton(
        QString::fromUtf8("\xe9\x87\x8d\xe8\xaf\x95"),
        errorPage);
    m_retryBtn->setCursor(Qt::PointingHandCursor);
    m_retryBtn->setStyleSheet(accentBtnStyle);
    connect(m_retryBtn, &QPushButton::clicked,
            this, &InsightCard::requestAnalysis);

    errorLayout->addWidget(errorTitle);
    errorLayout->addWidget(m_errorLabel, 0, Qt::AlignCenter);
    errorLayout->addWidget(m_retryBtn, 0, Qt::AlignCenter);
    m_stack->addWidget(errorPage);

    // ======== Page 3 — Result ========
    auto *resultPage = new QWidget();
    auto *resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(16, 20, 16, 20);
    resultLayout->setSpacing(12);

    auto *resultTitle = new QLabel(
        QString::fromUtf8("AI \xe4\xbd\xbf\xe7\x94\xa8\xe5\x88\x86\xe6\x9e\x90"),
        resultPage);
    resultTitle->setFont(DesignTokens::appFont(14, QFont::Medium));
    resultTitle->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong.name()));

    m_headlineLabel = new QLabel(resultPage);
    m_headlineLabel->setFont(DesignTokens::appFont(14, QFont::Bold));
    m_headlineLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kTextStrong.name()));
    m_headlineLabel->setWordWrap(true);

    m_summaryLabel = new QLabel(resultPage);
    m_summaryLabel->setFont(DesignTokens::appFont(12));
    m_summaryLabel->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(DesignTokens::kText.name()));
    m_summaryLabel->setWordWrap(true);

    m_reportBtn = new QPushButton(
        QString::fromUtf8("\xe6\x9f\xa5\xe7\x9c\x8b\xe5\xae\x8c\xe6\x95\xb4\xe6\x8a\xa5\xe5\x91\x8a"
                          " \xe2\x86\x92"),
        resultPage);
    m_reportBtn->setCursor(Qt::PointingHandCursor);
    m_reportBtn->setStyleSheet(linkBtnStyle);
    connect(m_reportBtn, &QPushButton::clicked,
            this, &InsightCard::viewFullReport);

    resultLayout->addWidget(resultTitle);
    resultLayout->addWidget(m_headlineLabel);
    resultLayout->addWidget(m_summaryLabel);
    resultLayout->addWidget(m_reportBtn);
    resultLayout->addStretch();
    m_stack->addWidget(resultPage);

    // --- Default state ---
    m_stack->setCurrentIndex(0);
    outerLayout->addWidget(m_stack);
}

void InsightCard::setConfigured(bool configured)
{
    m_notConfiguredLabel->setVisible(!configured);
    m_analyzeBtn->setVisible(configured);
}

void InsightCard::setLoading(bool loading)
{
    m_stack->setCurrentIndex(loading ? 1 : 0);
}

void InsightCard::setError(const QString &error)
{
    m_errorLabel->setText(error);
    m_stack->setCurrentIndex(2);
}

void InsightCard::setResult(const QString &headline, const QString &summary,
                            const QString &suggestions)
{
    m_headlineLabel->setText(headline);

    // Combine summary and suggestions if both are non-empty
    QString body = summary;
    if (!suggestions.isEmpty()) {
        if (!body.isEmpty())
            body += "\n\n";
        body += suggestions;
    }
    m_summaryLabel->setText(body);

    m_stack->setCurrentIndex(3);
}
