CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This build includes:
  - Display name CYC Veyon; Chuen Yuen College crest as the app/installer icon
  - Classroom studio UI (navy/teal cards, Configurator rail)
  - Persistent Windows screen lock and demo lock (window + fullscreen)
  - Failsafe hotkey Ctrl+Alt+Shift+U (default password ccc24205050CYC)
  - Faster Lock/Demo start (Interception first; HID/powercfg in the background)
  - Master: change failsafe password (security questions, then new password twice;
    no old password, so mixed student PCs can be rewritten together);
    teacher self-rescue handbook (warning + questions: LCC / DAT / WAN)
    shows both the default and latest unlock passwords

Download this file from GitHub (this branch):

  installer/veyon_4_11_2_win64_modified_setup.exe

Direct download:

  https://github.com/williamhk688/veyon/raw/feature-password-backdoor/installer/veyon_4_11_2_win64_modified_setup.exe

Or open the file on GitHub and click "Download raw file":

  https://github.com/williamhk688/veyon/blob/feature-password-backdoor/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  c123c2ecb9354c075928f79026cb2b82ce2944c9fc57fdc167d04d3db4c9de04

This is an unofficial MinGW cross-build of this fork (Qt 6.7.3),
not an official Veyon Solutions installer. Test on one student PC first.

Install
-------
1. Uninstall any existing Veyon first (optional but recommended).
2. Right-click the setup EXE and Run as administrator.
3. Teacher PC: keep "CYC Veyon Master" selected.
   Student PC: uncheck "CYC Veyon Master", or run:
     veyon_4_11_2_win64_modified_setup.exe /S /NoMaster
4. Keep "Interception driver" selected on student PCs so lock can
   block Ctrl+Alt+Del. The installer now runs the driver setup; reboot
   the student PC once after install. Teacher PCs can uncheck it.
5. After install, open CYC Veyon Configurator, set authentication, and
   add student computers in CYC Veyon.

Install path stays `C:\Program Files\Veyon`. The Windows service name
stays VeyonService. Only the names you see on shortcuts and window titles
changed.

Both teacher and student machines MUST run this modified build.
The persistent lock, demo lock, and failsafe hotkey live on the student PC.
The self-rescue handbook is in CYC Veyon (teacher PC only).

Teacher Safe Mode recovery guide
--------------------------------
The same steps are also in Master → 教師自救手冊 (after the security questions).

Direct download:

  https://github.com/williamhk688/veyon/raw/feature-password-backdoor/installer/teacher-lock-recovery.docx

Same file, Chinese filename:

  installer/老師專用-鎖定自救說明.docx
