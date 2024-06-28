package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONObject;

import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public final class EntryPoint {
    public static final String TAG = "PIF";
    private static final Map<Field, String> map = new HashMap<>();

    static {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            Field keyStoreSpi = keyStore.getClass().getDeclaredField("keyStoreSpi");

            keyStoreSpi.setAccessible(true);

            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) keyStoreSpi.get(keyStore);

        } catch (Throwable t) {
            Log.e(TAG, "Couldn't get keyStoreSpi field!", t);
        }

        Provider provider = Security.getProvider("AndroidKeyStore");

        Provider customProvider = new CustomProvider(provider);

        Security.removeProvider("AndroidKeyStore");
        Security.insertProviderAt(customProvider, 1);
    }

    public static void init(String json) {

        if (TextUtils.isEmpty(json)) {
            Log.e(TAG, "JSON is empty!");
        } else {
            try {
                JSONObject jsonObject = new JSONObject(json);

                jsonObject.keys().forEachRemaining(s -> {
                    try {
                        if ("DEVICE_INITIAL_SDK_INT".equals(s)) return;

                        String value = jsonObject.getString(s);

                        if (TextUtils.isEmpty(value)) return;

                        Field field = getFieldByName(s);

                        if (field == null) return;

                        map.put(field, value);

                    } catch (Throwable t) {
                        Log.e(TAG, "Error parsing JSON", t);
                    }
                });
            } catch (Throwable t) {
                Log.e(TAG, "Error parsing JSON", t);
            }
        }

        Log.i(TAG, "Fields ready to spoof: " + map.size());

        spoofFields();
    }

    static void spoofFields() {
        map.forEach((field, s) -> {
            try {
                if (s.equals(field.get(null))) return;
                field.setAccessible(true);
                String oldValue = String.valueOf(field.get(null));
                field.set(null, s);
                Log.d(TAG, String.format("""
                        ---------------------------------------
                        [%s]
                        OLD: '%s'
                        NEW: '%s'
                        ---------------------------------------
                        """, field.getName(), oldValue, field.get(null)));
            } catch (Throwable t) {
                Log.e(TAG, "Error modifying field", t);
            }
        });
    }

    private static Field getFieldByName(String name) {
        Field field;
        try {
            field = Build.class.getDeclaredField(name);
        } catch (NoSuchFieldException e) {
            try {
                field = Build.VERSION.class.getDeclaredField(name);
            } catch (NoSuchFieldException ex) {
                return null;
            }
        }
        field.setAccessible(true);
        return field;
    }
}
