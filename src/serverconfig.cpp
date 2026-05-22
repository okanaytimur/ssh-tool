#include "serverconfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

QJsonObject ServerConfig::toJson() const {
    QJsonObject o;
    o["id"] = id;
    o["name"] = name;
    o["host"] = host;
    o["port"] = port;
    o["username"] = username;
    o["auth"] = auth;
    o["password"] = password;        // NOT: prod'da DPAPI/Credential Manager ile şifrele
    o["keyPath"] = keyPath;
    o["keyPassphrase"] = keyPassphrase;
    o["initialDirectory"] = initialDirectory;
    o["color"] = color;
    o["group"] = group;
    return o;
}

ServerConfig ServerConfig::fromJson(const QJsonObject &o) {
    ServerConfig s;
    s.id               = o.value("id").toString();
    if (s.id.isEmpty()) s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name             = o.value("name").toString();
    s.host             = o.value("host").toString();
    s.port             = o.value("port").toInt(22);
    s.username         = o.value("username").toString();
    s.auth             = o.value("auth").toString("password");
    s.password         = o.value("password").toString();
    s.keyPath          = o.value("keyPath").toString();
    s.keyPassphrase    = o.value("keyPassphrase").toString();
    s.initialDirectory = o.value("initialDirectory").toString("/");
    s.color            = o.value("color").toString();
    s.group            = o.value("group").toString();
    return s;
}

bool ServerStore::load(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;
    auto root = doc.object();
    m_version = root.value("version").toInt(1);
    m_servers.clear();
    for (auto v : root.value("servers").toArray()) {
        m_servers.push_back(ServerConfig::fromJson(v.toObject()));
    }
    return true;
}

bool ServerStore::save(const QString &path) const {
    QJsonObject root;
    root["version"] = m_version;
    QJsonArray arr;
    for (const auto &s : m_servers) arr.push_back(s.toJson());
    root["servers"] = arr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
