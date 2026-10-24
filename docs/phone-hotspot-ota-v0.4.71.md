# OTA wrong-file diagnosis and downgrade prevention v0.4.71

2026-10-24. The user reported an intended v0.4.70 phone-hotspot update reaching 100%, rebooting, and entering the normal display without the new boot notices.

## Confirmed cause

Readback of the actual device found app0 containing the exact published v0.4.70 image, but the newer OTA selector (`seq=8 / VALID`) chose app1. App1's descriptor was v0.4.67 and its full image hash matched the archived v0.4.67 release (SHA-256 `840219491004f7e0c1d6c271d29fd292940a0770f105d7fe646df89250f04a9a`). The installed Mac app independently reported v0.4.67. The new boot-notice screen was introduced in v0.4.69, so the selected older image could not display it.

This establishes that the observed restart selected an older image, not that v0.4.70's pending-state screen was skipped. Flash contents cannot establish which iPhone Files item was chosen or why. Earlier packages remained available during manual testing, and the upload flow accepted older release images without an explicit downgrade barrier. Previous USB trial verification did not cover this wrong-file path.

Local evidence is preserved under `artifacts/phone-ota-v0.4.71/investigation/`: both selector sectors, decoded records, descriptors, and full-image comparisons. Inspection did not alter flash/settings and reset the device back into its original application afterward.

## Fix

- The phone page compares the release's numeric version core with the current device version and rejects older releases. It shows a prominent current → target version line. The device independently rejects the same downgrade at `/ota/begin` with `firmware_downgrade`, before confirmation, token publication, or erasing the spare slot. USB recovery remains available for deliberate older-image recovery. Named custom development builds retain their existing handling.
- Official `thincube-<version>.bin` filenames must match the descriptor inside the file; a renamed older package is rejected as `filename_version_mismatch`. File content remains the authority for version and device identity.
- Reinstalling the same release remains supported and still requires physical confirmation, a fresh trial boot, and normal health validation. No timer-driven completion or early success state was added.
- Successful image validation logs actual current/target versions and the selected slot for future investigations.
- The transfer folder `artifacts/phone-ota-v0.4.71/for-iphone/` contains only `thincube-v0.4.71.bin`. The existing collection of build logs and older packages is outside that folder.

## Build and validation

Release and Base builds passed. Release image is 1,849,968 bytes, SHA-256 `ef3cbbf692a61a4f48a1aea632670adc75786e0ad213582c2656fe807fd62ed3`. Full local checks passed, including 49 actual OTA-source ASan/UBSan scenarios, release-version comparisons, independently enforced server rejection with zero erase/commit, phone downgrade/renamed-file tests, and same-version reinstallation admission. Swift: 142 tests, one existing skip, zero failures. Final-image phone checks passed.

USB installation and the real iPhone hotspot retest were verified separately below. The hotspot evidence uses the actual phone upload and resulting slot selection, not a manually armed USB trial.

- USB: wrote inactive app0 and seq=9 selector after verified preflight and OTA/NVS backup, preserving the selected app1/v0.4.67 fallback. Both writes passed esptool hash verification. After more than 60 seconds, the installed Mac app observed v0.4.71 without a startup-check indicator and with upgrade actions enabled (`bootVerified=true`); no serial connection interrupted the trial. This confirms the USB bootstrap only.
- iPhone hotspot: the user confirmed the old package is blocked and the new file shows v0.4.71 → v0.4.71. They then initiated the actual phone upload, confirmed on the device, reported the amber `SELF CHECK / DO NOT RESTART` screen immediately after the automatic restart, and subsequently confirmed the green `UPDATE COMPLETE` screen. The Mac independently observed v0.4.71 with `bootVerified=false` and actions disabled during the trial, followed by `bootVerified=true` and actions restored. No serial connection was opened during this trial.
- After the confirmed completion and more than 90 seconds from the restart report, readback found app1 `seq=10 / VALID` with the entire image matching the v0.4.71 package. The app0 `seq=9 / VALID` fallback selector was byte-for-byte unchanged from its expected confirmed state; its complete v0.4.71 image also matched. A final normal restart logged v0.4.71, `ota state : valid`, and `run app1`. Evidence: `usb-flash/hotspot-postflash-states.json`, `hotspot-postflash-boot.log`, and `hotspot-live-health.json`. Ordinary confirmed boots correctly return directly to the normal display.
