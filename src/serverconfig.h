#pragma once
#include <QString>
#include <QVector>
#include <QJsonObject>

struct ServerConfig {
    QString id;
    QString name;
    QString host;
    int     port = 22;
    QString username;
    QString auth;           // "password" veya "key"
    QString password;
    QString keyPath;
    QString keyPassphrase;
    QString initialDirectory = "/";
    QString color;
    QString group;

    QJsonObject toJson() const;
    static ServerConfig fromJson(const QJsonObject &o);
};

class ServerStore {
public:
    bool load(const QString &path);
    bool save(const QString &path) const;

    const QVector<ServerConfig>& servers() const { return m_servers; }
    void add(const ServerConfig &s) { m_servers.push_back(s); }
    void update(int idx, const ServerConfig &s) { m_servers[idx] = s; }
    void remove(int idx) { m_servers.remove(idx); }

private:
    QVector<ServerConfig> m_servers;
    int m_version = 1;
};
