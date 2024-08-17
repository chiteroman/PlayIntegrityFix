Telegram channel:
https://t.me/playintegrityfix

Device verdict should pass by default.
If not, try removing /data/adb/pif.json file.

Donations:
https://www.paypal.com/paypalme/chiteroman

NOTE: If your ROM is signed with test-keys, modify "spoofSignature" value in
/data/adb/modules/playintegrityfix/pif.json and set to "true".
Remember to kill com.google.android.gms.unstable process after this!

# v17.2

- Fix custom ROMs support
- Spoof android.os.Build fields in Zygisk instead Java
- Compatibility with TrickyStore module
- If hook is disabled, unload Zygisk lib
- Add granular advanced spoofing options (thanks @osm0sis)
- Other minor improvements
