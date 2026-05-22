#pragma once
#include <QMainWindow>
#include <QList>
#include <QPoint>
#include <QUrl>
#include "serverconfig.h"

class QTreeView;
class QTableView;
class QStatusBar;
class QLineEdit;
class QToolButton;
class SshSession;
class SftpModel;
class TerminalWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void openConnectionsDialog();
    void onConnectRequested(const ServerConfig &cfg);

    // SshSession sinyalleri
    void onConnected();
    void onConnectError(const QString &msg);
    void onDisconnected();
    void onShellData(const QByteArray &data);

    // Sol panel
    void onSftpDirListed(const QString &path, const QVariantList &entries);
    void onSftpDoubleClicked(const QModelIndex &i);
    void onSftpContextMenu(const QPoint &pos);
    void onPathEntered();
    void onPathUp();
    void onEditCurrentFile();

    // Toolbar
    void onDisconnectClicked();

private:
    void buildUi();
    void buildMenus();
    QString configPath() const;
    void handleDroppedUrls(const QList<QUrl> &urls);
    void startDragOut();
    static QString formatBytes(qint64 b);

    ServerStore   m_store;
    SshSession   *m_ssh   = nullptr;
    SftpModel    *m_model = nullptr;
    QLineEdit    *m_pathEdit = nullptr;
    QTableView   *m_fileView = nullptr;
    TerminalWidget *m_term = nullptr;
    ServerConfig  m_current;
    QString       m_cwd = "/";

    // Transfer progress prefix ("[3/10] foo.txt: ")
    QString       m_transferPrefix;

    // Drag-out tespiti
    QPoint        m_dragOutStart;
    bool          m_dragOutPending = false;
};
