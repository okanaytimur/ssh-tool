#include "syntaxhighlighter.h"
#include <QFileInfo>

// VS Code Dark+ benzeri palet
static const QColor kFgKeyword (0x56, 0x9c, 0xd6);   // mavi
static const QColor kFgType    (0x4e, 0xc9, 0xb0);   // turkuaz
static const QColor kFgString  (0xce, 0x91, 0x78);   // turuncu
static const QColor kFgNumber  (0xb5, 0xce, 0xa8);   // açık yeşil
static const QColor kFgComment (0x6a, 0x99, 0x55);   // yeşil
static const QColor kFgFunc    (0xdc, 0xdc, 0xaa);   // krem
static const QColor kFgOp      (0xd4, 0xd4, 0xd4);   // varsayılan
static const QColor kFgPreproc (0xc5, 0x86, 0xc0);   // mor
static const QColor kFgKey     (0x9c, 0xdc, 0xfe);   // açık mavi (YAML/JSON key, XML attr)

static QTextCharFormat fmt(const QColor &c, bool bold = false, bool italic = false) {
    QTextCharFormat f;
    f.setForeground(c);
    if (bold) f.setFontWeight(QFont::Bold);
    if (italic) f.setFontItalic(true);
    return f;
}

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {}

QString SyntaxHighlighter::detectLangByFilename(const QString &filename) {
    QFileInfo fi(filename);
    QString name = fi.fileName().toLower();
    QString ext  = fi.suffix().toLower();

    // Bilinen tam dosya adları
    if (name == "dockerfile" || name.startsWith("dockerfile."))   return "dockerfile";
    if (name == "makefile"   || name == "gnumakefile")            return "makefile";
    if (name == "cmakelists.txt")                                 return "cmake";
    if (name == ".bashrc" || name == ".bash_profile"
        || name == ".profile" || name == ".zshrc")                return "bash";
    if (name == ".gitignore" || name == ".dockerignore")          return "text";

    static const QMap<QString, QString> byExt = {
        {"py","python"},{"pyw","python"},
        {"js","javascript"},{"mjs","javascript"},{"cjs","javascript"},
        {"ts","javascript"},{"tsx","javascript"},{"jsx","javascript"},
        {"yml","yaml"},{"yaml","yaml"},
        {"json","json"},
        {"ini","ini"},{"conf","ini"},{"cfg","ini"},{"properties","ini"},
        {"toml","ini"},
        {"sh","bash"},{"bash","bash"},
        {"xml","xml"},{"html","xml"},{"htm","xml"},{"svg","xml"},
        {"c","cpp"},{"h","cpp"},{"cpp","cpp"},{"hpp","cpp"},
        {"cc","cpp"},{"cxx","cpp"},{"hh","cpp"},
        {"cs","cpp"}, // C# için C/C++ kuralları yeterli
        {"java","cpp"},
        {"go","cpp"},
        {"rs","cpp"},
        {"sql","sql"},
        {"cmake","cmake"},
    };
    auto it = byExt.find(ext);
    if (it != byExt.end()) return it.value();
    return "text";
}

void SyntaxHighlighter::setLanguageForFilename(const QString &filename) {
    m_rules.clear();
    m_hasBlock = false;
    QString lang = detectLangByFilename(filename);
    buildRules(lang);
    rehighlight();
}

void SyntaxHighlighter::buildRules(const QString &lang) {
    auto add = [&](const QString &pat, const QTextCharFormat &f,
                   QRegularExpression::PatternOptions opt = QRegularExpression::NoPatternOption) {
        m_rules.push_back({QRegularExpression(pat, opt), f});
    };

    if (lang == "python") {
        for (const QString &kw : {
            "and","as","assert","async","await","break","class","continue","def","del",
            "elif","else","except","finally","for","from","global","if","import","in",
            "is","lambda","nonlocal","not","or","pass","raise","return","try","while",
            "with","yield","True","False","None","self","cls"
        }) add("\\b" + kw + "\\b", fmt(kFgKeyword, true));
        add("#[^\\n]*", fmt(kFgComment, false, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'",     fmt(kFgString));
        add("\\b[0-9]+(?:\\.[0-9]+)?\\b",         fmt(kFgNumber));
        add("\\bdef\\s+([A-Za-z_][A-Za-z0-9_]*)", fmt(kFgFunc));
    }
    else if (lang == "javascript") {
        for (const QString &kw : {
            "break","case","catch","class","const","continue","debugger","default","delete","do",
            "else","export","extends","false","finally","for","function","if","import","in","instanceof",
            "new","null","of","return","super","switch","this","throw","true","try","typeof","var","void",
            "while","with","yield","let","async","await","static"
        }) add("\\b" + kw + "\\b", fmt(kFgKeyword, true));
        add("//[^\\n]*",                          fmt(kFgComment, false, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'",     fmt(kFgString));
        add("`[^`\\\\]*(?:\\\\.[^`\\\\]*)*`",     fmt(kFgString));
        add("\\b[0-9]+(?:\\.[0-9]+)?\\b",         fmt(kFgNumber));
        m_blockStart  = QRegularExpression("/\\*");
        m_blockEnd    = QRegularExpression("\\*/");
        m_blockFormat = fmt(kFgComment, false, true);
        m_hasBlock = true;
    }
    else if (lang == "yaml") {
        // anahtar: değer
        add("^[ \\t]*([A-Za-z0-9_.\\-]+)\\s*:", fmt(kFgKey, true));
        add("^[ \\t]*-[ \\t]",                  fmt(kFgKeyword));
        add("#[^\\n]*",                         fmt(kFgComment, false, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^']*'",                          fmt(kFgString));
        add("\\b(true|false|null|yes|no|on|off)\\b", fmt(kFgPreproc, true),
            QRegularExpression::CaseInsensitiveOption);
        add("\\b[0-9]+(?:\\.[0-9]+)?\\b",       fmt(kFgNumber));
    }
    else if (lang == "json") {
        // "key":
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"\\s*:", fmt(kFgKey, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"",      fmt(kFgString));
        add("\\b(true|false|null)\\b",                 fmt(kFgPreproc, true));
        add("\\b-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\b", fmt(kFgNumber));
    }
    else if (lang == "ini") {
        add("^\\s*\\[[^\\]]*\\]", fmt(kFgKey, true));
        add("^[ \\t]*[A-Za-z0-9_.\\-]+\\s*=", fmt(kFgKey));
        add(";[^\\n]*", fmt(kFgComment, false, true));
        add("#[^\\n]*", fmt(kFgComment, false, true));
        add("\"[^\"]*\"", fmt(kFgString));
        add("'[^']*'",    fmt(kFgString));
        add("\\b[0-9]+(?:\\.[0-9]+)?\\b", fmt(kFgNumber));
    }
    else if (lang == "bash") {
        for (const QString &kw : {
            "if","then","else","elif","fi","case","esac","for","while","until","do","done",
            "function","in","return","break","continue","local","export","readonly","unset",
            "alias","source","eval","exec","trap","shift","set"
        }) add("\\b" + kw + "\\b", fmt(kFgKeyword, true));
        add("#[^\\n]*",                           fmt(kFgComment, false, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^']*'",                            fmt(kFgString));
        add("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?",  fmt(kFgPreproc));
        add("\\b[0-9]+\\b",                       fmt(kFgNumber));
    }
    else if (lang == "xml") {
        add("</?\\s*[A-Za-z][A-Za-z0-9_:\\-]*",  fmt(kFgKeyword, true));
        add("/?>",                                fmt(kFgKeyword, true));
        add("\\b([A-Za-z_:][A-Za-z0-9_:\\-]*)=",  fmt(kFgKey));
        add("\"[^\"]*\"",                         fmt(kFgString));
        add("'[^']*'",                            fmt(kFgString));
        m_blockStart  = QRegularExpression("<!--");
        m_blockEnd    = QRegularExpression("-->");
        m_blockFormat = fmt(kFgComment, false, true);
        m_hasBlock = true;
    }
    else if (lang == "cpp") {
        for (const QString &kw : {
            "alignas","alignof","and","auto","bitand","bitor","bool","break","case","catch",
            "char","class","compl","const","constexpr","const_cast","continue","decltype",
            "default","delete","do","double","dynamic_cast","else","enum","explicit","export",
            "extern","false","float","for","friend","goto","if","inline","int","long","mutable",
            "namespace","new","noexcept","not","nullptr","operator","or","private","protected",
            "public","register","reinterpret_cast","return","short","signed","sizeof","static",
            "static_assert","static_cast","struct","switch","template","this","thread_local",
            "throw","true","try","typedef","typeid","typename","union","unsigned","using",
            "virtual","void","volatile","wchar_t","while","xor","override","final"
        }) add("\\b" + kw + "\\b", fmt(kFgKeyword, true));
        add("\\b[A-Z][A-Za-z0-9_]*\\b",           fmt(kFgType));
        add("//[^\\n]*",                          fmt(kFgComment, false, true));
        add("^\\s*#[A-Za-z]+",                    fmt(kFgPreproc, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'",     fmt(kFgString));
        add("\\b[0-9]+(?:\\.[0-9]+)?(?:[fFLuU]*)\\b", fmt(kFgNumber));
        m_blockStart  = QRegularExpression("/\\*");
        m_blockEnd    = QRegularExpression("\\*/");
        m_blockFormat = fmt(kFgComment, false, true);
        m_hasBlock = true;
    }
    else if (lang == "sql") {
        for (const QString &kw : {
            "SELECT","FROM","WHERE","INSERT","UPDATE","DELETE","CREATE","DROP","ALTER","TABLE",
            "INDEX","VIEW","JOIN","INNER","OUTER","LEFT","RIGHT","FULL","ON","AS","AND","OR","NOT",
            "IN","IS","NULL","LIKE","ORDER","BY","GROUP","HAVING","LIMIT","OFFSET","DISTINCT",
            "UNION","INTO","VALUES","SET","CASE","WHEN","THEN","ELSE","END","DATABASE","SCHEMA",
            "PRIMARY","KEY","FOREIGN","REFERENCES","CONSTRAINT","DEFAULT","UNIQUE","CHECK",
            "BEGIN","COMMIT","ROLLBACK","TRANSACTION"
        }) add("\\b" + kw + "\\b", fmt(kFgKeyword, true),
               QRegularExpression::CaseInsensitiveOption);
        add("--[^\\n]*",                          fmt(kFgComment, false, true));
        add("'[^']*(?:''[^']*)*'",                fmt(kFgString));
        add("\"[^\"]*\"",                         fmt(kFgString));
        add("\\b[0-9]+(?:\\.[0-9]+)?\\b",         fmt(kFgNumber));
        m_blockStart  = QRegularExpression("/\\*");
        m_blockEnd    = QRegularExpression("\\*/");
        m_blockFormat = fmt(kFgComment, false, true);
        m_hasBlock = true;
    }
    else if (lang == "dockerfile") {
        for (const QString &kw : {
            "FROM","RUN","CMD","LABEL","MAINTAINER","EXPOSE","ENV","ADD","COPY","ENTRYPOINT",
            "VOLUME","USER","WORKDIR","ARG","ONBUILD","STOPSIGNAL","HEALTHCHECK","SHELL"
        }) add("^\\s*" + kw + "\\b", fmt(kFgKeyword, true));
        add("#[^\\n]*",                           fmt(kFgComment, false, true));
        add("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\"", fmt(kFgString));
        add("'[^']*'",                            fmt(kFgString));
        add("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?",  fmt(kFgPreproc));
    }
    else if (lang == "makefile" || lang == "cmake") {
        add("^[A-Za-z0-9_.\\-]+\\s*:", fmt(kFgKey, true));
        add("\\$\\(?[A-Za-z_][A-Za-z0-9_]*\\)?", fmt(kFgPreproc));
        add("#[^\\n]*", fmt(kFgComment, false, true));
        add("\"[^\"]*\"", fmt(kFgString));
    }
    // "text" / unknown — kural yok, düz beyaz
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
    for (const Rule &r : m_rules) {
        QRegularExpressionMatchIterator it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            int start = m.capturedStart();
            int len   = m.capturedLength();
            // Eğer regex'te capture group #1 varsa onu boyamayı dene
            if (m.lastCapturedIndex() >= 1) {
                int gs = m.capturedStart(1);
                int gl = m.capturedLength(1);
                if (gs >= 0 && gl > 0) { start = gs; len = gl; }
            }
            setFormat(start, len, r.format);
        }
    }

    // Multi-line block (C-style /* */, XML <!-- -->)
    if (m_hasBlock) {
        setCurrentBlockState(0);
        int startIdx = 0;
        if (previousBlockState() != 1) {
            auto m = m_blockStart.match(text);
            startIdx = m.hasMatch() ? m.capturedStart() : -1;
        }
        while (startIdx >= 0) {
            auto endM = m_blockEnd.match(text, startIdx);
            int endIdx, len;
            if (!endM.hasMatch()) {
                setCurrentBlockState(1);
                len = text.length() - startIdx;
                setFormat(startIdx, len, m_blockFormat);
                break;
            } else {
                endIdx = endM.capturedStart();
                len = endIdx - startIdx + endM.capturedLength();
                setFormat(startIdx, len, m_blockFormat);
                auto nextM = m_blockStart.match(text, startIdx + len);
                startIdx = nextM.hasMatch() ? nextM.capturedStart() : -1;
            }
        }
    }
}
