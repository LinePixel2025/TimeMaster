#pragma once

#include <QFrame>

class QLabel;
class QVBoxLayout;

/// Unified card container: rounded surface, thin border, optional title row.
/// All dashboard cards derive from this so visual style stays consistent
/// and theme switching is handled in one place.
class CardFrame : public QFrame
{
    Q_OBJECT
public:
    explicit CardFrame(const QString &title, QWidget *parent = nullptr);
    explicit CardFrame(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    QLabel *titleLabel() const { return m_titleLabel; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_titleLabel = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
};
