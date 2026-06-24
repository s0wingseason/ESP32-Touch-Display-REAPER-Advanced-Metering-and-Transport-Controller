; StudioBeacon Installer Script
; Inno Setup 6.x

#define MyAppName "StudioBeacon"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Falcon Studios"
#define MyAppURL "https://falconstudios.com"
#define MyAppExeName "StudioBeacon.exe"
#define SourcePath "C:\StudioBeacon"

[Setup]
AppId={{A8E4B2C3-D5F6-47A8-B9C1-E2F3A4B5C6D7}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=auto
AllowNoIcons=no
PrivilegesRequired=admin
OutputDir={#SourcePath}\installer\output
OutputBaseFilename=StudioBeacon_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
ChangesAssociations=no
CreateAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startmenu"; Description: "Create Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce
Name: "reaperintegration"; Description: "Enable REAPER Integration (Automatic)"; GroupDescription: "DAW Integration:"; Flags: checkedonce

[Dirs]
; Ensure REAPER Scripts directory exists
Name: "{userappdata}\REAPER"; Flags: uninsneveruninstall
Name: "{userappdata}\REAPER\Scripts"; Flags: uninsneveruninstall

[Files]
; Main application - copy contents of win-unpacked (not the folder itself)
Source: "{#SourcePath}\dist\win-unpacked\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; REAPER Auto-Start Script - copy to REAPER scripts folder
Source: "{#SourcePath}\scripts\__startup_studiobeacon.lua"; DestDir: "{userappdata}\REAPER\Scripts"; Tasks: reaperintegration; Flags: ignoreversion

; Also keep a copy in the app folder for manual install reference
Source: "{#SourcePath}\scripts\__startup_studiobeacon.lua"; DestDir: "{app}\scripts"; Flags: ignoreversion

; Manual REAPER script as reference
Source: "{#SourcePath}\scripts\StudioBeacon_REAPER_Bridge.lua"; DestDir: "{app}\scripts"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
; Start Menu entries
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "Studio Status Display"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop shortcut (optional)
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Comment: "Studio Status Display"

; User Programs (Start Menu search)
Name: "{userprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startmenu; Comment: "Studio Status Display"

[Registry]
; Register with Windows App Paths for Start Menu search
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{#MyAppExeName}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{#MyAppExeName}"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey

; Application registration
Root: HKLM; Subkey: "SOFTWARE\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\{#MyAppPublisher}\{#MyAppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekey

[Run]
; Launch after install
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Post-install complete
  end;
end;
