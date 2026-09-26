CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

This branch (cursor/classroom-web-filter-ecca) adds classroom web
filter on top of the WOL edition. The installer binary below is still
the parent WOL package until a Windows cross-compile of webfilter.dll
is published here.

WOL edition (no wol-adapter.log; white lock overlay; Ethernet wait 15s)
  Source: 541861d5
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download (WOL parent, no web-filter plugin yet):

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  ade3d580f791f89816bcb4f089779d1b5f1b9dfce5e7a9dcea56ad9f43d5e920

PR #2 is not modified. The WOL adapter files are not changed on this
branch.
