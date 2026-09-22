CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

WOL edition (lock overlay uses school crest; Ethernet wait 15s)
  Source: 0f9450af
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  192f7b78c46f94ffe193335145d88d6080d7301374fc3367542f3f4b317f7609

Changes vs the previous WOL flash-ok package:
  - Lock screen shows the Chuen Yuen College crest instead of the padlock
  - Power On waits up to 15 seconds for school DHCP (10.81.x.x)

Lock, failsafe, and input blocking are unchanged.
PR #2 is not modified.

Close Master, install as administrator, then open:

  C:\Program Files\Veyon\veyon-master.exe
