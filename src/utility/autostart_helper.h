#ifndef AUTOSTART_HELPER_H
#define AUTOSTART_HELPER_H

#include <QString>

class AutoStartHelper
{
public:
    static void setAutoStart(bool enable);
    static bool isAutoStartEnabled();
};

#endif // AUTOSTART_HELPER_H
