#include "terminalwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QString>

// Standart 16 ANSI rengi (xterm-uyumlu)
static const QRgb kAnsiPalette[16] = {
    qRgb(0x00,0x00,0x00), qRgb(0xcd,0x00,0x00), qRgb(0x00,0xcd,0x00), qRgb(0xcd,0xcd,0x00),
    qRgb(0x1e,0x90,0xff), qRgb(0xcd,0x00,0xcd), qRgb(0x00,0xcd,0xcd), qRgb(0xe5,0xe5,0xe5),
    qRgb(0x7f,0x7f,0x7f), qRgb(0xff,0x00,0x00), qRgb(0x00,0xff,0x00), qRgb(0xff,0xff,0x00),
    qRgb(0x5c,0x5c,0xff), qRgb(0xff,0x00,0xff), qRgb(0x00,0xff,0xff), qRgb(0xff,0xff,0xff),
};

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(10);
    setFont(m_font);

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_InputMethodEnabled);
    setMouseTracking(false);
    setCursor(Qt::IBeamCursor);

    QFontMetrics fm(m_font);
    m_cellW    = fm.horizontalAdvance(QChar('M'));
    m_cellH    = fm.height();
    m_baseline = fm.ascent();
    if (m_cellW < 1) m_cellW = 1;
    if (m_cellH < 1) m_cellH = 1;

    initVterm(m_rows, m_cols);
}

TerminalWidget::~TerminalWidget() {
    shutdownVterm();
}

void TerminalWidget::initVterm(int rows, int cols) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_reset(m_screen, 1);
    vterm_screen_enable_altscreen(m_screen, 1);

    static const VTermScreenCallbacks cbs = {
        &TerminalWidget::cb_damage,
        &TerminalWidget::cb_moverect,
        &TerminalWidget::cb_movecursor,
        &TerminalWidget::cb_settermprop,
        &TerminalWidget::cb_bell,
        nullptr, // resize - kendi kontrolümüzde
        &TerminalWidget::cb_sb_pushline,
        &TerminalWidget::cb_sb_popline,
        nullptr, // sb_clear
        nullptr, // sb_pushline4 (kullanmıyoruz)
    };
    vterm_screen_set_callbacks(m_screen, &cbs, this);

    vterm_output_set_callback(m_vt, &TerminalWidget::cb_output, this);

    seedPalette();
}

void TerminalWidget::shutdownVterm() {
    if (m_vt) { vterm_free(m_vt); m_vt = nullptr; m_screen = nullptr; }
}

void TerminalWidget::seedPalette() {
    VTermState *st = vterm_obtain_state(m_vt);
    if (!st) return;

    VTermColor defFg, defBg;
    vterm_color_rgb(&defFg, m_fg.red(), m_fg.green(), m_fg.blue());
    defFg.type |= VTERM_COLOR_DEFAULT_FG;
    vterm_color_rgb(&defBg, m_bg.red(), m_bg.green(), m_bg.blue());
    defBg.type |= VTERM_COLOR_DEFAULT_BG;
    vterm_state_set_default_colors(st, &defFg, &defBg);

    for (int i = 0; i < 16; ++i) {
        VTermColor c;
        QColor q(kAnsiPalette[i]);
        vterm_color_rgb(&c, q.red(), q.green(), q.blue());
        vterm_state_set_palette_color(st, i, &c);
    }
}

void TerminalWidget::appendOutput(const QByteArray &data) {
    if (!m_vt) return;
    vterm_input_write(m_vt, data.constData(), (size_t)data.size());
    vterm_screen_flush_damage(m_screen);
    update();
}

// ------------------ libvterm callbacks ------------------

int TerminalWidget::cb_damage(VTermRect /*r*/, void *u) {
    static_cast<TerminalWidget*>(u)->update();
    return 1;
}

int TerminalWidget::cb_moverect(VTermRect /*d*/, VTermRect /*s*/, void *u) {
    static_cast<TerminalWidget*>(u)->update();
    return 1;
}

int TerminalWidget::cb_movecursor(VTermPos p, VTermPos /*old*/, int visible, void *u) {
    auto *self = static_cast<TerminalWidget*>(u);
    self->m_cursor = p;
    self->m_cursorVisible = visible != 0;
    self->update();
    return 1;
}

int TerminalWidget::cb_settermprop(VTermProp p, VTermValue *v, void *u) {
    auto *self = static_cast<TerminalWidget*>(u);
    if (p == VTERM_PROP_CURSORVISIBLE) {
        self->m_cursorVisible = v->boolean != 0;
    }
    // Altscreen/reverse/cursor — hepsinde tam refresh garantiye al
    self->update();
    return 1;
}

int TerminalWidget::cb_bell(void *u) {
    Q_UNUSED(u);
    QApplication::beep();
    return 1;
}

int TerminalWidget::cb_sb_pushline(int, const VTermScreenCell *, void *) {
    return 1; // scrollback şimdilik yok, satırı düşür
}

int TerminalWidget::cb_sb_popline(int, VTermScreenCell *, void *) {
    return 0;
}

void TerminalWidget::cb_output(const char *s, size_t len, void *u) {
    auto *self = static_cast<TerminalWidget*>(u);
    emit self->inputReady(QByteArray(s, (int)len));
}

// ------------------ Color resolution ------------------

QColor TerminalWidget::toQColor(const VTermColor *col, bool isFg) const {
    if (!col) return isFg ? m_fg : m_bg;
    if (col->type & VTERM_COLOR_DEFAULT_FG) return m_fg;
    if (col->type & VTERM_COLOR_DEFAULT_BG) return m_bg;

    VTermColor copy = *col;
    if ((copy.type & VTERM_COLOR_TYPE_MASK) == VTERM_COLOR_INDEXED) {
        if (m_screen) vterm_screen_convert_color_to_rgb(m_screen, &copy);
    }
    if ((copy.type & VTERM_COLOR_TYPE_MASK) == VTERM_COLOR_RGB) {
        return QColor(copy.rgb.red, copy.rgb.green, copy.rgb.blue);
    }
    int idx = copy.indexed.idx;
    if (idx >= 0 && idx < 16) return QColor(kAnsiPalette[idx]);
    return isFg ? m_fg : m_bg;
}

// ------------------ Paint ------------------

void TerminalWidget::paintEvent(QPaintEvent *e) {
    QPainter p(this);
    p.fillRect(rect(), m_bg);
    if (!m_screen) return;

    p.setFont(m_font);

    const QRect clip = e->rect();
    const int firstCol = qMax(0, clip.left() / m_cellW);
    const int lastCol  = qMin(m_cols - 1, clip.right() / m_cellW);
    const int firstRow = qMax(0, clip.top() / m_cellH);
    const int lastRow  = qMin(m_rows - 1, clip.bottom() / m_cellH);

    VTermPos selLo{-1,-1}, selHi{-1,-1};
    bool hasSel = (m_selStart.row >= 0 && m_selEnd.row >= 0
                   && (m_selStart.row != m_selEnd.row || m_selStart.col != m_selEnd.col));
    if (hasSel) normalizeSel(selLo, selHi);

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int col = firstCol; col <= lastCol; ++col) {
            VTermScreenCell cell;
            VTermPos pos{row, col};
            if (!vterm_screen_get_cell(m_screen, pos, &cell)) continue;
            if (cell.width == 0) continue;

            const int x = col * m_cellW;
            const int y = row * m_cellH;
            const int w = m_cellW * cell.width;

            bool reverse = cell.attrs.reverse != 0;
            QColor fg = toQColor(&cell.fg, true);
            QColor bg = toQColor(&cell.bg, false);
            if (reverse) std::swap(fg, bg);

            bool inSel = false;
            if (hasSel) {
                VTermPos cur{row, col};
                inSel = (vterm_pos_cmp(cur, selLo) >= 0 && vterm_pos_cmp(cur, selHi) < 0);
            }
            if (inSel) {
                bg = QColor(0x26, 0x4f, 0x78);
                fg = QColor(0xff, 0xff, 0xff);
            }

            p.fillRect(x, y, w, m_cellH, bg);

            if (cell.chars[0]) {
                QString s;
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i)
                    s += QString::fromUcs4(reinterpret_cast<const uint*>(&cell.chars[i]), 1);
                QFont f = m_font;
                f.setBold(cell.attrs.bold != 0);
                f.setItalic(cell.attrs.italic != 0);
                f.setUnderline(cell.attrs.underline != 0);
                f.setStrikeOut(cell.attrs.strike != 0);
                p.setFont(f);
                p.setPen(fg);
                p.drawText(x, y + m_baseline, s);
            }
        }
    }

    // Cursor
    if (m_cursorVisible
        && m_cursor.row >= firstRow && m_cursor.row <= lastRow
        && m_cursor.col >= firstCol && m_cursor.col <= lastCol)
    {
        QRect cur(m_cursor.col * m_cellW, m_cursor.row * m_cellH, m_cellW, m_cellH);
        if (m_hasFocus) {
            p.fillRect(cur, m_fg);
            VTermScreenCell cell;
            if (vterm_screen_get_cell(m_screen, m_cursor, &cell) && cell.chars[0]) {
                QString s;
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i)
                    s += QString::fromUcs4(reinterpret_cast<const uint*>(&cell.chars[i]), 1);
                p.setFont(m_font);
                p.setPen(m_bg);
                p.drawText(cur.x(), cur.y() + m_baseline, s);
            }
        } else {
            p.setPen(m_fg);
            p.drawRect(cur.adjusted(0, 0, -1, -1));
        }
    }
}

// ------------------ Resize ------------------

void TerminalWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    int cols = qMax(2, width()  / m_cellW);
    int rows = qMax(2, height() / m_cellH);
    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols; m_rows = rows;
    if (m_vt) vterm_set_size(m_vt, rows, cols);
    emit resized(cols, rows);
    update();
}

// ------------------ Key input ------------------

static VTermModifier qtModsToVterm(Qt::KeyboardModifiers m) {
    unsigned r = 0;
    if (m & Qt::ShiftModifier)   r |= VTERM_MOD_SHIFT;
    if (m & Qt::AltModifier)     r |= VTERM_MOD_ALT;
    if (m & Qt::ControlModifier) r |= VTERM_MOD_CTRL;
    return (VTermModifier)r;
}

void TerminalWidget::keyPressEvent(QKeyEvent *e) {
    if (!m_vt) return;

    // Yerel kısayollar: Ctrl+Shift+C kopya, Ctrl+Shift+V yapıştır
    if ((e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::ShiftModifier)) {
        if (e->key() == Qt::Key_C) {
            QApplication::clipboard()->setText(selectedText());
            return;
        }
        if (e->key() == Qt::Key_V) {
            QString t = QApplication::clipboard()->text();
            if (!t.isEmpty()) emit inputReady(t.toUtf8());
            return;
        }
    }

    VTermModifier mod = qtModsToVterm(e->modifiers());

    switch (e->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:     vterm_keyboard_key(m_vt, VTERM_KEY_ENTER,     mod); return;
        case Qt::Key_Backspace: vterm_keyboard_key(m_vt, VTERM_KEY_BACKSPACE, mod); return;
        case Qt::Key_Tab:       vterm_keyboard_key(m_vt, VTERM_KEY_TAB,       mod); return;
        case Qt::Key_Escape:    vterm_keyboard_key(m_vt, VTERM_KEY_ESCAPE,    mod); return;
        case Qt::Key_Up:        vterm_keyboard_key(m_vt, VTERM_KEY_UP,        mod); return;
        case Qt::Key_Down:      vterm_keyboard_key(m_vt, VTERM_KEY_DOWN,      mod); return;
        case Qt::Key_Left:      vterm_keyboard_key(m_vt, VTERM_KEY_LEFT,      mod); return;
        case Qt::Key_Right:     vterm_keyboard_key(m_vt, VTERM_KEY_RIGHT,     mod); return;
        case Qt::Key_Insert:    vterm_keyboard_key(m_vt, VTERM_KEY_INS,       mod); return;
        case Qt::Key_Delete:    vterm_keyboard_key(m_vt, VTERM_KEY_DEL,       mod); return;
        case Qt::Key_Home:      vterm_keyboard_key(m_vt, VTERM_KEY_HOME,      mod); return;
        case Qt::Key_End:       vterm_keyboard_key(m_vt, VTERM_KEY_END,       mod); return;
        case Qt::Key_PageUp:    vterm_keyboard_key(m_vt, VTERM_KEY_PAGEUP,    mod); return;
        case Qt::Key_PageDown:  vterm_keyboard_key(m_vt, VTERM_KEY_PAGEDOWN,  mod); return;
        default: break;
    }

    if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35) {
        int n = e->key() - Qt::Key_F1 + 1;
        vterm_keyboard_key(m_vt, (VTermKey)VTERM_KEY_FUNCTION(n), mod);
        return;
    }

    // Ctrl+harf: Qt'nin text()'i \x01-\x1A control byte verir, libvterm bunu CSI u'ya
    // çevirir (";5u" görünür). Bunun yerine lowercase letter ver, libvterm c&=0x1f yapar.
    if ((mod & VTERM_MOD_CTRL)
        && e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z)
    {
        uint32_t c = 'a' + (e->key() - Qt::Key_A);
        vterm_keyboard_unichar(m_vt, c, mod);
        return;
    }

    // Düz karakter
    const QString text = e->text();
    if (!text.isEmpty()) {
        for (uint c : text.toUcs4())
            vterm_keyboard_unichar(m_vt, c, mod);
        return;
    }

    QWidget::keyPressEvent(e);
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *e) {
    if (!m_vt) { e->ignore(); return; }
    const QString commit = e->commitString();
    if (!commit.isEmpty()) {
        for (uint c : commit.toUcs4())
            vterm_keyboard_unichar(m_vt, c, VTERM_MOD_NONE);
    }
    e->accept();
}

// ------------------ Mouse / selection ------------------

void TerminalWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    int col = qBound(0, e->pos().x() / m_cellW, m_cols - 1);
    int row = qBound(0, e->pos().y() / m_cellH, m_rows - 1);
    m_selStart = {row, col};
    m_selEnd   = {row, col};
    m_selecting = true;
    update();
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *e) {
    if (!m_selecting) return;
    int col = qBound(0, e->pos().x() / m_cellW, m_cols - 1);
    int row = qBound(0, e->pos().y() / m_cellH, m_rows - 1);
    m_selEnd = {row, col};
    update();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    m_selecting = false;
    int col = qBound(0, e->pos().x() / m_cellW, m_cols - 1);
    int row = qBound(0, e->pos().y() / m_cellH, m_rows - 1);
    m_selEnd = {row, col};
    update();
    QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *e) {
    QMenu menu(this);
    menu.addAction(tr("Copy (Ctrl+Shift+C)"), [this]{
        QApplication::clipboard()->setText(selectedText());
    });
    menu.addAction(tr("Paste (Ctrl+Shift+V)"), [this]{
        QString t = QApplication::clipboard()->text();
        if (!t.isEmpty()) emit inputReady(t.toUtf8());
    });
    menu.exec(e->globalPos());
}

void TerminalWidget::normalizeSel(VTermPos &lo, VTermPos &hi) const {
    lo = m_selStart; hi = m_selEnd;
    if (vterm_pos_cmp(lo, hi) > 0) std::swap(lo, hi);
    hi.col += 1;
    if (hi.col > m_cols) { hi.col = 0; hi.row += 1; }
}

QString TerminalWidget::selectedText() const {
    if (!m_screen) return {};
    if (m_selStart.row < 0 || m_selEnd.row < 0) return {};
    VTermPos lo, hi;
    normalizeSel(lo, hi);

    QString out;
    for (int row = lo.row; row <= hi.row && row < m_rows; ++row) {
        int c0 = (row == lo.row) ? lo.col : 0;
        int c1 = (row == hi.row) ? hi.col : m_cols;
        QString line;
        int lastNonBlank = -1;
        for (int col = c0; col < c1; ++col) {
            VTermScreenCell cell;
            VTermPos p{row, col};
            if (!vterm_screen_get_cell(m_screen, p, &cell)) continue;
            if (cell.width == 0) continue;
            if (cell.chars[0]) {
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i)
                    line += QString::fromUcs4(reinterpret_cast<const uint*>(&cell.chars[i]), 1);
                lastNonBlank = line.size();
            } else {
                line.append(' ');
            }
        }
        if (lastNonBlank >= 0) line.truncate(lastNonBlank);
        out.append(line);
        if (row != hi.row) out.append('\n');
    }
    return out;
}

void TerminalWidget::focusInEvent(QFocusEvent *e) {
    QWidget::focusInEvent(e);
    m_hasFocus = true;
    update();
}

void TerminalWidget::focusOutEvent(QFocusEvent *e) {
    QWidget::focusOutEvent(e);
    m_hasFocus = false;
    update();
}

void TerminalWidget::clampSel() {
    auto clamp = [this](VTermPos &p){
        if (p.row >= m_rows) p.row = m_rows - 1;
        if (p.col >= m_cols) p.col = m_cols - 1;
        if (p.row < 0) p.row = 0;
        if (p.col < 0) p.col = 0;
    };
    if (m_selStart.row >= 0) clamp(m_selStart);
    if (m_selEnd.row   >= 0) clamp(m_selEnd);
}
