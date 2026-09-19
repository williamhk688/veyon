Veyon 4.11.2 (modified) — Windows 64-bit installer
====================================================

This build includes:
  - Classroom studio UI (navy/teal cards, Configurator rail)
  - Persistent Windows screen lock
  - Persistent Windows demo lock (window + fullscreen)

Download this file from GitHub:

  installer/veyon_4_11_2_win64_modified_setup.exe

Direct download (this branch):

  https://github.com/williamhk688/veyon/raw/cursor/modern-ui-persistent-lock-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Or open the file on GitHub and click "Download raw file":

  https://github.com/williamhk688/veyon/blob/cursor/modern-ui-persistent-lock-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  215c645067bbd22e3ee0160d14069754c087479107f574fdd5f17d446d1e9c7a

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
The persistent lock and demo live in the student Veyon Service.
