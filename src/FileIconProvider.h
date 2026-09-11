#pragma once

#include <QFileIconProvider>

class FileIconProvider final : public QFileIconProvider {
public:
    QIcon icon(const QFileInfo &info) const override;
};
