#include "mainwindow.h"
#include "connectionsdialog.h"
#include "sshsession.h"
#include "sftpmodel.h"
#include "terminalwidget.h"
#include "editorwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QToolButton>
#include <QStatusBar>
#include <QSplitter>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QStyle>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDrag>
#include <QMimeData>
#include <QFileInfo>
#include <QMouseEvent>
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QDateTime>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SSH Tool");
    resize(1100, 720);

    m_ssh = new SshSession(this);
    m_model = new SftpModel(this);

    buildUi();
    buildMenus();

    // Konfigürasyonu yükle (yoksa boş başla)
    m_store.load(configPath());

    connect(m_ssh, &SshSession::connected,        this, &MainWindow::onConnected);
    connect(m_ssh, &SshSession::connectError,     this, &MainWindow::onConnectError);
    connect(m_ssh, &SshSession::disconnected,     this, &MainWindow::onDisconnected);
    connect(m_ssh, &SshSession::shellData,        this, &MainWindow::onShellData);
    connect(m_ssh, &SshSession::sftpDirListed,    this, &MainWindow::onSftpDirListed);
    connect(m_ssh, &SshSession::sftpError, this, [this](const QString &m){
        statusBar()->showMessage("SFTP: " + m, 5000);
    });
    connect(m_ssh, &SshSession::sftpProgress, this, [this](qint64 d, qint64 t){
        QString line;
        if (t > 0) {
            line = QString("%1%2 / %3 (%4%)")
                    .arg(m_transferPrefix)
                    .arg(formatBytes(d))
                    .arg(formatBytes(t))
                    .arg(d * 100 / t);
        } else {
            line = QString("%1%2").arg(m_transferPrefix).arg(formatBytes(d));
        }
        statusBar()->showMessage(line);
    });
    connect(m_ssh, &SshSession::sftpTransferDone, this, [this](const QString &l, const QString &r){
        Q_UNUSED(l); Q_UNUSED(r);
        statusBar()->showMessage(tr("%1bitti").arg(m_transferPrefix), 2000);
    });

    connect(m_term, &TerminalWidget::inputReady, m_ssh, &SshSession::writeShell);
    connect(m_term, &TerminalWidget::resized, m_ssh, &SshSession::resizePty);
}

MainWindow::~MainWindow() {
    m_store.save(configPath());
}

QString MainWindow::configPath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/servers.json";
}

void MainWindow::buildUi() {
    auto *split = new QSplitter(this);

    // Sol panel: yol çubuğu + dosya tablosu
    auto *leftWrap = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);

    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(2, 2, 2, 0);
    pathRow->setSpacing(2);
    auto *upBtn = new QToolButton(leftWrap);
    upBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    upBtn->setToolTip(tr("Üst klasör"));
    auto *refreshBtn = new QToolButton(leftWrap);
    refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refreshBtn->setToolTip(tr("Yenile"));
    m_pathEdit = new QLineEdit(leftWrap);
    m_pathEdit->setPlaceholderText("/");
    m_pathEdit->setClearButtonEnabled(true);
    pathRow->addWidget(upBtn);
    pathRow->addWidget(refreshBtn);
    pathRow->addWidget(m_pathEdit, 1);
    leftLayout->addLayout(pathRow);

    m_fileView = new QTableView(leftWrap);
    m_fileView->setModel(m_model);
    m_fileView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileView->horizontalHeader()->setStretchLastSection(true);
    m_fileView->setSortingEnabled(false);
    m_fileView->setContextMenuPolicy(Qt::CustomContextMenu);
    // Drag-drop: Windows Explorer'dan dosya bırakılınca SFTP upload
    m_fileView->setAcceptDrops(true);
    m_fileView->viewport()->setAcceptDrops(true);
    m_fileView->installEventFilter(this);
    m_fileView->viewport()->installEventFilter(this);
    connect(m_fileView, &QTableView::doubleClicked, this, &MainWindow::onSftpDoubleClicked);
    connect(m_fileView, &QTableView::customContextMenuRequested,
            this, &MainWindow::onSftpContextMenu);
    leftLayout->addWidget(m_fileView, 1);

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &MainWindow::onPathEntered);
    connect(upBtn,      &QToolButton::clicked,    this, &MainWindow::onPathUp);
    connect(refreshBtn, &QToolButton::clicked,    this, [this]{
        if (m_ssh && m_ssh->isConnected()) m_ssh->sftpListDir(m_cwd);
    });

    m_term = new TerminalWidget(this);

    split->addWidget(leftWrap);
    split->addWidget(m_term);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 3);
    setCentralWidget(split);

    statusBar()->showMessage("Disconnected");
}

void MainWindow::buildMenus() {
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *aConn = fileMenu->addAction(tr("&Connections..."));
    aConn->setShortcut(QKeySequence("Ctrl+L"));
    connect(aConn, &QAction::triggered, this, &MainWindow::openConnectionsDialog);

    auto *aDisc = fileMenu->addAction(tr("&Disconnect"));
    connect(aDisc, &QAction::triggered, this, &MainWindow::onDisconnectClicked);

    fileMenu->addSeparator();
    auto *aQuit = fileMenu->addAction(tr("&Quit"));
    aQuit->setShortcut(QKeySequence::Quit);
    connect(aQuit, &QAction::triggered, this, &QWidget::close);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this]{
        QMessageBox::about(this, tr("About"),
            tr("<h3>SSH Tool 0.1</h3>"
               "<p>PuTTY benzeri SSH + SFTP istemcisi.</p>"
               "<p>Qt 5.15 &middot; libssh &middot; libvterm</p>"
               "<p>Hedef: Windows 7+</p>"
               "<p><b>Okan Aytimur</b></p>"));
    });
}

QString MainWindow::formatBytes(qint64 b) {
    if (b < 1024)              return QString::number(b) + " B";
    double v = b / 1024.0;
    if (v < 1024)              return QString::number(v, 'f', 1) + " KB";
    v /= 1024.0;
    if (v < 1024)              return QString::number(v, 'f', 1) + " MB";
    v /= 1024.0;
    return QString::number(v, 'f', 2) + " GB";
}

void MainWindow::openConnectionsDialog() {
    ConnectionsDialog dlg(&m_store, this);
    connect(&dlg, &ConnectionsDialog::connectRequested,
            this, &MainWindow::onConnectRequested);
    connect(&dlg, &ConnectionsDialog::saveRequested, this, [this]{
        m_store.save(configPath());
    });
    dlg.exec();
    m_store.save(configPath());   // belt-and-suspenders
}

void MainWindow::onConnectRequested(const ServerConfig &cfg) {
    m_current = cfg;
    statusBar()->showMessage(QString("Connecting to %1...").arg(cfg.host));
    m_ssh->connectToServer(cfg);
}

void MainWindow::onConnected() {
    setWindowTitle(QString("SSH Tool - %1@%2").arg(m_current.username, m_current.host));
    statusBar()->showMessage("Connected", 3000);
    m_ssh->openShell(80, 24);
    m_cwd = m_current.initialDirectory.isEmpty() ? "/" : m_current.initialDirectory;
    m_ssh->sftpListDir(m_cwd);
}

void MainWindow::onConnectError(const QString &msg) {
    QMessageBox::warning(this, "Connection failed", msg);
    statusBar()->showMessage("Disconnected");
}

void MainWindow::onDisconnected() {
    setWindowTitle("SSH Tool");
    statusBar()->showMessage("Disconnected");
}

void MainWindow::onDisconnectClicked() {
    m_ssh->disconnect();
}

void MainWindow::onShellData(const QByteArray &data) {
    m_term->appendOutput(data);
}

void MainWindow::onSftpDirListed(const QString &path, const QVariantList &entries) {
    m_cwd = path;
    if (m_pathEdit) {
        QSignalBlocker block(m_pathEdit);
        m_pathEdit->setText(path);
    }
    QVariantList withParent;
    if (path != "/") {
        QVariantMap up;
        up["name"] = "..";
        up["isDir"] = true;
        up["size"] = 0;
        up["mtime"] = 0;
        withParent.push_back(up);
    }
    withParent.append(entries);
    m_model->setEntries(path, withParent);
    statusBar()->showMessage(path);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
    const bool fromFileView = (obj == m_fileView) ||
                              (m_fileView && obj == m_fileView->viewport());
    if (fromFileView) {
        switch (e->type()) {
        case QEvent::DragEnter: {
            auto *de = static_cast<QDragEnterEvent*>(e);
            // Kendi drag'imiz mi (source == this) → tekrar upload etmeyi engelle
            if (de->source() == this) { de->ignore(); return true; }
            if (m_ssh && m_ssh->isConnected() && de->mimeData()->hasUrls()) {
                de->setDropAction(Qt::CopyAction);
                de->accept();
                return true;
            }
            de->ignore();
            return true;
        }
        case QEvent::DragMove: {
            auto *de = static_cast<QDragMoveEvent*>(e);
            if (de->source() == this) { de->ignore(); return true; }
            if (m_ssh && m_ssh->isConnected() && de->mimeData()->hasUrls()) {
                de->setDropAction(Qt::CopyAction);
                de->accept();
                return true;
            }
            de->ignore();
            return true;
        }
        case QEvent::Drop: {
            auto *de = static_cast<QDropEvent*>(e);
            if (de->source() == this) { de->ignore(); return true; }
            if (m_ssh && m_ssh->isConnected() && de->mimeData()->hasUrls()) {
                handleDroppedUrls(de->mimeData()->urls());
                de->setDropAction(Qt::CopyAction);
                de->accept();
                return true;
            }
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton) {
                QModelIndex idx = m_fileView->indexAt(me->pos());
                m_dragOutPending = idx.isValid();
                if (m_dragOutPending) m_dragOutStart = me->pos();
            }
            return false;   // QTableView seçim yapsın
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent*>(e);
            if (m_dragOutPending && (me->buttons() & Qt::LeftButton)) {
                if ((me->pos() - m_dragOutStart).manhattanLength()
                    >= QApplication::startDragDistance())
                {
                    m_dragOutPending = false;
                    startDragOut();
                    return true;   // event'i tüket
                }
            }
            return false;
        }
        case QEvent::MouseButtonRelease:
            m_dragOutPending = false;
            return false;
        default: break;
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::handleDroppedUrls(const QList<QUrl> &urls) {
    if (!m_ssh || !m_ssh->isConnected()) {
        statusBar()->showMessage(tr("Bağlı değil"), 3000);
        return;
    }

    // Önce yüklenecek dosyaları filtrele (klasörler atlanır)
    struct Item { QString local; QString name; };
    QList<Item> items;
    int skipped = 0;
    for (const QUrl &u : urls) {
        if (!u.isLocalFile()) { ++skipped; continue; }
        QString local = u.toLocalFile();
        QFileInfo fi(local);
        if (!fi.exists() || fi.isDir() || !fi.isFile()) { ++skipped; continue; }
        items.push_back({local, fi.fileName()});
    }
    if (items.isEmpty()) {
        statusBar()->showMessage(tr("Yüklenecek dosya yok (%1 atlandı)").arg(skipped), 4000);
        return;
    }

    const int N = items.size();
    for (int i = 0; i < N; ++i) {
        m_transferPrefix = QString("[%1/%2] %3 ").arg(i + 1).arg(N).arg(items[i].name);
        statusBar()->showMessage(m_transferPrefix + tr("yükleniyor..."));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        QString remote = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + items[i].name;
        m_ssh->sftpUpload(items[i].local, remote);
    }
    m_transferPrefix.clear();
    m_ssh->sftpListDir(m_cwd);

    QString msg = tr("Drop: %1 dosya yüklendi").arg(N);
    if (skipped > 0) msg += tr(", %1 atlandı").arg(skipped);
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::onEditCurrentFile() {
    if (!m_ssh || !m_ssh->isConnected()) {
        statusBar()->showMessage(tr("Bağlı değil"), 3000);
        return;
    }
    QModelIndex idx = m_fileView->currentIndex();
    if (!idx.isValid()) return;
    auto e = m_model->entryAt(idx.row()).toMap();
    QString name = e["name"].toString();
    if (name == ".." || e["isDir"].toBool()) return;
    qint64 size = e["size"].toLongLong();

    if (size > 5 * 1024 * 1024) {
        auto r = QMessageBox::question(this, tr("Büyük dosya"),
            tr("Dosya boyutu %1. Editör'de açmak istediğinden emin misin?")
                .arg(formatBytes(size)),
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }

    QString remote = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + name;

    // Sunucu adı (config'ten, yoksa host) — dosya sistemi için sanitize
    QString serverName = m_current.name.isEmpty() ? m_current.host : m_current.name;
    QString serverSafe = QString(serverName)
        .remove(QRegularExpression("[<>:\"/\\\\|?*]"));
    if (serverSafe.isEmpty()) serverSafe = "unknown";

    // %TEMP%\ssh-tool-edit\<filename>
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                     + "/ssh-tool-edit";
    QDir().mkpath(tmpDir);
    QString local = tmpDir + "/" + name;

    m_transferPrefix = QString("Edit indir: %1 ").arg(name);
    statusBar()->showMessage(m_transferPrefix + tr("indiriliyor..."));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!m_ssh->sftpDownload(remote, local)) {
        m_transferPrefix.clear();
        statusBar()->showMessage(tr("İndirme başarısız"), 5000);
        return;
    }
    m_transferPrefix.clear();

    // Orijinalin yedeğini al: %APPDATA%\local\ssh-tool\backups\<name>,<server>,<ts>
    QString backupsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + "/backups";
    QDir().mkpath(backupsDir);
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString backupName = QString("%1,%2,%3").arg(name, serverSafe, ts);
    QString backupPath = backupsDir + "/" + backupName;
    if (!QFile::copy(local, backupPath)) {
        statusBar()->showMessage(tr("Backup oluşturulamadı: %1").arg(backupPath), 5000);
        // devam et — düzenlemeyi engelleme
    } else {
        statusBar()->showMessage(tr("Backup: %1").arg(backupPath), 3000);
    }

    // Editör penceresi
    auto *editor = new EditorWindow(local, remote, serverSafe, this);
    connect(editor, &EditorWindow::saveRequested, this,
        [this, editor](const QString &lp, const QString &rp) {
            m_transferPrefix = QString("Save: %1 ").arg(QFileInfo(rp).fileName());
            statusBar()->showMessage(m_transferPrefix + tr("yükleniyor..."));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            // Bu upload sırasında patlayan sftpError'ı yakala
            QString errMsg;
            auto errConn = connect(m_ssh, &SshSession::sftpError, this,
                [&errMsg](const QString &m) { errMsg = m; },
                Qt::DirectConnection);
            bool ok = m_ssh->sftpUpload(lp, rp);
            QObject::disconnect(errConn);

            m_transferPrefix.clear();
            if (ok && errMsg.isEmpty()) {
                editor->onUploadCompleted();
                m_ssh->sftpListDir(m_cwd);
            } else {
                editor->onUploadFailed(errMsg.isEmpty() ? tr("Bilinmeyen hata") : errMsg);
            }
        });
    editor->show();
    editor->raise();
    editor->activateWindow();
}

void MainWindow::startDragOut() {
    if (!m_ssh || !m_ssh->isConnected()) return;
    auto *sel = m_fileView->selectionModel();
    if (!sel) return;
    QModelIndexList rows = sel->selectedRows();
    if (rows.isEmpty()) {
        QModelIndex cur = m_fileView->currentIndex();
        if (cur.isValid()) rows << cur;
    }
    if (rows.isEmpty()) return;

    // Hedef temp klasör
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                     + "/ssh-tool-drag";
    QDir().mkpath(tmpDir);

    // Indirilecek dosyaları topla
    struct Item { QString remote; QString local; QString name; };
    QList<Item> items;
    int skipped = 0;
    for (const QModelIndex &idx : rows) {
        auto e = m_model->entryAt(idx.row()).toMap();
        QString name = e["name"].toString();
        if (name == "..") { ++skipped; continue; }
        if (e["isDir"].toBool()) { ++skipped; continue; }    // klasör indirme şimdilik yok
        QString remote = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + name;
        QString local = tmpDir + "/" + name;
        items.push_back({remote, local, name});
    }
    if (items.isEmpty()) {
        statusBar()->showMessage(tr("Sürüklenecek dosya yok (klasörler atlanır)"), 4000);
        return;
    }

    // Senkron indir. processEvents ExcludeUserInputEvents ile —
    // mouse button release event'i ertelenir, drag.exec'e kadar tutulur.
    QList<QUrl> urls;
    const int N = items.size();
    for (int i = 0; i < N; ++i) {
        m_transferPrefix = QString("[%1/%2] %3 ").arg(i + 1).arg(N).arg(items[i].name);
        statusBar()->showMessage(m_transferPrefix + tr("indiriliyor (drag)..."));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        m_ssh->sftpDownload(items[i].remote, items[i].local);
        urls.append(QUrl::fromLocalFile(items[i].local));
    }
    m_transferPrefix.clear();
    statusBar()->showMessage(tr("Drag başlat: %1 dosya hazır").arg(N), 3000);

    auto *mime = new QMimeData;
    mime->setUrls(urls);
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
    // Temp dosyaları %TEMP%\ssh-tool-drag\ altında kalır.
    // Explorer kopyalamayı asenkron tamamlayabildiği için immediate silmiyoruz;
    // Windows %TEMP%'i kendisi temizler.
}

void MainWindow::onPathEntered() {
    if (!m_ssh || !m_ssh->isConnected()) return;
    QString p = m_pathEdit->text().trimmed();
    if (p.isEmpty()) p = "/";
    if (!p.startsWith('/')) p = "/" + p;
    m_ssh->sftpListDir(p);
}

void MainWindow::onPathUp() {
    if (!m_ssh || !m_ssh->isConnected()) return;
    if (m_cwd == "/" || m_cwd.isEmpty()) return;
    int slash = m_cwd.lastIndexOf('/');
    QString next = (slash <= 0) ? "/" : m_cwd.left(slash);
    m_ssh->sftpListDir(next);
}

void MainWindow::onSftpDoubleClicked(const QModelIndex &i) {
    auto e = m_model->entryAt(i.row()).toMap();
    QString name = e["name"].toString();
    if (e["isDir"].toBool()) {
        QString next;
        if (name == "..") {
            int slash = m_cwd.lastIndexOf('/');
            next = (slash <= 0) ? "/" : m_cwd.left(slash);
        } else {
            next = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + name;
        }
        m_ssh->sftpListDir(next);
    }
}

void MainWindow::onSftpContextMenu(const QPoint &pos) {
    QMenu menu(this);
    auto *aEdit     = menu.addAction(tr("Düzenle..."));
    auto *aDownload = menu.addAction(tr("Download..."));
    auto *aUpload   = menu.addAction(tr("Upload here..."));
    auto *aRefresh  = menu.addAction(tr("Refresh"));

    // Düzenle: sadece bir dosya seçiliyse aktif
    bool canEdit = false;
    QModelIndex idx = m_fileView->currentIndex();
    if (idx.isValid()) {
        auto e = m_model->entryAt(idx.row()).toMap();
        if (!e["isDir"].toBool() && e["name"].toString() != "..") canEdit = true;
    }
    aEdit->setEnabled(canEdit);

    QAction *act = menu.exec(m_fileView->viewport()->mapToGlobal(pos));
    if (!act) return;

    if (act == aEdit)    { onEditCurrentFile(); return; }
    if (act == aRefresh) {
        m_ssh->sftpListDir(m_cwd);
        return;
    }
    if (act == aDownload) {
        auto idx = m_fileView->currentIndex();
        if (!idx.isValid()) return;
        auto e = m_model->entryAt(idx.row()).toMap();
        if (e["isDir"].toBool()) return;
        QString remote = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + e["name"].toString();
        QString local = QFileDialog::getSaveFileName(this, "Save as", e["name"].toString());
        if (!local.isEmpty()) m_ssh->sftpDownload(remote, local);
        return;
    }
    if (act == aUpload) {
        QString local = QFileDialog::getOpenFileName(this, "Choose file to upload");
        if (local.isEmpty()) return;
        QString name = QFileInfo(local).fileName();
        QString remote = (m_cwd.endsWith('/') ? m_cwd : m_cwd + "/") + name;
        m_ssh->sftpUpload(local, remote);
    }
}
