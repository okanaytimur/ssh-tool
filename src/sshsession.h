#pragma once
#include <QObject>
#include <QThread>
#include <QByteArray>
#include "serverconfig.h"

// libssh forward declarations (header bağımlılığını cpp'ye it)
struct ssh_session_struct;
struct ssh_channel_struct;
struct sftp_session_struct;
typedef ssh_session_struct* ssh_session;
typedef ssh_channel_struct* ssh_channel;
typedef sftp_session_struct* sftp_session;

class SshSession : public QObject {
    Q_OBJECT
public:
    explicit SshSession(QObject *parent = nullptr);
    ~SshSession();

    bool isConnected() const { return m_connected; }

public slots:
    void connectToServer(const ServerConfig &cfg);
    void disconnect();

    // Shell channel
    void openShell(int cols, int rows);
    void writeShell(const QByteArray &data);
    void resizePty(int cols, int rows);

    // SFTP — bool return: true = başarılı
    void sftpListDir(const QString &path);
    bool sftpDownload(const QString &remotePath, const QString &localPath);
    bool sftpUpload(const QString &localPath, const QString &remotePath);

signals:
    void connected();
    void connectError(const QString &msg);
    void disconnected();

    void shellData(const QByteArray &data);
    void shellClosed();

    void sftpDirListed(const QString &path, const QVariantList &entries);
    void sftpError(const QString &msg);
    void sftpProgress(qint64 done, qint64 total);
    void sftpTransferDone(const QString &localPath, const QString &remotePath);

private:
    void pollLoop();             // shell verisini okumak için periyodik
    bool authenticate(const ServerConfig &cfg);

    ssh_session  m_ssh   = nullptr;
    ssh_channel  m_chan  = nullptr;
    sftp_session m_sftp  = nullptr;
    bool         m_connected = false;
    QString      m_lastError;
};
