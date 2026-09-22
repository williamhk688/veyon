CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

WOL edition (no wol-adapter.log; white lock overlay; Ethernet wait 15s)
  Source: 541861d5
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  ade3d580f791f89816bcb4f089779d1b5f1b9dfce5e7a9dcea56ad9f43d5e920

Power On does not write C:\ProgramData\Veyon\wol-adapter.log.
You can delete that file if it is still on the teacher PC.
Lock screen: white background, school crest centered.
Power On waits up to 15 seconds for school DHCP.

PR #2 is not modified. Close Master, install as administrator, then restart
Veyon Service and open:

  C:\Program Files\Veyon\veyon-master.exe

Student PCs need this build to see the white lock overlay.
