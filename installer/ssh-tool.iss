; SSH Tool — Inno Setup script
; Build: ISCC.exe installer\ssh-tool.iss
; Output: installer\out\ssh-tool-setup-<version>.exe

#define MyAppName       "SSH Tool"
#define MyAppVersion    "0.1.0"
#define MyAppPublisher  "Okan Aytimur"
#define MyAppURL        "https://github.com/okanaytimur/ssh-tool"
#define MyAppExeName    "ssh-tool.exe"
#define SourceDir       "..\build\Release"

[Setup]
; AppId değişmemeli — uninstaller bunu kullanır
AppId={{F7782F50-7528-4EAE-BEBC-497A2EE69562}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\SSH Tool
DefaultGroupName=SSH Tool
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=out
OutputBaseFilename=ssh-tool-setup-{#MyAppVersion}
SetupIconFile=..\resources\ssh-tool.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Windows 7 ve üstü
MinVersion=6.1
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Beraberinde gelen VC++ Redist'i installer içinde yer kapla
DiskSpanning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Ana exe
Source: "{#SourceDir}\{#MyAppExeName}";       DestDir: "{app}"; Flags: ignoreversion
; Qt5 runtime DLL'leri
Source: "{#SourceDir}\Qt5Core.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt5Gui.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt5Widgets.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt5Svg.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\libEGL.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\libGLESv2.dll";         DestDir: "{app}"; Flags: ignoreversion
; libssh + crypto
Source: "{#SourceDir}\ssh.dll";               DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\libcrypto-3-x64.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\libssl-3-x64.dll";      DestDir: "{app}"; Flags: ignoreversion
; Qt plugin'leri
Source: "{#SourceDir}\imageformats\*";  DestDir: "{app}\imageformats";  Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\platforms\*";     DestDir: "{app}\platforms";     Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\styles\*";        DestDir: "{app}\styles";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\iconengines\*";   DestDir: "{app}\iconengines";   Flags: ignoreversion recursesubdirs createallsubdirs
; Visual C++ Redistributable — kurulum sırasında sessizce çalışıp silinir
Source: "{#SourceDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}";                   Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";             Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; VC++ Runtime'ı sessiz kur — zaten yüklüyse no-op döner (~3 saniye)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Microsoft Visual C++ Runtime kontrol ediliyor..."; Flags: waituntilterminated
; Kurulum sonrası başlatma seçeneği (varsayılan: kapalı)
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; \
    Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
; Kullanıcı verisini SİLMİYORUZ (servers.json + backups)
; Sadece kurulumla gelen dosya/plugin klasörlerini Inno otomatik temizler.
