Veyon 4.11.2 (modified) — Windows 64-bit installer
====================================================

Download this file from GitHub:

  installer/veyon_4_11_2_win64_modified_setup.exe

Direct download (this branch):

  https://github.com/williamhk688/veyon/raw/cursor/modern-ui-persistent-lock-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Or open the file on GitHub and click "Download raw file":

  https://github.com/williamhk688/veyon/blob/cursor/modern-ui-persistent-lock-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  31db34ee2ea82495dcde82757c8f219a082ad0b642f05f244314c7ec08a498f5

This is an unofficial MinGW cross-build of this fork (Qt 6.7.3),
not an official Veyon Solutions installer. Test on one student PC first.

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
