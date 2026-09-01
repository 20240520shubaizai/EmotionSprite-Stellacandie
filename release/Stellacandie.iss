#ifndef MyAppVersion
  #define MyAppVersion "0.9.0-rc.1"
#endif
#ifndef StageDir
  #error StageDir must point to the staged release directory
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef CompressionMode
  #define CompressionMode "lzma2/ultra64"
#endif
#ifndef SolidMode
  #define SolidMode "yes"
#endif

#define MyAppName "Stellacandie"
#define MyAppPublisher "20240520shubaizai"
#define MyAppExeName "EmotionSprite.exe"
#ifndef MyAppId
  #define MyAppId "{{4BB2A7B0-1D4D-4C53-AFFB-5943D5B80874}"
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/20240520shubaizai/EmotionSprite-Stellacandie
AppSupportURL=https://github.com/20240520shubaizai/EmotionSprite-Stellacandie/issues
AppUpdatesURL=https://github.com/20240520shubaizai/EmotionSprite-Stellacandie/releases
DefaultDirName={localappdata}\Programs\Stellacandie
DefaultGroupName=Stellacandie
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=Stellacandie-{#MyAppVersion}-Windows-x64-Setup
Compression={#CompressionMode}
SolidCompression={#SolidMode}
WizardStyle=modern
SetupIconFile={#SourcePath}\..\assets\app\Stellacandie.ico
CloseApplications=force
RestartApplications=no
SetupLogging=yes
UninstallDisplayName=Stellacandie
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion=0.9.0.1
VersionInfoProductName=Stellacandie
VersionInfoProductVersion=0.9.0.1
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Stellacandie AI companion desktop pet
VersionInfoCopyright=Copyright (c) 2026 20240520shubaizai

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式："; Flags: unchecked

[Files]
Source: "{#StageDir}\payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\agent-runtime-backup.zip"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\Repair-Agent.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\release-manifest.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\USER_GUIDE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Stellacandie"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\卸载 Stellacandie"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Stellacandie"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 Stellacandie"; Flags: nowait postinstall skipifsilent

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  if Exec(ExpandConstant('{cmd}'), '/C tasklist /FI "IMAGENAME eq EmotionSprite.exe" /NH | find /I "EmotionSprite.exe" >NUL', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0) then
    Result := 'Stellacandie 正在运行。请先从系统托盘退出程序，再继续安装或升级。';
  if (Result = '') and Exec(ExpandConstant('{cmd}'), '/C tasklist /FI "IMAGENAME eq agent-core.exe" /NH | find /I "agent-core.exe" >NUL', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0) then
    Result := 'Stellacandie Agent 服务正在运行。请先退出桌宠，再继续安装或升级。';
end;
