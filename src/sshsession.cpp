#include "sshsession.h"
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <fcntl.h>
#include <QFile>
#include <QTimer>
#include <QVariantMap>
#include <QDebug>
#include <QCoreApplication>
#include <QEventLoop>

SshSession::SshSession(QObject *parent) : QObject(parent) {}

SshSession::~SshSession() {
    disconnect();
}

void SshSession::connectToServer(const ServerConfig &cfg) {
    if (m_connected) disconnect();

    m_ssh = ssh_new();
    if (!m_ssh) { emit connectError("ssh_new failed"); return; }

    QByteArray hostBa = cfg.host.toUtf8();
    QByteArray userBa = cfg.username.toUtf8();
    int port = cfg.port;

    ssh_options_set(m_ssh, SSH_OPTIONS_HOST, hostBa.constData());
    ssh_options_set(m_ssh, SSH_OPTIONS_USER, userBa.constData());
    ssh_options_set(m_ssh, SSH_OPTIONS_PORT, &port);
    long timeout = 15;
    ssh_options_set(m_ssh, SSH_OPTIONS_TIMEOUT, &timeout);

    int rc = ssh_connect(m_ssh);
    if (rc != SSH_OK) {
        emit connectError(QString("connect: %1").arg(ssh_get_error(m_ssh)));
        ssh_free(m_ssh); m_ssh = nullptr;
        return;
    }

    // TODO: known_hosts doğrulaması — ssh_session_is_known_server / ssh_session_update_known_hosts
    // Şu an ilk sürümde bypass; UI'da kullanıcıya host key fingerprint sorulmalı.

    if (!authenticate(cfg)) {
        emit connectError(m_lastError);
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh); m_ssh = nullptr;
        return;
    }

    // SFTP'yi hemen aç
    m_sftp = sftp_new(m_ssh);
    if (!m_sftp || sftp_init(m_sftp) != SSH_OK) {
        emit connectError(QString("sftp init: %1").arg(ssh_get_error(m_ssh)));
        return;
    }

    m_connected = true;
    emit connected();
}

bool SshSession::authenticate(const ServerConfig &cfg) {
    int rc = SSH_AUTH_DENIED;

    if (cfg.auth == "key" && !cfg.keyPath.isEmpty()) {
        ssh_key key = nullptr;
        QByteArray keyPathBa = cfg.keyPath.toUtf8();
        QByteArray passBa    = cfg.keyPassphrase.toUtf8();
        if (ssh_pki_import_privkey_file(keyPathBa.constData(),
                                        cfg.keyPassphrase.isEmpty() ? nullptr : passBa.constData(),
                                        nullptr, nullptr, &key) == SSH_OK) {
            rc = ssh_userauth_publickey(m_ssh, nullptr, key);
            ssh_key_free(key);
        } else {
            m_lastError = "Anahtar dosyası okunamadı veya passphrase yanlış";
            return false;
        }
    } else {
        QByteArray pwBa = cfg.password.toUtf8();
        rc = ssh_userauth_password(m_ssh, nullptr, pwBa.constData());
    }

    if (rc != SSH_AUTH_SUCCESS) {
        m_lastError = QString("auth: %1").arg(ssh_get_error(m_ssh));
        return false;
    }
    return true;
}

void SshSession::disconnect() {
    if (m_sftp) { sftp_free(m_sftp); m_sftp = nullptr; }
    if (m_chan) {
        ssh_channel_close(m_chan);
        ssh_channel_free(m_chan);
        m_chan = nullptr;
    }
    if (m_ssh) {
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
    }
    if (m_connected) {
        m_connected = false;
        emit disconnected();
    }
}

void SshSession::openShell(int cols, int rows) {
    if (!m_ssh) return;
    m_chan = ssh_channel_new(m_ssh);
    if (!m_chan) { emit shellClosed(); return; }
    if (ssh_channel_open_session(m_chan) != SSH_OK) { emit shellClosed(); return; }
    ssh_channel_request_pty_size(m_chan, "xterm-256color", cols, rows);
    if (ssh_channel_request_shell(m_chan) != SSH_OK) { emit shellClosed(); return; }

    // Periyodik poll (Qt event loop dostu). Production'da QSocketNotifier kullan.
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        if (!m_chan) { timer->stop(); timer->deleteLater(); return; }
        char buf[4096];
        int n = ssh_channel_read_nonblocking(m_chan, buf, sizeof(buf), 0);
        if (n > 0) emit shellData(QByteArray(buf, n));
        // stderr de oku
        n = ssh_channel_read_nonblocking(m_chan, buf, sizeof(buf), 1);
        if (n > 0) emit shellData(QByteArray(buf, n));
        if (ssh_channel_is_eof(m_chan)) {
            timer->stop(); timer->deleteLater();
            emit shellClosed();
        }
    });
    timer->start(20);
}

void SshSession::writeShell(const QByteArray &data) {
    if (m_chan) ssh_channel_write(m_chan, data.constData(), data.size());
}

void SshSession::resizePty(int cols, int rows) {
    if (m_chan) ssh_channel_change_pty_size(m_chan, cols, rows);
}

void SshSession::sftpListDir(const QString &path) {
    if (!m_sftp) { emit sftpError("SFTP açık değil"); return; }
    QByteArray pathBa = path.toUtf8();
    sftp_dir dir = sftp_opendir(m_sftp, pathBa.constData());
    if (!dir) { emit sftpError(QString("opendir: %1").arg(ssh_get_error(m_ssh))); return; }

    QVariantList entries;
    sftp_attributes attr;
    while ((attr = sftp_readdir(m_sftp, dir)) != nullptr) {
        QVariantMap e;
        e["name"]   = QString::fromUtf8(attr->name);
        e["size"]   = (qlonglong)attr->size;
        e["isDir"]  = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
        e["isLink"] = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
        e["mtime"]  = (qlonglong)attr->mtime;
        e["perms"]  = (quint32)attr->permissions;
        e["owner"]  = attr->owner ? QString::fromUtf8(attr->owner) : QString();
        entries.push_back(e);
        sftp_attributes_free(attr);
    }
    sftp_closedir(dir);
    emit sftpDirListed(path, entries);
}

bool SshSession::sftpDownload(const QString &remotePath, const QString &localPath) {
    if (!m_sftp) { emit sftpError("SFTP açık değil"); return false; }
    QByteArray rpBa = remotePath.toUtf8();
    sftp_file rf = sftp_open(m_sftp, rpBa.constData(), O_RDONLY, 0);
    if (!rf) { emit sftpError(QString("open remote: %1").arg(ssh_get_error(m_ssh))); return false; }

    QFile lf(localPath);
    if (!lf.open(QIODevice::WriteOnly)) {
        sftp_close(rf);
        emit sftpError("Local dosya açılamadı");
        return false;
    }

    sftp_attributes a = sftp_fstat(rf);
    qint64 total = a ? (qint64)a->size : -1;
    if (a) sftp_attributes_free(a);
    qint64 done = 0;
    char buf[32768];
    int n;
    bool readOK = true;
    while ((n = sftp_read(rf, buf, sizeof(buf))) > 0) {
        lf.write(buf, n);
        done += n;
        emit sftpProgress(done, total);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    if (n < 0) {
        emit sftpError(QString("read: %1").arg(ssh_get_error(m_ssh)));
        readOK = false;
    }
    lf.close();
    sftp_close(rf);
    if (readOK) emit sftpTransferDone(localPath, remotePath);
    return readOK;
}

bool SshSession::sftpUpload(const QString &localPath, const QString &remotePath) {
    if (!m_sftp) { emit sftpError("SFTP açık değil"); return false; }
    QFile lf(localPath);
    if (!lf.open(QIODevice::ReadOnly)) { emit sftpError("Local dosya okunamadı"); return false; }

    QByteArray rpBa = remotePath.toUtf8();
    sftp_file rf = sftp_open(m_sftp, rpBa.constData(),
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!rf) { emit sftpError(QString("open remote: %1").arg(ssh_get_error(m_ssh))); return false; }

    qint64 total = lf.size();
    qint64 done = 0;
    char buf[32768];
    bool writeOK = true;
    while (!lf.atEnd()) {
        qint64 n = lf.read(buf, sizeof(buf));
        if (n <= 0) break;
        if (sftp_write(rf, buf, n) != n) {
            emit sftpError(QString("write: %1").arg(ssh_get_error(m_ssh)));
            writeOK = false;
            break;
        }
        done += n;
        emit sftpProgress(done, total);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    sftp_close(rf);
    if (writeOK) emit sftpTransferDone(localPath, remotePath);
    return writeOK;
}
