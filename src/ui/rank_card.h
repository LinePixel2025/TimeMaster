#pragma once

#include "ui/card_frame.h"

#include <QIcon>
#include <QScrollArea>
#include <QVariantMap>
#include <QVector>

class QButtonGroup;
class QPushButton;
class QVBoxLayout;

/// 应用/组别使用排行。同时提供两种数据时（用户已配置组别），卡片头部
/// 出现「应用排行 / 组别排行」切换；否则只显示应用排行。
class RankCard : public CardFrame
{
    Q_OBJECT
public:
    explicit RankCard(QWidget *parent = nullptr);

    /// 应用排行数据；仅传此参数时卡片不显示切换按钮。
    void refresh(const QVector<QVariantMap> &rankData);
    /// 同时提供应用与组别数据；groupData 非空即启用切换。
    void setData(const QVector<QVariantMap> &appData,
                 const QVector<QVariantMap> &groupData);

private:
    enum class Mode { Apps, Groups };

    void setMode(Mode mode);
    void renderCurrent();
    void updateToggleStyles();
    QString toggleStyle(QPushButton *button) const;

    QVBoxLayout *m_listLayout = nullptr;
    QWidget *m_listWidget = nullptr;
    QScrollArea *m_scrollArea = nullptr;

    QButtonGroup *m_modeGroup = nullptr;
    QPushButton *m_appBtn = nullptr;
    QPushButton *m_groupBtn = nullptr;
    QVector<QVariantMap> m_appData;
    QVector<QVariantMap> m_groupData;
    Mode m_mode = Mode::Apps;
    bool m_updating = false;
};
