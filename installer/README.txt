CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

Test package: WOL Ethernet enable, verify SetIfEntry actually worked
  Source: 5586726b
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  0c1efdf0ba52f98dcc3f4173837e1123f34c57673fe27575847e9e2ddc6b686b

The previous log showed the helper ran and SetIfEntry claimed success on
interface 20, then restored it 5 seconds later. Windows can report
SetIfEntry success without changing the adapter. This build checks the
real admin status and falls back to netsh if the NIC stays disabled.

Close Master, install as administrator, then open:

  C:\Program Files\Veyon\veyon-master.exe

Watch Intel Ethernet, click Power on. If it still does not flash, send
the new:

  C:\ProgramData\Veyon\wol-adapter.log
