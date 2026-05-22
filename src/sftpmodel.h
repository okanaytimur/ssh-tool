#pragma once
#include <QAbstractTableModel>
#include <QVariantList>

class SftpModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit SftpModel(QObject *parent = nullptr);

    void setEntries(const QString &path, const QVariantList &entries);
    QString currentPath() const { return m_path; }

    int rowCount(const QModelIndex &) const override { return m_entries.size(); }
    int columnCount(const QModelIndex &) const override { return 3; }
    QVariant data(const QModelIndex &i, int role) const override;
    QVariant headerData(int s, Qt::Orientation o, int r) const override;

    QVariant entryAt(int row) const;

private:
    QString m_path;
    QVariantList m_entries;
};
