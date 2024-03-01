package es.chiteroman.playintegrityfix;

import android.os.Build;
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
    private static final Map<Field, String> map = new HashMap<>();

    static {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            Field keyStoreSpi = keyStore.getClass().getDeclaredField("keyStoreSpi");

            keyStoreSpi.setAccessible(true);

            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) keyStoreSpi.get(keyStore);

        } catch (Throwable t) {
            LOG("Couldn't get keyStoreSpi: " + t);
        }

        Provider provider = Security.getProvider("AndroidKeyStore");

        Provider customProvider = new CustomProvider(provider);

        Security.removeProvider("AndroidKeyStore");
        Security.insertProviderAt(customProvider, 1);
    }

    public static void init(String json) {

        try {
            JSONObject jsonObject = new JSONObject(json);

            jsonObject.keys().forEachRemaining(s -> {
                try {
                    String value = jsonObject.getString(s);
                    Field field = getFieldByName(s);

                    if (field == null) {
                        LOG("Field " + s + " not found!");
                        return;
                    }

                    map.put(field, value);
                    LOG("Save " + field.getName() + " with value: " + value);

                } catch (Throwable t) {
                    LOG("Couldn't parse " + s + " key!");
                }
            });

            spoofFields();

        } catch (Throwable t) {
            LOG("Error loading json file: " + t);
        }
    }

    static void spoofFields() {
        map.forEach((field, s) -> {
            try {
                if (s.equals(field.get(null))) return;
                field.setAccessible(true);
                field.set(null, s);
                LOG("Set " + field.getName() + " field value: " + s);
            } catch (Throwable t) {
                LOG(t.toString());
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

    static void LOG(String msg) {
        Log.d("PIF", msg);
    }
}
