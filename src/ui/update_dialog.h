#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QDialog>

#include "update/update_checker.h"

class QLabel;
class QTextBrowser;
class QPushButton;

/// 更新内容弹窗：展示新版本信息与 Release Notes，提供「立即更新 / 稍后」。
/// 自动检查（updateAvailable）与设置页手动检查完成后共用。
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(const UpdateInfo &info, QWidget *parent = nullptr);

private:
    void applyTheme();
    void openDownload();

    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QTextBrowser *m_notesView = nullptr;
    QPushButton *m_updateBtn = nullptr;
    UpdateInfo m_info;
};

#endif // UPDATE_DIALOG_H
