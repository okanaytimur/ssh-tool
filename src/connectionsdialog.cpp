#include "connectionsdialog.h"
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QLabel>
#include <QUuid>
#include <QMessageBox>
#include <QCloseEvent>

ConnectionsDialog::ConnectionsDialog(ServerStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setWindowTitle(tr("Connections"));
    resize(720, 460);

    m_list = new QListWidget(this);

    m_name = new QLineEdit(this);
    m_host = new QLineEdit(this);
    m_port = new QSpinBox(this); m_port->setRange(1, 65535); m_port->setValue(22);
    m_user = new QLineEdit(this);
    m_authType = new QComboBox(this);
    m_authType->addItems({"password", "key"});
    m_password = new QLineEdit(this); m_password->setEchoMode(QLineEdit::Password);
    m_keyPath = new QLineEdit(this);
    m_browseKey = new QPushButton("...", this); m_browseKey->setFixedWidth(28);
    m_keyPass = new QLineEdit(this); m_keyPass->setEchoMode(QLineEdit::Password);
    m_group = new QLineEdit(this);
    m_initDir = new QLineEdit(this); m_initDir->setText("/");

    auto *form = new QFormLayout;
    form->addRow(tr("Name"), m_name);
    form->addRow(tr("Host"), m_host);
    form->addRow(tr("Port"), m_port);
    form->addRow(tr("User"), m_user);
    form->addRow(tr("Auth"), m_authType);
    form->addRow(tr("Password"), m_password);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(m_keyPath); keyRow->addWidget(m_browseKey);
    auto *keyWrap = new QWidget(this); keyWrap->setLayout(keyRow);
    form->addRow(tr("Key file"), keyWrap);
    form->addRow(tr("Passphrase"), m_keyPass);
    form->addRow(tr("Group"), m_group);
    form->addRow(tr("Initial dir"), m_initDir);

    m_btnAdd     = new QPushButton(tr("Add"), this);
    m_btnDel     = new QPushButton(tr("Delete"), this);
    m_btnSave    = new QPushButton(tr("Save"), this);
    m_btnConnect = new QPushButton(tr("Connect"), this);
    m_btnConnect->setDefault(true);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_btnAdd);
    btnRow->addWidget(m_btnDel);
    btnRow->addStretch();
    btnRow->addWidget(m_btnSave);
    btnRow->addWidget(m_btnConnect);

    auto *right = new QVBoxLayout;
    right->addLayout(form);
    right->addStretch();
    right->addLayout(btnRow);

    auto *rightWrap = new QWidget(this); rightWrap->setLayout(right);

    auto *split = new QSplitter(this);
    split->addWidget(m_list);
    split->addWidget(rightWrap);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);

    auto *root = new QVBoxLayout(this);
    root->addWidget(split);

    connect(m_list, &QListWidget::currentRowChanged, this, &ConnectionsDialog::onCurrentRowChanged);
    connect(m_btnAdd,     &QPushButton::clicked, this, &ConnectionsDialog::onAdd);
    connect(m_btnDel,     &QPushButton::clicked, this, &ConnectionsDialog::onDelete);
    connect(m_btnSave,    &QPushButton::clicked, this, &ConnectionsDialog::onSave);
    connect(m_btnConnect, &QPushButton::clicked, this, &ConnectionsDialog::onConnect);
    connect(m_browseKey,  &QPushButton::clicked, this, &ConnectionsDialog::onBrowseKey);

    hookAutoCommit();
    refreshList();
}

void ConnectionsDialog::hookAutoCommit() {
    auto trig = [this]{ commitCurrent(); };
    connect(m_name,     &QLineEdit::textEdited,         this, trig);
    connect(m_host,     &QLineEdit::textEdited,         this, trig);
    connect(m_port,     QOverload<int>::of(&QSpinBox::valueChanged), this, trig);
    connect(m_user,     &QLineEdit::textEdited,         this, trig);
    connect(m_authType, &QComboBox::currentTextChanged, this, trig);
    connect(m_password, &QLineEdit::textEdited,         this, trig);
    connect(m_keyPath,  &QLineEdit::textEdited,         this, trig);
    connect(m_keyPass,  &QLineEdit::textEdited,         this, trig);
    connect(m_group,    &QLineEdit::textEdited,         this, trig);
    connect(m_initDir,  &QLineEdit::textEdited,         this, trig);
}

void ConnectionsDialog::commitCurrent() {
    if (m_loadingFields) return;
    if (m_currentIdx < 0 || m_currentIdx >= m_store->servers().size()) return;
    ServerConfig s = m_store->servers().at(m_currentIdx);
    writeFieldsFromUi(s);
    m_store->update(m_currentIdx, s);
    // Liste etiketini güncelle (Name değiştiyse görünür olsun)
    QString label = s.name.isEmpty() ? s.host : s.name;
    if (!s.group.isEmpty()) label = QString("[%1] %2").arg(s.group, label);
    if (auto *item = m_list->item(m_currentIdx)) item->setText(label);
    emit saveRequested();
}

void ConnectionsDialog::closeEvent(QCloseEvent *e) {
    commitCurrent();
    emit saveRequested();
    QDialog::closeEvent(e);
}

void ConnectionsDialog::refreshList() {
    m_list->clear();
    for (const auto &s : m_store->servers()) {
        QString label = s.name.isEmpty() ? s.host : s.name;
        if (!s.group.isEmpty()) label = QString("[%1] %2").arg(s.group, label);
        m_list->addItem(label);
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void ConnectionsDialog::onCurrentRowChanged(int row) {
    // Önce eski satırı commit'le
    commitCurrent();
    m_currentIdx = (row >= 0 && row < m_store->servers().size()) ? row : -1;
    if (m_currentIdx >= 0) loadFieldsToUi(m_store->servers().at(m_currentIdx));
}

void ConnectionsDialog::loadFieldsToUi(const ServerConfig &s) {
    m_loadingFields = true;
    m_name->setText(s.name);
    m_host->setText(s.host);
    m_port->setValue(s.port);
    m_user->setText(s.username);
    m_authType->setCurrentText(s.auth);
    m_password->setText(s.password);
    m_keyPath->setText(s.keyPath);
    m_keyPass->setText(s.keyPassphrase);
    m_group->setText(s.group);
    m_initDir->setText(s.initialDirectory);
    m_loadingFields = false;
}

void ConnectionsDialog::writeFieldsFromUi(ServerConfig &s) {
    s.name             = m_name->text();
    s.host             = m_host->text();
    s.port             = m_port->value();
    s.username         = m_user->text();
    s.auth             = m_authType->currentText();
    s.password         = m_password->text();
    s.keyPath          = m_keyPath->text();
    s.keyPassphrase    = m_keyPass->text();
    s.group            = m_group->text();
    s.initialDirectory = m_initDir->text();
}

void ConnectionsDialog::onAdd() {
    commitCurrent();   // önce mevcut formu kaybetme
    ServerConfig s;
    s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = tr("New server");
    s.host = "127.0.0.1";
    s.port = 22;
    s.auth = "password";
    s.initialDirectory = "/";
    m_store->add(s);
    refreshList();
    m_list->setCurrentRow(m_store->servers().size() - 1);
    emit saveRequested();
}

void ConnectionsDialog::onDelete() {
    int idx = m_list->currentRow();
    if (idx < 0) return;
    if (QMessageBox::question(this, tr("Delete"),
            tr("Delete this server entry?")) != QMessageBox::Yes) return;
    m_currentIdx = -1;   // commit'i susturalım, kayıt silindi
    m_store->remove(idx);
    refreshList();
    emit saveRequested();
}

void ConnectionsDialog::onSave() {
    commitCurrent();   // auto-commit zaten çoğu durumu kapatır, bu explicit trigger
    emit saveRequested();
}

void ConnectionsDialog::onConnect() {
    commitCurrent();
    if (m_currentIdx < 0) return;
    emit saveRequested();
    emit connectRequested(m_store->servers().at(m_currentIdx));
    accept();
}

void ConnectionsDialog::onBrowseKey() {
    QString p = QFileDialog::getOpenFileName(this, tr("Select private key"),
                    QString(), tr("All files (*)"));
    if (!p.isEmpty()) m_keyPath->setText(p);
}
