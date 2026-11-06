; Inno Setup Script for EzSnmp Data Logger
; Cross-platform SNMP Telemetry Engine

#ifndef AppVersion
#define AppVersion "2026.11.6.1"
#endif

#ifndef SourceExe
#define SourceExe "..\dist\EzSnmpDataLogger.exe"
#endif

[Setup]
AppId={{E5D4B692-7E5A-483A-9E86-443E838F5C1B}
AppName=EzSnmp Data Logger
AppVersion={#AppVersion}
AppVerName=EzSnmp Data Logger {#AppVersion}
AppPublisher=carlkidcrypto
AppPublisherURL=https://carlkidcrypto.github.io/ezsnmp_data_logger/
AppSupportURL=https://github.com/carlkidcrypto/ezsnmp_data_logger/issues
AppUpdatesURL=https://github.com/carlkidcrypto/ezsnmp_data_logger/releases
DefaultDirName={autopf}\EzSnmpDataLogger
DefaultGroupName=EzSnmp Data Logger
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=EzSnmpDataLogger-Windows-x64-Setup
SetupIconFile=..\ezsnmp_data_logger\gui\static\app_icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "envPath"; Description: "Add application to PATH environment variable (allows running 'ezsnmp-data-logger' or 'EzSnmpDataLogger' from command prompt)"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "EzSnmpDataLogger.exe"; Flags: ignoreversion
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "ezsnmp-data-logger.exe"; Flags: ignoreversion
Source: "..\ezsnmp_data_logger\gui\static\app_icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.rst"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\EzSnmp Data Logger"; Filename: "{app}\EzSnmpDataLogger.exe"; IconFilename: "{app}\app_icon.ico"
Name: "{autodesktop}\EzSnmp Data Logger"; Filename: "{app}\EzSnmpDataLogger.exe"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: envPath; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
Filename: "{app}\EzSnmpDataLogger.exe"; Description: "{cm:LaunchProgram,EzSnmp Data Logger}"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;
