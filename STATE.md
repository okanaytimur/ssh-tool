# STATE.md — ssh-tool

Bu dosya, projenin **şu anki durumunu** ve **nasıl devam edileceğini** anlatır. Yeni bir
Claude session'ında ilk önce `README.md` ve bu dosya okunmalı. Son güncelleme: 2026-05-22.

---

## 1. Proje özeti

- **Ne?** Qt 5.15.2 + libssh + libvterm üzerine kurulu, **Windows 7+** hedefli, hafif
  bir PuTTY benzeri SSH + SFTP istemcisi.
- **Ana UI**: sol panelde SFTP dosya tablosu + path bar; sağ panelde gerçek terminal
  (libvterm grid render); üst menüde Connections, About.
- **İskeletin orijinal hâli** `README.md` Bağımlılıklar / Build bölümünde anlatılıyor.

---

## 2. Geliştirme ortamı (Windows, bu makine)

Bu yollar gerçek dosya sisteminde mevcut; yeniden bulmaya gerek yok:

| Şey | Konum |
|---|---|
| Qt 5.15.2 (dynamic) | `C:\Qt\5.15.2\msvc2019_64` |
| Qt kits (mingw, msvc) | aynı klasör altında, `mingw81_64` da var ama kullanmıyoruz |
| Qt Creator | `C:\Qt\Tools\Qt Creator 20.0.0-beta1` (cmake getirmedi) |
| MinGW 8.1 (kurulu, kullanılmıyor) | `C:\Qt\Tools\mingw810_64` |
| MSVC 2022 Community + v143 toolset | `C:\Program Files\Microsoft Visual Studio\2022\Community` (`cl.exe` v14.44) |
| Windows 11 SDK | `C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0` (rc.exe, mt.exe) |
| Portable CMake 3.29.6 | `C:\dev\cmake\bin\cmake.exe` |
| vcpkg | `C:\dev\vcpkg\` |
| libssh + openssl + zlib (vcpkg) | `C:\dev\vcpkg\installed\x64-windows\` |
| Git | `C:\Program Files\Git\cmd\git.exe` |

**Önemli:** Sistemde `cmake` PATH'te yok; her zaman tam yol kullan
(`C:\dev\cmake\bin\cmake.exe`).

---

## 3. Build / çalıştır

### İlk-defa configure (build/ silmişsek)
```
C:\dev\cmake\bin\cmake.exe -S C:\Users\okan\Desktop\ssh-tool ^
                           -B C:\Users\okan\Desktop\ssh-tool\build ^
                           -G "Visual Studio 17 2022" -A x64 ^
                           -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64 ^
                           -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake ^
                           -DVCPKG_TARGET_TRIPLET=x64-windows
```

### Sonraki build'ler
```
C:\dev\cmake\bin\cmake.exe --build C:\Users\okan\Desktop\ssh-tool\build --config Release
```

Çıktı: `build\Release\ssh-tool.exe` (~228 KB). Yanındaki Qt DLL'leri, libssh DLL'leri ve
ikon zaten kopyalı (önceki `windeployqt` çalıştırması sonucu); CMake/vcpkg değişiklikleri
gerektirmediği sürece tekrar deploy gerekmez.

### Statik / shipping build için
- `vcpkg install libssh:x64-windows-static mbedtls:x64-windows-static`
- Configure'a `-DSSHTOOL_STATIC_RUNTIME=ON -DVCPKG_TARGET_TRIPLET=x64-windows-static` ekle
- Qt'yi statik derle (README'deki gibi); o yapılana kadar deployment dinamik Qt + DLL'ler

---

## 4. Mimari (src/ ve third_party/)

| Dosya | Görev |
|---|---|
| `src/main.cpp` | Entry; org/app adı (`local` / `ssh-tool`); `setWindowIcon(:/ssh-tool.png)` |
| `src/mainwindow.{h,cpp}` | Ana pencere. Path bar (üst), file table, terminal, status bar. Connections menüsü, About. **Çift yönlü drag-drop**. **Edit context menu** + editor save flow. eventFilter'da hem incoming drop hem outgoing drag mouse-tracking. |
| `src/sshsession.{h,cpp}` | libssh sarmalayıcısı. `connectToServer`, `openShell`, `writeShell`, `resizePty`, `sftpListDir`, `sftpDownload`, `sftpUpload`. Upload/download artık **bool** dönüyor + her chunk'ta `processEvents(ExcludeUserInputEvents)`. |
| `src/sftpmodel.{h,cpp}` | `QAbstractTableModel`; `entryAt(int) → QVariant` (header `QVariantMap` değil). |
| `src/terminalwidget.{h,cpp}` | **libvterm tabanlı grid terminal**. `QPlainTextEdit` türevi DEĞİL — düz `QWidget`, kendi `QPainter` ile cell render. VS Code Dark+ palette, 16 ANSI + indexed→RGB. Selection, copy/paste (Ctrl+Shift+C/V), IME. Ctrl+harf için lowercase letter trick (CSI u'ya düşmesin diye). |
| `src/connectionsdialog.{h,cpp}` | Sunucu listesi editörü. **Auto-commit** her field değişiminde m_store'a yazar + `saveRequested` sinyali; MainWindow disk'e flush eder. Save butonu redundant ama hâlâ duruyor. |
| `src/serverconfig.{h,cpp}` | JSON load/save (`servers.json`). Parolalar **plaintext** — DPAPI TODO. |
| `src/editorwindow.{h,cpp}` | Standalone kod editörü. QPlainTextEdit + syntax highlight, Ctrl+S = save (saveRequested sinyali), Ctrl+Shift+S = save & close. Modified durumda title'da `*`. Close prompt unsaved. |
| `src/syntaxhighlighter.{h,cpp}` | `QSyntaxHighlighter`; regex tabanlı per-language rules. Diller: python, js/ts, yaml, json, ini/conf/toml, bash, xml/html, c/cpp/cs/java/go/rust, sql, dockerfile, makefile/cmake. Multi-line block (`/* */`, `<!-- -->`) için block-state. |
| `third_party/libvterm/` | neovim'in libvterm fork'u vendor'lanmış. Kendi `CMakeLists.txt` ile static `vterm.lib` derler. |
| `resources/app.qrc` | `:/ssh-tool.png` |
| `resources/ssh-tool.{ico,rc,manifest}` | Windows resource: ikon + Win7 manifest |
| `CMakeLists.txt` | Tek hedef `ssh-tool`. `find_package(Qt5)`, `find_package(libssh CONFIG)`, `add_subdirectory(third_party/libvterm)`. `SSHTOOL_STATIC_RUNTIME` option (default OFF = dinamik CRT). |

---

## 5. Veri konumları (runtime)

| Şey | Yol |
|---|---|
| Sunucu listesi (plaintext!) | `%APPDATA%\local\ssh-tool\servers.json` |
| Editör backup'ları | `%APPDATA%\local\ssh-tool\backups\<dosya>,<sunucu>,<yyyy-MM-dd_HH-mm-ss>` |
| Editör için indirilen geçici dosya | `%TEMP%\ssh-tool-edit\<dosya>` |
| Drag-out geçici dosya (server → desktop) | `%TEMP%\ssh-tool-drag\<dosya>` |

Aç → `Start` → `Run` → `%APPDATA%\local\ssh-tool` (Explorer açar).

---

## 6. Yapılan iş — kronolojik özet

Birkaç session boyunca eklenen şeyler, sıra ile:

1. **Toolchain bootstrap.** cmake + vcpkg kurulumu. Windows 11 SDK eksikti (VS'ye eklendi).
   `libssh:x64-windows` derlendi (openssl 3.6, ~11 dk).
2. **İlk derleme + kaynak düzeltmeleri.**
   - `CMakeLists.txt`: statik CRT opt-in (`SSHTOOL_STATIC_RUNTIME`), default dinamik
   - `sftpmodel.h`: `entryAt` dönüş tipi `QVariant`'a düzeltildi
   - `terminalwidget.cpp`: yorum sonundaki `\` line-continuation kaldırıldı (C4010)
   - `sshsession.cpp`: `<fcntl.h>` eklendi, `S_I*` literal `0644` ile değiştirildi
   - `windeployqt` ile Qt DLL'leri staj'landı, libssh/ssl/crypto DLL'leri manuel kopyalandı
3. **Path bar** sol panelin üstünde. Up + Refresh butonu. Enter ile gezinme.
4. **Connections persistence UX fix.** Her field değişiminde otomatik m_store'a commit;
   row değişiminde önce eski satırı commit; dialog close'da yine commit. Eskiden Save'e
   basılmadan kapanırsa veri kayboluyordu.
5. **libvterm terminal.** `QPlainTextEdit`'in CSI yutan custom parser'ı atıldı; vendor
   libvterm + grid-based QPainter renderer. Renkler, selection, IME, fonksiyon tuşları,
   Ctrl+harf doğru control byte üretiyor.
6. **Icon.** PowerShell + System.Drawing ile multi-size .ico (16/32/48/256), `.rc` resource,
   `setWindowIcon`. Dark blue + `>_` glyph. Düzeltilmesi/değiştirilmesi: dosyaları yenile.
7. **Drag-drop (her iki yön).**
   - **Drag-in**: Explorer → SFTP panel uploads. `eventFilter` ile DragEnter/Drop.
   - **Drag-out**: SFTP panel → Explorer/Desktop. Mouse press/move tespiti, `%TEMP%`'a
     senkron indirme (UI input bloklu, mouse-state korunur), `QDrag::exec`.
   - Source check ile kendi drag'imiz tekrar upload'a düşmüyor.
8. **Status bar progress.** `m_transferPrefix` + human-readable bytes (`1.2 MB / 50 MB (24%)`).
   `processEvents(ExcludeUserInputEvents)` SshSession upload/download loop'larına
   eklendi → transfer sırasında UI repaint geçer.
9. **About.** Help → About'ta "Okan Aytimur".
10. **Kod editörü.** Sağ tık → "Düzenle…" → indir + backup + `EditorWindow` aç. Save =
    upload + status. Backup formatı: `<dosya>,<sunucu>,<yyyy-MM-dd_HH-mm-ss>`. Açma anında
    bir kez backup; her save'de değil.

---

## 7. Bilinen sınırlar / açık TODO'lar

Öncelik sırasına göre (kabaca):

- **Parolalar plaintext** `servers.json`'da. Çözüm: Windows DPAPI ile şifrele
  (`CryptProtectData` / `CryptUnprotectData`). README'de zaten TODO.
- **SSH session UI thread'inde.** Büyük SFTP transferinde processEvents çevirimi varsa
  da pencere yine de bloklu. Doğru fix: `SshSession`'ı `QThread` worker'a taşı, sinyallerle
  konuş. README TODO #4.
- **Scrollback yok** (terminal). libvterm `sb_pushline` callback'inde gelen satırı
  düşürüyorum. Bir `QVector<QVector<VTermScreenCell>>` ring buffer + sağda QScrollBar
  ile çözülür.
- **Nano alt-screen restore'unda zaman zaman leftover.** `cb_settermprop` her prop'ta
  `update()` çağırıyor ama bazen primary screen'e nano content sızıyor görünüyor. libvterm'in
  `?1049l` davranışı + bizim repaint mantığı arasında bir şey. Reproduce edilebilirse
  debug log gerek.
- **Klasör drag-drop yok** (her iki yön). Recursive walk + SFTP'de `sftp_mkdir` + dosyaların
  tek tek transferi gerek.
- **Editor: Find/Replace yok** (Ctrl+F). Basit dialog ile eklenebilir.
- **Editor: Line numbers yok.** QPlainTextEdit'in viewport'una sol margin ekleyerek
  custom widget çizilebilir.
- **Editor: Binary detection yok.** Açıştan önce ilk 512 byte'ı tara, `\0` varsa "Binary,
  yine de aç?" sor.
- **Ctrl+rakam / Ctrl+sembol terminal'de CSI u'ya düşer.** libvterm keyboard.c'nin
  default davranışı. Ctrl+harf intercept ettim (lowercase + `vterm_keyboard_unichar` ile
  doğru control byte üretiyor). Ctrl+5, Ctrl+@, Ctrl+\ vs. lazımsa ek intercept gerek.
- **Logging yok.** qDebug çıktısı sadece DebugView ile yakalanabilir. İhtiyaç olursa
  `qInstallMessageHandler` ile `%APPDATA%\local\ssh-tool\ssh-tool.log`'a yazılabilir.
- **known_hosts bypass.** Şu an her sunucuyu TOFU olmadan kabul ediyoruz. README TODO #1.

---

## 8. Yeni session'da nasıl devam edilir

Yeni bir Claude session'ında, ilk turn'de şu sırayla:

1. **Bu dosyayı (`STATE.md`) oku.** Mimari ve durumu anlamak için.
2. **`README.md` oku.** Orijinal niyetler ve build talimatları için.
3. **İlgili `src/` dosyalarını oku.** İstediğin değişikliğin dokunduğu yerleri.
4. **Toolchain hazır** — yukarıdaki yollar gerçek. Yeniden kurmaya gerek yok.
5. **Build doğrulaması** için: `C:\dev\cmake\bin\cmake.exe --build C:\Users\okan\Desktop\ssh-tool\build --config Release` çalıştır → temiz build dönerse kod state'i geçerli.
6. **Çalışan exe'yi kapatmadan tekrar build etme** (`Stop-Process -Name ssh-tool -Force`
   önce).

### Anlamlı dosya:satır referansları (sık dokunulan)

- Terminal keyboard handling: `src/terminalwidget.cpp:280` civarı (`keyPressEvent`)
- Renderer: `src/terminalwidget.cpp:180` (paintEvent)
- Status bar progress format: `src/mainwindow.cpp` constructor içindeki sftpProgress lambda
- Edit flow: `src/mainwindow.cpp` `onEditCurrentFile()`
- Editor save: `src/editorwindow.cpp` `onSave()` ve `onUploadCompleted/Failed`
- Drag-in: `src/mainwindow.cpp` `eventFilter`'da `QEvent::DragEnter/Drop`
- Drag-out: `src/mainwindow.cpp` `startDragOut()` ve eventFilter'da `MouseMove`
- Syntax rule eklemek: `src/syntaxhighlighter.cpp` `buildRules()`
- libvterm screen callbacks: `src/terminalwidget.cpp` `cb_*` fonksiyonları

### Bilinmesi gereken kararlar

- **Qt 5.15 hedeflendi** (Win7 desteği için). Qt 6'ya çıkmak Win7'yi düşürür.
- **libvterm vcpkg'de yok**, vendor'landı (`third_party/libvterm`). FetchContent
  yerine vendor seçimi kasıtlı: offline build + reproducibility.
- **MSVC + dinamik Qt** prototip yolu; statik Qt + statik CRT shipping için bekliyor.
- **Slot dönüş tipleri**: `sftpDownload`/`sftpUpload` artık `bool`. Qt slot'ları bool
  dönebilir, signal/slot bağlantısı dönüş değerini yok sayar.

---

İyi çalışmalar.
