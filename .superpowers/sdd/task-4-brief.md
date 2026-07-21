### Task 4: 重写 refresh 方法 + filter 方法 + 更新 onAddIgnored

**Files:**
- Modify: `src/ui/settings_dialog.cpp` — `refreshKnownAppsList()`, `refreshIgnoredList()`, `onAddIgnored()`, 新增 `filterKnownApps()`, `filterIgnoredApps()`

**Interfaces:**
- Consumes: `friendlyName` (Task 2), `AppIconProvider` (Task 2), `m_knownAppsList`, `m_ignoredAppsList`, `m_knownSearch`, `m_ignoredSearch` (Task 1, Task 3)

- [ ] **Step 1: 替换 refreshKnownAppsList()**

将原 `refreshKnownAppsList()` 方法（第 201-215 行）替换为：

```cpp
void SettingsDialog::refreshKnownAppsList()
{
    m_knownAppsList->clear();
    m_knownSearch->clear();

    QStringList processNames = m_db->getAllKnownProcessNames();
    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QSet<QString> ignoredNames;
    for (auto it = ignored.begin(); it != ignored.end(); ++it)
        ignoredNames.insert(it.value());

    QMap<QString, QString> aliases = m_db->getAppAliases();

    for (const QString &path : processNames) {
        QString name = friendlyName(path, aliases);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        QListWidgetItem *item = new QListWidgetItem(icon, name);
        item->setData(Qt::UserRole, path);
        if (ignoredNames.contains(path))
            item->setForeground(QColor("#9CA3AF"));
        m_knownAppsList->addItem(item);
    }
}
```

- [ ] **Step 2: 替换 refreshIgnoredList()**

将原 `refreshIgnoredList()` 方法（第 217-226 行）替换为：

```cpp
void SettingsDialog::refreshIgnoredList()
{
    m_ignoredAppsList->clear();
    m_ignoredSearch->clear();

    QMap<int, QString> ignored = m_db->getIgnoredApps();
    QMap<QString, QString> aliases = m_db->getAppAliases();

    for (auto it = ignored.begin(); it != ignored.end(); ++it) {
        QString path = it.value();
        QString name = friendlyName(path, aliases);
        QIcon icon = AppIconProvider::instance()->icon(path, 20);
        QListWidgetItem *item = new QListWidgetItem(icon, name);
        item->setData(Qt::UserRole, it.key());
        m_ignoredAppsList->addItem(item);
    }
}
```

- [ ] **Step 3: 更新 onAddIgnored()**

将原 `onAddIgnored()` 方法中：
```cpp
        m_db->addIgnoredApp(item->text());
```
替换为：
```cpp
        QString processPath = item->data(Qt::UserRole).toString();
        m_db->addIgnoredApp(processPath);
```

- [ ] **Step 4: 新增 filterKnownApps() 和 filterIgnoredApps()**

在 `refreshIgnoredList()` 方法定义之后，添加两个新 filter 方法：

```cpp
void SettingsDialog::filterKnownApps(const QString &text)
{
    for (int i = 0; i < m_knownAppsList->count(); ++i) {
        QListWidgetItem *item = m_knownAppsList->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void SettingsDialog::filterIgnoredApps(const QString &text)
{
    for (int i = 0; i < m_ignoredAppsList->count(); ++i) {
        QListWidgetItem *item = m_ignoredAppsList->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}
```

---

