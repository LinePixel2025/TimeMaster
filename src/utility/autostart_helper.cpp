#include "autostart_helper.h"

#include <QCoreApplication>
#include <QDir>
#include <windows.h>

static const wchar_t *kRunKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *kValueName  = L"TimeMaster";

void AutoStartHelper::setAutoStart(bool enable)
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
        return;

    if (enable) {
        QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        std::wstring wPath = exePath.toStdWString();
        RegSetValueExW(hKey, kValueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(wPath.c_str()),
                       static_cast<DWORD>((wPath.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, kValueName);
    }

    RegCloseKey(hKey);
}

bool AutoStartHelper::isAutoStartEnabled()
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
        return false;

    DWORD type;
    wchar_t buf[MAX_PATH];
    DWORD bufSize = sizeof(buf);
    result = RegQueryValueExW(hKey, kValueName, nullptr, &type,
                              reinterpret_cast<BYTE *>(buf), &bufSize);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}
