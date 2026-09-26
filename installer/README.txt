CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (cursor/classroom-web-filter-ecca) adds classroom web
filter on top of the WOL edition.

Master: one 「網絡管制」button with a dropdown
  - 封鎖不良網站
  - 只准課堂網站
  - 恢復網絡

While a filter is active, students show a badge in the top-right
(orange = blacklist, blue = classroom-only). Icons are a globe, not
the screen-lock padlock.

Whitelist now uses Chrome/Edge host-dot policy syntax so listed
classroom sites are allowed. Firefox, Brave, Chromium, Vivaldi, IE
and the Windows system proxy (PAC) are also filtered. Extra school
proxies can be added in Configurator; built-in proxies cannot be
deleted.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/classroom-web-filter-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Source: 43a7c1bb
Branch: cursor/classroom-web-filter-ecca

SHA-256:
  949aacf766290dbaab53333d0c95c0f967716851ddee4fa123b4988d4d987d8d
