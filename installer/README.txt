Veyon 4.11.2 (modified): modern dark UI + persistent Windows screen lock

Use this installer:
  veyon_4_11_2_win64_modified_setup.exe

It is a 64-bit Windows NSIS setup of this fork with:
- Dark/flat Qt theme for Master and Configurator
- Screen lock that survives reboot until Master unlocks (HKLM)

Install
-------
1. Uninstall any existing Veyon first (optional but recommended).
2. Right-click the setup EXE and Run as administrator.
3. Teacher PC: keep "Veyon Master" selected.
   Student PC: uncheck "Veyon Master", or run:
     veyon_4_11_2_win64_modified_setup.exe /S /NoMaster
4. Keep "Interception driver" selected on student PCs so lock can
   block Ctrl+Alt+Del.
5. After install, open Veyon Configurator, set authentication, and
   add student computers in Master.

Both teacher and student machines MUST run this modified build.
The persistent lock lives in the student Veyon Service.

This is an unofficial MinGW cross-build (Qt 6.7.3). It is not the
official Veyon Solutions installer. Test on one student PC first.
