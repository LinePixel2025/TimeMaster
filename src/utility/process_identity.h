#ifndef PROCESS_IDENTITY_H
#define PROCESS_IDENTITY_H

#include <QString>

namespace ProcessIdentity {

inline QString normalizeKey(const QString &processName)
{
    QString value = processName.trimmed();
    const int slash = qMax(value.lastIndexOf('/'), value.lastIndexOf('\\'));
    if (slash >= 0)
        value = value.mid(slash + 1);
    return value.toLower();
}

} // namespace ProcessIdentity

#endif // PROCESS_IDENTITY_H
