CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (feature/web-filter-session) adds timed WebFilterSession
on top of the classroom web filter / WOL edition.

Master: one 「網絡管制 (Web filter)」 button with a dropdown
  - 封鎖黑名單網站 (Block blacklist sites)
  - 只允許白名單網站 (Allow whitelist sites only)
  - 恢復網絡 (Restore web)
  - 查看剩餘時間 (View remaining time) — only while a session is active

Starting blacklist or whitelist asks for a lock duration
(10/20/30/45/60 minutes, custom duration, or custom end time).
The countdown window does not stay open; open it from 查看剩餘時間.
Restore web ends the active session and does not delete configured
lists. Unexpired sessions survive reboot until duration or the
configurable hard TTL (default 180 minutes). Ctrl+Alt+Shift+U still
uses the failsafe password and also clears a temporary web filter.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service.

Direct download:

  https://github.com/williamhk688/veyon/raw/feature/web-filter-session/installer/veyon_4_11_2_win64_modified_setup.exe

Source: d1202886
Branch: feature/web-filter-session

SHA-256:
  8e06b78d75336b1680468c3a85598bd776516d8e62447a33ddbabfa7f0971fdb
