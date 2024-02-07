We have a Telegram group!
If you want to share your knowledge join:
https://t.me/playintegrityfix

Device verdict should pass by default.
If not, try removing /data/adb/pif.json file.
DO NOT REMOVE pif.json in module's folder!

# v15.7.1

- Fix crash issue when JSON file have comments.
- Fix hooking in older Android versions.
- Fix CTS profile / Device verdict failures in few devices due bad spoofing code.
- Fix spoofing Provider issue.
- Added post-fs-data.sh script.
- Using latest (compileable) version of Dobby.
- Using RikkaW libcxx prefab.
- Update Gradle.