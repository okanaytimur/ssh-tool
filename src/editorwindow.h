#pragma once
#include <QMainWindow>

class QPlainTextEdit;
class QAction;
class QLabel;
class SyntaxHighlighter;

class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    EditorWindow(const QString &localPath,
                 const QString &remotePath,
                 const QString &serverName,
                 QWidget *parent = nullptr);
    ~EditorWindow() override;

    bool isModified() const;

signals:
    // Editör "Save" istedi: <localPath, remotePath>
    // Çağıran taraf (MainWindow) backup'ı alıp upload'u yapacak.
    void saveRequested(const QString &localPath, const QString &remotePath);

public slots:
    // Upload başarılı olursa MainWindow bunu çağırır → modified flag temizlenir
    void onUploadCompleted();
    void onUploadFailed(const QString &msg);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onTextChanged();
    void onSave();
    void onSaveAndClose();

private:
    void buildUi();
    bool loadFromDisk();
    bool flushToDisk();
    void updateTitle();

    QString             m_localPath;
    QString             m_remotePath;
    QString             m_serverName;
    QPlainTextEdit     *m_edit  = nullptr;
    SyntaxHighlighter  *m_hl    = nullptr;
    QLabel             *m_lblPath = nullptr;
    QLabel             *m_lblPos  = nullptr;
    QAction            *m_actSave = nullptr;

    bool m_modified = false;
    bool m_closeAfterSave = false;
};
