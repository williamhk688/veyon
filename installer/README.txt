CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (cursor/classroom-web-filter-ecca) adds classroom web
filter on top of the WOL edition.

Master: one 「網絡管制 (Web filter)」 button with a dropdown
  - 封鎖黑名單網站 (Block blacklist sites)
  - 只允許白名單網站 (Allow whitelist sites only)
  - 恢復網絡 (Restore web)

The computer-tile icon follows the current mode: red block for
blacklist, green globe for whitelist. Restore removes the badge.
Configurator sidebar title is 「Web filter (網絡管制)」.
Whitelist allow rules include http/https scheme forms so listed
classroom sites can open.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/classroom-web-filter-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Source: 220ae180
Branch: cursor/classroom-web-filter-ecca

SHA-256:
  1e64e38b3840519c946581140392c6965e90653ffd2857a24a9eb45287dc75ea
