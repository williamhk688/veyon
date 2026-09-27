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
the screen-lock padlock. The badge runs in veyon-worker so
veyon-server (QGuiApplication) stays alive and Master can connect.

Whitelist now uses Chrome/Edge host-dot policy syntax so listed
classroom sites are allowed. Firefox, Brave, Chromium, Vivaldi and IE
are also filtered. Extra school proxies can be added in Configurator;
built-in proxies cannot be deleted. This build does not install a
Windows system PAC / AutoConfigURL. Leftover PAC from older builds is
cleared when Veyon Service starts.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service. If an older web-filter DLL is still
loaded, reboot once after install.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/classroom-web-filter-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Source: 8c9caf5e
Branch: cursor/classroom-web-filter-ecca

SHA-256:
  6fed2f38cbf021eeeb89f3f5d243f2fe5996bf913c1307dcc745b4b02c6dd063
