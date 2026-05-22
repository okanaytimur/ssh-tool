#include "sftpmodel.h"
#include <QDateTime>
#include <QIcon>
#include <QApplication>
#include <QStyle>
#include <algorithm>

static QIcon iconForEntry(bool isDir, bool isLink) {
    // Function-local static: QApplication oluştuktan sonra ilk çağrıda initialize olur
    static const QIcon kFile     = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    static const QIcon kDir      = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    static const QIcon kFileLink = QApplication::style()->standardIcon(QStyle::SP_FileLinkIcon);
    static const QIcon kDirLink  = QApplication::style()->standardIcon(QStyle::SP_DirLinkIcon);
    if (isDir) return isLink ? kDirLink : kDir;
    return isLink ? kFileLink : kFile;
}

SftpModel::SftpModel(QObject *parent) : QAbstractTableModel(parent) {}

void SftpModel::setEntries(const QString &path, const QVariantList &entries) {
    beginResetModel();
    m_path = path;
    m_entries = entries;
    // Klasörler önce, sonra alfabetik
    std::sort(m_entries.begin(), m_entries.end(), [](const QVariant &a, const QVariant &b){
        auto ma = a.toMap(); auto mb = b.toMap();
        bool da = ma["isDir"].toBool(), db = mb["isDir"].toBool();
        if (da != db) return da;
        return ma["name"].toString().localeAwareCompare(mb["name"].toString()) < 0;
    });
    endResetModel();
}

QVariant SftpModel::entryAt(int row) const {
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries.at(row).toMap();
}

QVariant SftpModel::data(const QModelIndex &i, int role) const {
    if (!i.isValid() || i.row() >= m_entries.size()) return {};
    auto e = m_entries.at(i.row()).toMap();
    if (role == Qt::DisplayRole) {
        switch (i.column()) {
            case 0: return e["name"].toString();
            case 1: {
                if (e["isDir"].toBool()) return QString("<DIR>");
                qint64 sz = e["size"].toLongLong();
                if (sz < 1024) return QString::number(sz) + " B";
                if (sz < 1024*1024) return QString::number(sz/1024.0, 'f', 1) + " KB";
                if (sz < 1024LL*1024*1024) return QString::number(sz/(1024.0*1024), 'f', 1) + " MB";
                return QString::number(sz/(1024.0*1024*1024), 'f', 2) + " GB";
            }
            case 2: {
                qint64 t = e["mtime"].toLongLong();
                return QDateTime::fromSecsSinceEpoch(t).toString("yyyy-MM-dd hh:mm");
            }
        }
    }
    if (role == Qt::DecorationRole && i.column() == 0) {
        return iconForEntry(e["isDir"].toBool(), e["isLink"].toBool());
    }
    return {};
}

QVariant SftpModel::headerData(int s, Qt::Orientation o, int r) const {
    if (o != Qt::Horizontal || r != Qt::DisplayRole) return {};
    switch (s) {
        case 0: return "Name";
        case 1: return "Size";
        case 2: return "Modified";
    }
    return {};
}
