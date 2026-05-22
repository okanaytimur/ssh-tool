#pragma once
#include <QWidget>
#include <QFont>

extern "C" {
#include <vterm.h>
}

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    // Sunucudan gelen veriyi vterm'e besle
    void appendOutput(const QByteArray &data);

signals:
    // Kullanıcı klavyeden bir şey yazdı, SSH'a gitsin
    void inputReady(const QByteArray &data);

    // PTY boyutu değişti (kolon, satır)
    void resized(int cols, int rows);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    bool focusNextPrevChild(bool /*next*/) override { return false; } // Tab terminal'e gitsin

private:
    void initVterm(int rows, int cols);
    void shutdownVterm();
    void seedPalette();

    QColor toQColor(const VTermColor *col, bool isFg) const;
    QString selectedText() const;
    void clampSel();
    void normalizeSel(VTermPos &lo, VTermPos &hi) const;

    // libvterm callback thunks
    static int   cb_damage(VTermRect r, void *u);
    static int   cb_moverect(VTermRect d, VTermRect s, void *u);
    static int   cb_movecursor(VTermPos p, VTermPos old, int visible, void *u);
    static int   cb_settermprop(VTermProp p, VTermValue *v, void *u);
    static int   cb_bell(void *u);
    static int   cb_sb_pushline(int cols, const VTermScreenCell *cells, void *u);
    static int   cb_sb_popline(int cols, VTermScreenCell *cells, void *u);
    static void  cb_output(const char *s, size_t len, void *u);

    VTerm       *m_vt     = nullptr;
    VTermScreen *m_screen = nullptr;

    QFont m_font;
    int   m_cellW = 8;
    int   m_cellH = 16;
    int   m_baseline = 12;
    int   m_rows = 24;
    int   m_cols = 80;

    VTermPos m_cursor = {0, 0};
    bool     m_cursorVisible = true;
    bool     m_hasFocus = false;

    // Seçim (cell coords). row=-1 → seçim yok.
    bool     m_selecting = false;
    VTermPos m_selStart = {-1, -1};
    VTermPos m_selEnd   = {-1, -1};

    QColor m_bg = QColor(0x1e, 0x1e, 0x1e);
    QColor m_fg = QColor(0xd4, 0xd4, 0xd4);
};
