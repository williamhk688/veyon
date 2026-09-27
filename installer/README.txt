CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (feature/web-filter-session) adds timed WebFilterSession
on top of the classroom web filter / WOL edition.

Master: one 「Web filter (網絡管制)」 button with a dropdown
  - Block blacklist sites (封鎖黑名單網站)
  - Allow whitelist sites only (只允許白名單網站)
  - Restore web (恢復網絡)
  - View remaining time (查看剩餘時間) — only while a session is active

Starting blacklist or whitelist asks for a lock duration
(10/20/30/45/60 minutes, custom duration, or custom end time).
The countdown window does not stay open; open it from View remaining time.
Restore web ends the active session and does not delete configured
lists. Unexpired sessions survive reboot until duration or the
configurable hard TTL (default 180 minutes).

Emergency Unlock & Recover (緊急解鎖與自救) is the last toolbar item
(after Screenshot). It opens a dropdown with:
  - Change Failsafe Password (修改解鎖密碼)
  - Teacher Self-Rescue (教師自救手冊)

Ctrl+Alt+Shift+U still uses the failsafe password and also clears a
temporary web filter. The same session is not reapplied after that
hotkey; a new teacher session still applies. The handbook documents
this hotkey path for lock, demo, and blacklist/whitelist.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service.

Direct download:

  https://github.com/williamhk688/veyon/raw/feature/web-filter-session/installer/veyon_4_11_2_win64_modified_setup.exe

Source: ac348336
Branch: feature/web-filter-session

SHA-256:
  f7f44b42c43ea5d45b12550d57730fda40107b34d38404ce4ce9f9780bccd868
