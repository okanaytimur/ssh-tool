# SSH Tool

Windows 7+ üzerinde çalışan, hafif, framework gerektirmeyen PuTTY benzeri SSH+SFTP istemcisi.

## Yapı
- Sol panel: bağlı sunucudaki SFTP dosya listesi (çift tık = klasör değiştir, sağ tık = download/upload)
- Sağ panel: terminal (shell)
- File → Connections menüsünden sunucu listesi yönetimi
- Sunucular `%APPDATA%/local/ssh-tool/servers.json` içinde saklanır (örnek: `servers.example.json`)

## Bağımlılıklar
- Qt 5.15.2 LTS (statik build önerilir — Win7 desteği için son sürüm)
- libssh 0.10.x veya 0.11.x (libssh2 değil — SFTP API'si daha temiz)
- mbedTLS veya OpenSSL 1.1.1 (libssh kripto backend)
- zlib

## Windows build (özet)

### Qt statik derleme (bir kez)
```
git clone https://code.qt.io/qt/qt5.git -b 5.15.2
configure -static -release -opensource -confirm-license -platform win32-msvc ^
  -prefix C:\Qt\5.15.2-static -nomake examples -nomake tests ^
  -skip qt3d -skip qtwebengine -skip qtmultimedia ^
  -no-icu -no-opengl-es2
nmake
nmake install
```

### libssh
vcpkg ile en kolay:
```
vcpkg install libssh:x64-windows-static mbedtls:x64-windows-static
```

### Uygulamayı derle
```
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2-static ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release
```

## Dağıtım
Çıktı klasöründe:
```
ssh-tool.exe
libssh.dll              (LGPL — DLL olarak kalsın)
libcrypto-1_1.dll       (veya mbedcrypto.dll)
libssl-1_1.dll
zlib1.dll
```

Vcredist gerekmez (statik CRT). .NET gerekmez.

## TODO (sonraki sürümler)
- known_hosts doğrulaması ve fingerprint UI'ı
- libvterm entegrasyonu (256 renk, doğru cursor, alternatif ekran)
- Parolaların DPAPI / CryptProtectData ile şifrelenmesi
- SSH session'ı ayrı thread'e taşımak (büyük transferlerde UI bloklamasın)
- Ctrl+Tab ile birden fazla bağlantı sekmesi
- Reconnect / keep-alive
- Drag-drop ile upload
