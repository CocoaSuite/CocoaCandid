; CocoaCandid installer (Inno Setup 6). Built by .github/workflows/build.yml:
;   ISCC /DAppVersion=0.1.0 /DSourceDir=<installed app> /DOutDir=<release folder> CocoaCandid.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6B0C5E3A-4C2D-4E7B-9C1A-CA0D1DCA0D1D}
AppName=CocoaCandid
AppVersion={#AppVersion}
AppVerName=CocoaCandid {#AppVersion}
AppPublisher=CocoaSuite
AppPublisherURL=https://github.com/CocoaSuite/CocoaCandid
AppSupportURL=https://github.com/CocoaSuite/CocoaCandid/issues
AppUpdatesURL=https://github.com/CocoaSuite/CocoaCandid/releases/latest
DefaultDirName={autopf}\CocoaCandid
DefaultGroupName=CocoaCandid
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\LICENSE.txt
OutputDir={#OutDir}
OutputBaseFilename=CocoaCandid-{#AppVersion}-windows-x64-setup
SetupIconFile={#SourceDir}\..\candid\resources\windows\candid.ico
UninstallDisplayIcon={app}\bin\64bit\CocoaCandid.exe
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
PrivilegesRequired=admin
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "virtualcam"; Description: "Install the virtual camera (lets video calls use CocoaCandid as a camera)"; GroupDescription: "Extras:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\CocoaCandid"; Filename: "{app}\bin\64bit\CocoaCandid.exe"; WorkingDir: "{app}\bin\64bit"
Name: "{autodesktop}\CocoaCandid"; Filename: "{app}\bin\64bit\CocoaCandid.exe"; WorkingDir: "{app}\bin\64bit"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; Flags: waituntilterminated
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"""; Tasks: virtualcam; Flags: runhidden waituntilterminated
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/s ""{app}\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"""; Tasks: virtualcam; Flags: runhidden waituntilterminated skipifdoesntexist
Filename: "{app}\bin\64bit\CocoaCandid.exe"; WorkingDir: "{app}\bin\64bit"; Description: "{cm:LaunchProgram,CocoaCandid}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"""; Flags: runhidden; RunOnceId: "UnregVcam64"
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/u /s ""{app}\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"""; Flags: runhidden skipifdoesntexist; RunOnceId: "UnregVcam32"
