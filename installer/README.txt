Veyon 4.11.2 (modified) — Windows 64-bit installer
====================================================

This build includes:
  - Classroom studio UI (navy/teal cards, Configurator rail)
  - Persistent Windows screen lock and demo lock (window + fullscreen)
  - Failsafe hotkey Ctrl+Alt+Shift+U (default password ccc24205050CYC)
  - Master: change failsafe password (current + new password twice);
    teacher self-rescue handbook (warning + questions: LCC / DAT / WAN)

Download this file from GitHub (this branch):

  installer/veyon_4_11_2_win64_modified_setup.exe

Direct download:

  https://github.com/williamhk688/veyon/raw/feature-password-backdoor/installer/veyon_4_11_2_win64_modified_setup.exe

Or open the file on GitHub and click "Download raw file":

  https://github.com/williamhk688/veyon/blob/feature-password-backdoor/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  dab926ce3a0224fe08cdbec08ced150daaacaf76a395c60e72fb504dc45e27bc

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
   block Ctrl+Alt+Del. The installer now runs the driver setup; reboot
   the student PC once after install. Teacher PCs can uncheck it.
5. After install, open Veyon Configurator, set authentication, and
   add student computers in Master.

Both teacher and student machines MUST run this modified build.
The persistent lock, demo lock, and failsafe hotkey live on the student PC.
The self-rescue handbook is in Veyon Master (teacher PC only).

Teacher Safe Mode recovery guide
--------------------------------
The same steps are also in Master → 教師自救手冊 (after the security questions).

Direct download:

  https://github.com/williamhk688/veyon/raw/feature-password-backdoor/installer/teacher-lock-recovery.docx

Same file, Chinese filename:

  installer/老師專用-鎖定自救說明.docx
