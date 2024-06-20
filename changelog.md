Telegram channel:
https://t.me/playintegrityfix

Device verdict should pass by default.
If not, try removing /data/adb/pif.json file.

Donations:
https://www.paypal.com/paypalme/chiteroman

# v16.3

Google fixed the bug, no more Strong pass with SW keybox 😢

- Improve C++ and Java code
- Downgrade first_api_level to 24, so all devices (should) be able to pass Device
- Included keybox.xml parsing! You can create /data/adb/keybox.xml to define your own keybox (Strong passing with my private one :D)

By default, inside module folder, it exits pif.json and keybox.xml, do NOT delete these files

keybox.xml included in the module is SW one

Keybox "hack" does NOT work on broken TEE devices, like OnePlus
