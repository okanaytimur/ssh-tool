#pragma once
#include <QDialog>
#include "serverconfig.h"

class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;

class ConnectionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionsDialog(ServerStore *store, QWidget *parent = nullptr);

signals:
    void connectRequested(const ServerConfig &cfg);
    void saveRequested();   // store değişti, disk'e yazılması gerek

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void refreshList();
    void onCurrentRowChanged(int row);
    void onAdd();
    void onDelete();
    void onSave();
    void onConnect();
    void onBrowseKey();
    void commitCurrent();   // form alanlarını aktif kayda yaz

private:
    void writeFieldsFromUi(ServerConfig &s);
    void loadFieldsToUi(const ServerConfig &s);
    void hookAutoCommit();

    ServerStore *m_store;
    QListWidget *m_list;
    int m_currentIdx = -1;   // formdaki kayıt; -1 = yok
    bool m_loadingFields = false;   // loadFieldsToUi sırasında auto-commit'i susturmak için

    QLineEdit *m_name;
    QLineEdit *m_host;
    QSpinBox  *m_port;
    QLineEdit *m_user;
    QComboBox *m_authType;
    QLineEdit *m_password;
    QLineEdit *m_keyPath;
    QPushButton *m_browseKey;
    QLineEdit *m_keyPass;
    QLineEdit *m_group;
    QLineEdit *m_initDir;

    QPushButton *m_btnAdd, *m_btnDel, *m_btnSave, *m_btnConnect;
};
