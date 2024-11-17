Telegram channel:
https://t.me/playintegrityfix

Device verdict should pass by default.
If not, try removing /data/adb/pif.json file.

Donations:
https://www.paypal.com/paypalme/chiteroman

If you are using TrickyStore and you have a valid keybox, but Strong
isn't passing, maybe you should change the ROM.
Stock ROMs gives the best results.

# v18.0

- Module won't delete ro.build.selinux prop, if you can't pass attestation, you can try deleting it
  manually.
  More info here: https://github.com/chiteroman/PlayIntegrityFix/pull/470

- Update fingerprint to latest oriole (Pixel 6) beta rom

- Remove auto conflict apps (cause bootloops, just remove them manually)

- Improve Zygisk check

- Upgrade Gradle, AGP and CMake

- Enable LTO for Zygisk lib
