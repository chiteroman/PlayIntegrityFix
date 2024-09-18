Telegram channel:
https://t.me/playintegrityfix

Device verdict should pass by default.
If not, try removing /data/adb/pif.json file.

Donations:
https://www.paypal.com/paypalme/chiteroman

If your ROM is signed with test-keys, modify "spoofSignature" value in
/data/adb/modules/playintegrityfix/pif.json and set to "true".
Remember to kill com.google.android.gms.unstable process after this!

If you are using TrickyStore and you have a valid keybox, but Strong
isn't passing, maybe you should change the ROM.
Stock ROMs gives the best results.

# v17.6

- Remove keybox logic
- Update AGP, NDK & CMake
- Change build from MinSizeRel to Release (better performance)
- Enable LTO
- Refactor code
