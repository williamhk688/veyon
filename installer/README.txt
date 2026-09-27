CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (cursor/classroom-web-filter-ecca) adds classroom web
filter on top of the WOL edition.

Master: one 「網絡管制」button with a dropdown
  - 封鎖不良網站
  - 只准課堂網站
  - 恢復網絡

While a filter is active, students show a badge in the top-right
(orange = blacklist, blue = classroom-only). The badge runs in
veyon-worker so veyon-server stays alive.

This build also re-applies the Windows Firewall exception for
veyon-server.exe on every Service start (needed after reinstall,
especially on Wi-Fi / Public profiles) and does not follow a
leftover system PAC/proxy.

Install this package on both the teacher PC and every student PC,
then restart Veyon Service. Teacher and student must be on the same
LAN. Ethernet unplugged is fine if both use the same Wi-Fi subnet.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/classroom-web-filter-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

Source: fa83ed5e
Branch: cursor/classroom-web-filter-ecca

SHA-256:
  bdb5b944f0fc7fa2d5011ea7ca1f897dee4f097e5d0dd128498b75ef522a11d0
