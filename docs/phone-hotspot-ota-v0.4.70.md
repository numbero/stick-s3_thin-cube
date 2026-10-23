# English-only OTA boot notices v0.4.70

2026-10-24. Replaces the new device notices from v0.4.69 with English text using the existing Font0 bitmap font. The amber theme, steady sprite rendering, health checks, rollback policy, and fresh A/B dismissal gate are unchanged.

The pending trial shows `SELF CHECK`, `DO NOT / RESTART`, `KEEP POWER ON`, and elapsed seconds. A full reference progress bar still means waiting: after 60 seconds the text remains `CHECKING HEALTH...` until the backend actually confirms the image. Only the successful IDF health confirmation displays the green `UPDATE / COMPLETE`, `SAFE TO RESTART`, and `PRESS A / B / TO CONTINUE` screen. Ordinary already-confirmed boots do not show a false completion notice.

The 15-character Chinese bitmap subset and its unused header have been removed. The phone page now points to the exact English device text `UPDATE COMPLETE`; it continues to describe the upload ACK as written, pending boot health, without an automatic success timer. Existing phone page language is unchanged.

## Validation

- Release and Base firmware builds passed. Release image: 1,847,872 bytes; SHA-256 `0035a05691491b0ee5d31a984af8f1451927e288dd41b140250a4c0d33ffe2df`.
- The actual render function and original M5GFX Font0 were rendered offline for checking, waiting beyond 60 seconds, and completion. All primitives fit the 135×240 viewport; all notice strings are ASCII. Preview images and the reproducible host script are in `artifacts/phone-ota-v0.4.70/`. These are offline images, not device captures.
- The final v0.4.70 image passed the phone page identity, image, digest, upload ACK, pending result, and cancellation checks. The unchanged backend and button policy passed the v0.4.69 host/sanitizer checks documented in [v0.4.69](phone-hotspot-ota-v0.4.69.md).
- USB installed with device MAC, both original image hashes, and both VALID selector records verified first. OTA/NVS backups were saved. Only app0 and its seq=7 selector were written; both writes passed esptool hash verification. The already-confirmed app1/v0.4.69 fallback and settings were retained.
- No serial connection was opened during the trial. The installed Mac app first observed v0.4.70 with startup checking and upgrade actions disabled, then observed the same version after >60 seconds with checking gone and actions enabled (`bootVerified=true`). Evidence: `usb-flash/mac-boot-check-observations.json`. The device completion page is retained for the user; visual acceptance remains pending. No post-trial ROM metadata read or additional reset was performed.

## Firmware

`artifacts/phone-ota-v0.4.70/thincube-v0.4.70.bin`. Manifest, build log, phone-page test log, source snapshot, preview script, and USB evidence are stored alongside it. Only inactive app0 (`0x10000`) and its selector (`0xe000 / seq=7 / NEW`) are selected for the update; the confirmed app1/v0.4.69 fallback and device settings are retained.
