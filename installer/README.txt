CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (feature/web-filter-session) adds timed WebFilterSession
on top of the classroom web filter / WOL edition.

Master: one 「網絡管制 (Web filter)」 button with a dropdown
  - 封鎖黑名單網站 (Block blacklist sites)
  - 只允許白名單網站 (Allow whitelist sites only)
  - 恢復網絡 (Restore web)

Starting blacklist or whitelist asks for a lock duration
(10/20/30/45/60 minutes, custom duration, or custom end time).
Restore web ends the active session and does not delete configured
lists. Unexpired sessions survive reboot until duration or the
configurable hard TTL (default 180 minutes). Ctrl+Alt+Shift+U still
uses the failsafe password and also clears a temporary web filter.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service.

Direct download:

  https://github.com/williamhk688/veyon/raw/feature/web-filter-session/installer/veyon_4_11_2_win64_modified_setup.exe

Source: 496c944b
Branch: feature/web-filter-session

SHA-256:
  6b685d336e3620742a13983c5add934bbc62a27b58ec917932de83b7a6db3609
