# Remove Play Services from the Magisk Denylist when set to enforcing
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
fi

# Check if safetynet-fix is installed
if [ -d "/data/adb/modules/safetynet-fix" ]; then
    touch "/data/adb/modules/safetynet-fix/remove"
fi