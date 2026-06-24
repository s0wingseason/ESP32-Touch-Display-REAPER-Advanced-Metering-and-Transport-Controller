; ============================================================================
; MeterBridge v2.0 RC1 — Inno Setup Installer Script
; FalconEYE Software Dev — June 2026
;
; Builds a fully self-contained installer that:
;   1. Detects Python 3 (offers download link if missing)
;   2. Installs relay, REAPER scripts, firmware binaries, docs
;   3. Installs Python deps (pyserial) silently
;   4. Auto-detects and copies REAPER scripts into REAPER's Scripts folder
;   5. Raises a first-run config helper to capture ESP32 IP
;   6. Creates Start Menu + optional Desktop shortcuts
;   7. Includes a standalone esptool for one-click firmware flashing
; ============================================================================

#define MyAppName      "MeterBridge"
#define MyAppVersion   "2.0-RC1"
#define MyAppPublisher "FalconEYE Software Dev"
#define MyAppURL       "https://github.com/s0wingseason/ESP32-Touch-Display-REAPER-Advanced-Metering-and-Transport-Controller"
#define MyAppExe       "run_relay.bat"

[Setup]
AppId={{B8E2C4D5-F1A3-4B67-8C90-D2E4F6A8B0C2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=no
LicenseFile=..\RELEASE_NOTES.md
InfoBeforeFile=installer_welcome.rtf
OutputDir=..\release\installer
OutputBaseFilename=MeterBridge_v{#MyAppVersion}_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=130
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UninstallDisplayName={#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\run_relay.bat
CreateUninstallRegKey=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableDirPage=no
DisableProgramGroupPage=no
; Store installed version in registry for upgrade detection
VersionInfoVersion=2.0.0.1
VersionInfoProductName={#MyAppName}
VersionInfoDescription={#MyAppName} Installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";    Description: "Full Installation - Relay + REAPER Scripts + Firmware + Docs (recommended)"
Name: "relay";   Description: "Relay Server + REAPER Scripts only (no firmware binaries)"
Name: "custom";  Description: "Custom — let me choose"; Flags: iscustom

[Components]
Name: "relay";    Description: "MeterBridge Relay Server (Python + config) — REQUIRED"; Types: full relay custom; Flags: fixed
Name: "reaper";   Description: "REAPER Bridge Scripts  (Lua script + JSFX spectrum plugin)"; Types: full relay custom
Name: "firmware"; Description: "ESP32 Firmware Binaries  (for one-click flashing)"; Types: full custom
Name: "esptool";  Description: "ESPTool Flash Utility  (standalone — no PlatformIO needed)"; Types: full custom
Name: "docs";     Description: "MeterBridge Documentation  (offline HTML manual)"; Types: full custom

[Tasks]
Name: "desktopshortcut";  Description: "Create a Desktop shortcut for the relay"; GroupDescription: "Shortcuts:"
Name: "installreaper";    Description: "Auto-install REAPER bridge scripts now"; GroupDescription: "REAPER Integration:"; Components: reaper
Name: "startrelay";       Description: "Launch MeterBridge Relay after installation"; GroupDescription: "First Run:"

[Files]
; ── Relay core ──────────────────────────────────────────────────────
Source: "..\meterbridge_relay.py";             DestDir: "{app}";          Components: relay; Flags: ignoreversion
Source: "..\relay_config.txt";                  DestDir: "{app}";          Components: relay; Flags: onlyifdoesntexist
Source: "..\run_relay.bat";                     DestDir: "{app}";          Components: relay; Flags: ignoreversion
Source: "..\flash_firmware.bat";               DestDir: "{app}";          Components: relay; Flags: ignoreversion
Source: "..\install_reaper_bridge.bat";         DestDir: "{app}";          Components: relay; Flags: ignoreversion
Source: "..\RELEASE_NOTES.md";                  DestDir: "{app}";          Components: relay; Flags: ignoreversion
Source: "..\requirements.txt";                  DestDir: "{app}";          Components: relay; Flags: ignoreversion

; ── REAPER bridge ────────────────────────────────────────────────────
Source: "..\scripts\meterbridge_reaper_bridge.lua"; DestDir: "{app}\reaper"; Components: reaper; Flags: ignoreversion
Source: "..\scripts\meterbridge_spectrum.jsfx";     DestDir: "{app}\reaper"; Components: reaper; Flags: ignoreversion

; ── ESP32 Firmware ────────────────────────────────────────────────────
Source: "..\esp32-firmware\.pio\build\crowpanel_7inch\firmware.bin";   DestDir: "{app}\firmware"; DestName: "meterbridge_firmware_rc1.bin"; Components: firmware; Flags: ignoreversion
Source: "..\esp32-firmware\.pio\build\crowpanel_7inch\bootloader.bin"; DestDir: "{app}\firmware"; Components: firmware; Flags: ignoreversion
Source: "..\esp32-firmware\.pio\build\crowpanel_7inch\partitions.bin"; DestDir: "{app}\firmware"; Components: firmware; Flags: ignoreversion

; ESPTool standalone (optional — included if tools\esptool\ was populated by build_installer.bat)
Source: "..\tools\esptool\*"; DestDir: "{app}\tools\esptool"; Components: esptool; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist

; Documentation (optional — included if docs\ is present)
Source: "..\docs\*"; DestDir: "{app}\docs"; Components: docs; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist

[Dirs]
Name: "{app}\logs"; Flags: uninsneveruninstall

[Icons]
; Start menu
Name: "{group}\Run MeterBridge Relay";          Filename: "{app}\run_relay.bat";              Comment: "Start the MeterBridge relay server"
Name: "{group}\Flash ESP32 Firmware";            Filename: "{app}\flash_firmware.bat";          Comment: "Flash new firmware to your ESP32 over USB"
Name: "{group}\Install REAPER Bridge Scripts";   Filename: "{app}\install_reaper_bridge.bat";   Comment: "Copy bridge scripts into REAPER"
Name: "{group}\Open Quick-Start Guide";          Filename: "{app}\docs\index.html";             Comment: "Open the MeterBridge documentation"
Name: "{group}\Release Notes";                   Filename: "{app}\RELEASE_NOTES.md";            Comment: "Read the full changelog"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
; Desktop
Name: "{commondesktop}\MeterBridge Relay";       Filename: "{app}\run_relay.bat"; Tasks: desktopshortcut; Comment: "Launch MeterBridge relay"

[Run]
; Install Python deps silently (pyserial)
Filename: "python"; Parameters: "-m pip install --quiet pyserial"; Description: "Install Python dependencies (pyserial)"; WorkingDir: "{app}"; Flags: runhidden waituntilterminated; StatusMsg: "Installing Python dependencies..."; Components: relay

; Copy REAPER scripts if task was ticked
Filename: "{app}\install_reaper_bridge.bat"; Description: "Install REAPER bridge scripts"; WorkingDir: "{app}"; Flags: shellexec waituntilterminated; Tasks: installreaper; Components: reaper

; Open quick-start guide
Filename: "{app}\docs\index.html"; Description: "Open MeterBridge Quick-Start Guide in browser"; Flags: postinstall shellexec skipifsilent nowait unchecked; Components: docs

; Launch relay now
Filename: "{app}\run_relay.bat"; Description: "Launch MeterBridge Relay now"; WorkingDir: "{app}"; Flags: postinstall shellexec skipifsilent nowait unchecked; Tasks: startrelay

[UninstallDelete]
Type: filesandordirs; Name: "{app}\__pycache__"
Type: filesandordirs; Name: "{app}\logs\*.log"


; ============================================================================
; [Code] — custom wizard logic
;   • Pre-install: Python 3 detection, offer download link
;   • Custom page: ESP32 IP address capture → writes relay_config.txt
;   • Custom page: COM port selector for firmware flashing
;   • Post-install: validation summary
; ============================================================================
[Code]

var
  ESP32Page:  TInputQueryWizardPage;
  PythonVer:  String;
  REAPERPath: String;
  ESP32IP:    String;


{ ─── Python detection helper ──────────────────────────────────────── }
function DetectPython(var version: String): Boolean;
var
  SubKeys: TArrayOfString;
  pyPath:  String;
begin
  Result  := False;
  version := '';

  { Check registry — HKLM then HKCU for any PythonCore subkey }
  if RegGetSubkeyNames(HKLM, 'SOFTWARE\Python\PythonCore', SubKeys) then
    if GetArrayLength(SubKeys) > 0 then begin
      Result := True; version := 'Python ' + SubKeys[0]; Exit;
    end;
  if RegGetSubkeyNames(HKCU, 'SOFTWARE\Python\PythonCore', SubKeys) then
    if GetArrayLength(SubKeys) > 0 then begin
      Result := True; version := 'Python ' + SubKeys[0]; Exit;
    end;

  { Fallback: check PATH via FindOnPath }
  pyPath := '';
  if RegQueryStringValue(HKLM, 'SOFTWARE\Python\PythonCore\3\InstallPath', 'ExecutablePath', pyPath) then
    if pyPath <> '' then begin Result := True; version := 'Python 3 (registry)'; Exit; end;
  if RegQueryStringValue(HKCU, 'SOFTWARE\Python\PythonCore\3\InstallPath', 'ExecutablePath', pyPath) then
    if pyPath <> '' then begin Result := True; version := 'Python 3 (registry)'; Exit; end;
end;


{ ─── REAPER detection helper ──────────────────────────────────────── }
function DetectREAPER(var path: String): Boolean;
var
  regPath: String;
begin
  Result := False;
  path   := '';
  regPath := '';
  { HKCU first (most installs), then HKLM }
  if RegQueryStringValue(HKCU, 'SOFTWARE\REAPER', 'InstallDir', regPath) then begin
    path := regPath; Result := True; Exit;
  end;
  if RegQueryStringValue(HKLM, 'SOFTWARE\REAPER', 'InstallDir', regPath) then begin
    path := regPath; Result := True; Exit;
  end;
  { Fallback: common locations }
  if DirExists(ExpandConstant('{pf}\REAPER')) then begin
    path := ExpandConstant('{pf}\REAPER'); Result := True;
  end;
end;


{ ─── Wizard initialisation ────────────────────────────────────────── }
procedure InitializeWizard;
begin
  { Page 1: ESP32 IP address }
  ESP32Page := CreateInputQueryPage(wpSelectTasks,
    'Configure ESP32 Connection',
    'Enter your ESP32''s WiFi IP address.',
    'MeterBridge communicates with your CrowPanel ESP32 over WiFi. ' +
    'The IP address is displayed in the status bar when the device boots.' + #13#10 + #13#10 +
    'Leave as 0.0.0.0 to enable auto-discovery (device must be on same LAN).');
  ESP32Page.Add('ESP32 IP Address:', False);
  ESP32Page.Values[0] := '0.0.0.0';
end;


{ ─── Pre-install gate: Python check ────────────────────────────────── }
function InitializeSetup(): Boolean;
var
  PyFound:   Boolean;
  DownloadQ: Integer;
begin
  Result  := True;
  PyFound := DetectPython(PythonVer);

  if not PyFound then begin
    DownloadQ := MsgBox(
      'Python 3 was not detected on this computer.' + #13#10 +
      'MeterBridge requires Python 3.8 or newer to run the relay server.' + #13#10 + #13#10 +
      'Please install Python before continuing:' + #13#10 +
      '  https://python.org/downloads/' + #13#10 + #13#10 +
      'IMPORTANT: Check "Add Python to PATH" during installation.' + #13#10 + #13#10 +
      'Click YES to continue installing anyway (you can install Python later).' + #13#10 +
      'Click NO to cancel and install Python first.',
      mbConfirmation, MB_YESNO);
    if DownloadQ = IDNO then
      Result := False;
  end;

  DetectREAPER(REAPERPath);
end;


{ ─── Write relay_config.txt with user's ESP32 IP ───────────────────── }
procedure WriteConfig;
var
  ConfigPath: String;
  ip:         String;
  content:    String;
begin
  ConfigPath := ExpandConstant('{app}\relay_config.txt');
  ip := Trim(ESP32Page.Values[0]);
  if ip = '' then ip := '0.0.0.0';

  content := 'esp32_ip=' + ip + #13#10 +
              'update_ms=50' + #13#10 +
              '# ESP32 IP address. Set to 0.0.0.0 for auto-discovery.' + #13#10 +
              '# The relay will broadcast on the LAN to find meterbridge.local if 0.0.0.0.' + #13#10;

  SaveStringToFile(ConfigPath, content, False);
end;


{ Post-install steps: write config and show summary }
procedure CurStepChanged(CurStep: TSetupStep);
var
  pyStr: String;
  reaperStr: String;
  summaryMsg: String;
begin
  if CurStep = ssPostInstall then begin
    WriteConfig;
    if DetectPython(pyStr) then pyStr := 'Found: ' + pyStr
    else pyStr := 'NOT FOUND - install from python.org';
    if DetectREAPER(reaperStr) then reaperStr := 'Found: ' + reaperStr
    else reaperStr := 'Not detected';
    summaryMsg :=
      'MeterBridge installed successfully!' + #13#10 + #13#10 +
      'Python:  ' + pyStr + #13#10 +
      'REAPER:  ' + reaperStr + #13#10 +
      'ESP32:   ' + Trim(ESP32Page.Values[0]) + #13#10 + #13#10 +
      'Next Steps:' + #13#10 +
      '1. Open REAPER, load meterbridge_reaper_bridge.lua' + #13#10 +
      '2. Power on CrowPanel ESP32 (same WiFi as your PC)' + #13#10 +
      '3. Run MeterBridge Relay from Start Menu or Desktop' + #13#10 +
      '4. Press Play in REAPER - enjoy your meters!';
    MsgBox(summaryMsg, mbInformation, MB_OK);
  end;
end;


{ ─── Validation on ESP32 IP field ──────────────────────────────────── }
function NextButtonClick(CurPageID: Integer): Boolean;
var
  ip: String;
begin
  Result := True;

  if CurPageID = ESP32Page.ID then begin
    ip := Trim(ESP32Page.Values[0]);

    { Accept 0.0.0.0 (auto-discover) or blank }
    if (ip = '') or (ip = '0.0.0.0') then begin
      ESP32Page.Values[0] := '0.0.0.0';
      Exit;
    end;

    { Very basic IP format check (4 octets separated by dots) }
    if (Length(ip) < 7) or (Pos('.', ip) = 0) then begin
      MsgBox('Please enter a valid IP address (e.g. 192.168.1.180)' + #13#10 +
             'or leave as 0.0.0.0 for automatic discovery.',
             mbError, MB_OK);
      Result := False;
    end;
  end;
end;
