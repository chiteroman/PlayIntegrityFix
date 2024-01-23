package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.Log;

import org.json.JSONObject;

import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.Provider;
import java.security.Security;
import java.util.Iterator;

public final class EntryPoint {
    private static final Field[] fieldsCache = new Field[Build.class.getDeclaredFields().length + Build.VERSION.class.getDeclaredFields().length];
    private static final String[] valuesCache = new String[fieldsCache.length];

    public static void init(String json) {
        spoofProvider();

        try {
            JSONObject jsonObject = new JSONObject(json);
            Iterator<String> keys = jsonObject.keys();

            int index = 0;
            while (keys.hasNext()) {
                String key = keys.next();
                try {
                    String value = jsonObject.getString(key);
                    Field field = getFieldByName(key);

                    if (field == null) {
                        LOG("Field " + key + " not found!");
                        continue;
                    }

                    fieldsCache[index] = field;
                    valuesCache[index] = value;
                    index++;

                    LOG("Save " + field.getName() + " with value: " + value);

                } catch (Throwable t) {
                    LOG("Couldn't parse " + key + " key!");
                }
            }

            LOG("Map size: " + index);
            spoofFields(index);

        } catch (Throwable t) {
            LOG("Error loading json file: " + t);
        }
    }

    private static void spoofProvider() {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            keyStore.load(null);

            Field f = keyStore.getClass().getDeclaredField("keyStoreSpi");

            f.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) f.get(keyStore);
            f.setAccessible(false);

            Provider provider = Security.getProvider("AndroidKeyStore");

            Provider customProvider = new CustomProvider(provider);

            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);

            LOG("Spoof Provider done!");

        } catch (Throwable t) {
            LOG("Error trying to spoof Provider: " + t);
        }
    }

    public static void spoofFields(int size) {
        for (int i = 0; i < size; i++) {
            Field field = fieldsCache[i];
            String value = valuesCache[i];

            try {
                if (value.equals(field.get(null))) continue;
                field.set(null, value);
                LOG("Set " + field.getName() + " field value: " + value);
            } catch (IllegalAccessException e) {
                LOG("Couldn't access " + field.getName() + " value " + value + " | Exception: " + e);
            }
        }
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
