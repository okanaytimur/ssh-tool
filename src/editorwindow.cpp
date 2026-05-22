#include "editorwindow.h"
#include "syntaxhighlighter.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>

EditorWindow::EditorWindow(const QString &localPath,
                           const QString &remotePath,
                           const QString &serverName,
                           QWidget *parent)
    : QMainWindow(parent)
    , m_localPath(localPath)
    , m_remotePath(remotePath)
    , m_serverName(serverName)
{
    setAttribute(Qt::WA_DeleteOnClose);
    resize(960, 680);
    buildUi();
    loadFromDisk();
    updateTitle();
}

EditorWindow::~EditorWindow() = default;

bool EditorWindow::isModified() const { return m_modified; }

void EditorWindow::buildUi() {
    m_edit = new QPlainTextEdit(this);
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(10);
    m_edit->setFont(f);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edit->setTabStopDistance(QFontMetricsF(f).horizontalAdvance(QChar(' ')) * 4);
    m_edit->setStyleSheet(
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
        "selection-background-color: #264f78; }");
    setCentralWidget(m_edit);

    m_hl = new SyntaxHighlighter(m_edit->document());
    m_hl->setLanguageForFilename(m_remotePath);

    auto *tb = addToolBar(tr("Editor"));
    tb->setMovable(false);
    m_actSave = tb->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                              tr("Save (Ctrl+S)"));
    m_actSave->setShortcut(QKeySequence::Save);
    connect(m_actSave, &QAction::triggered, this, &EditorWindow::onSave);

    auto *actSaveClose = tb->addAction(tr("Save && Close"));
    actSaveClose->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actSaveClose, &QAction::triggered, this, &EditorWindow::onSaveAndClose);

    tb->addSeparator();
    auto *actClose = tb->addAction(style()->standardIcon(QStyle::SP_DialogCloseButton),
                                   tr("Close"));
    connect(actClose, &QAction::triggered, this, &QWidget::close);

    m_lblPath = new QLabel(this);
    m_lblPos  = new QLabel(this);
    statusBar()->addWidget(m_lblPath, 1);
    statusBar()->addPermanentWidget(m_lblPos);

    connect(m_edit, &QPlainTextEdit::textChanged, this, &EditorWindow::onTextChanged);
    connect(m_edit, &QPlainTextEdit::cursorPositionChanged, this, [this]{
        auto c = m_edit->textCursor();
        int line = c.blockNumber() + 1;
        int col  = c.positionInBlock() + 1;
        m_lblPos->setText(QString("Ln %1, Col %2").arg(line).arg(col));
    });
}

bool EditorWindow::loadFromDisk() {
    QFile f(m_localPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Editor"),
            tr("Local dosya açılamadı:\n%1").arg(m_localPath));
        return false;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    m_edit->blockSignals(true);
    m_edit->setPlainText(ts.readAll());
    m_edit->blockSignals(false);
    m_modified = false;
    m_lblPos->setText("Ln 1, Col 1");
    return true;
}

bool EditorWindow::flushToDisk() {
    QFile f(m_localPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Editor"),
            tr("Local dosyaya yazılamadı:\n%1").arg(m_localPath));
        return false;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    ts << m_edit->toPlainText();
    return true;
}

void EditorWindow::updateTitle() {
    QFileInfo fi(m_remotePath);
    QString star = m_modified ? " *" : "";
    setWindowTitle(QString("%1%2 — %3 [%4]")
                   .arg(fi.fileName(), star, m_serverName, m_remotePath));
    m_lblPath->setText(QString("%1 @ %2").arg(m_remotePath, m_serverName));
}

void EditorWindow::onTextChanged() {
    if (!m_modified) {
        m_modified = true;
        updateTitle();
    }
}

void EditorWindow::onSave() {
    if (!flushToDisk()) return;
    emit saveRequested(m_localPath, m_remotePath);
    // MainWindow upload sonrası onUploadCompleted/Failed çağıracak
}

void EditorWindow::onSaveAndClose() {
    m_closeAfterSave = true;
    onSave();
}

void EditorWindow::onUploadCompleted() {
    m_modified = false;
    updateTitle();
    statusBar()->showMessage(tr("Sunucuya kaydedildi"), 3000);
    if (m_closeAfterSave) {
        m_closeAfterSave = false;
        close();
    }
}

void EditorWindow::onUploadFailed(const QString &msg) {
    m_closeAfterSave = false;
    QMessageBox::warning(this, tr("Save failed"),
        tr("Sunucuya kaydedilemedi:\n%1").arg(msg));
}

void EditorWindow::closeEvent(QCloseEvent *e) {
    if (!m_modified) { e->accept(); return; }
    auto ret = QMessageBox::question(this, tr("Kaydedilmemiş değişiklikler"),
        tr("Dosyada kaydedilmemiş değişiklikler var.\nKapatmadan önce kaydetmek ister misin?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (ret == QMessageBox::Cancel) { e->ignore(); return; }
    if (ret == QMessageBox::Save) {
        m_closeAfterSave = true;
        onSave();
        e->ignore();   // upload sonrası onUploadCompleted close eder
        return;
    }
    e->accept();
}
