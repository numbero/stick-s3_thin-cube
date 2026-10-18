# Bosch BMI270 SensorAPI

Source: https://github.com/boschsensortec/BMI270_SensorAPI/tree/41129fcfe39c583ee5462d79195741945d51c1fe

Only common API and Legacy sources are vendored. Original SHA-256 values:

```json
{
  "bmi2.c": "8d42ba3281b8448f674a4df616a7e176a259ad3c38c53b6d2c49bfdbdb0a82db",
  "bmi2.h": "a86c53c480974a0c56e24d9d4e736445c61ebc07addec6d3eaceddb8a2c97cd3",
  "bmi2_defs.h": "39c2cf19bfbf735cb0ed209efd975a731be50dff71b432b24a6a2625b2b14304",
  "bmi270_legacy.c": "6d53c8c3fa9204a34fbb8258f499a579d4d9a9326834fe90d0acd27455d3fe73",
  "bmi270_legacy.h": "4893be8413bfb793df1fe77c9d1109d7cc29e3bd59d684948dd1e707ea3df9c0"
}
```

Local patch: `set_config_page_primary()` copies strictly fewer than `idx` bytes, avoiding a one-byte stack overrun at the end of its 16-byte feature page. Covered by `scripts/test-legacy-imu.sh` with ASan/UBSan. No blob changes. Retain LICENSE in source and binary distribution materials.
