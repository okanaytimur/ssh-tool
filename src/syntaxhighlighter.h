#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);
    void setLanguageForFilename(const QString &filename);

    // "" / "text" → renksiz
    static QString detectLangByFilename(const QString &filename);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<Rule> m_rules;

    // C-style /* ... */ veya HTML/XML <!-- ... --> için
    QRegularExpression m_blockStart;
    QRegularExpression m_blockEnd;
    QTextCharFormat    m_blockFormat;
    bool               m_hasBlock = false;

    void buildRules(const QString &lang);
};
