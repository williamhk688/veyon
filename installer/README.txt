CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (cursor/classroom-web-filter-ecca) adds classroom web
filter on top of the WOL edition. Teacher Master has three mutually
exclusive buttons: blacklist, whitelist, and restore. Veyon ports
stay reachable. Common web proxies and DoH hosts are hardcoded-blocked.
Blacklist can persist until the teacher restores. Whitelist is
session-only and is dropped on reboot or Veyon Service restart.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service. The new Master buttons only work when
the student Service also has plugins/webfilter.dll.

WOL edition features are unchanged (no wol-adapter.log; white lock
overlay; Ethernet wait 15s). PR #2 is not modified. The WOL adapter
files are not changed on this branch.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/classroom-web-filter-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Source: b2769b39
Branch: cursor/classroom-web-filter-ecca

SHA-256:
  a6c03e6a20f11787067d874613268e62ad1c6b8f958788a5e7640054ee01d9e0
