#pragma once
#include <QIcon>

/// Creates a QIcon that renders from an SVG resource at any size.
/// Uses QSvgRenderer internally for crisp scaling.
/// Usage: QPushButton btn; btn.setIcon(makeSvgIcon(":/icons/settings.svg"));
QIcon makeSvgIcon(const QString &resourcePath);
