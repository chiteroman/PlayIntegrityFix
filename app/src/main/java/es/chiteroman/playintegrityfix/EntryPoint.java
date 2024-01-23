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
    private static final Map<String, Field> fieldCache = new HashMap<>();
    private static JSONObject jsonObject;

    static {
        try {
            Provider provider = Security.getProvider("AndroidKeyStore");
            Provider customProvider = new CustomProvider(provider);
            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);
            LOG("Spoof Provider done!");
        } catch (Throwable t) {
            LOG("Error trying to spoof Provider: " + t);
        }
    }

    public static void init(String json) {
        try {
            jsonObject = new JSONObject(json);
            spoofFields();
        } catch (Throwable t) {
            LOG("Error loading json file: " + t);
        }
    }

    public static void spoofFields() {
        jsonObject.keys().forEachRemaining(s -> {
            try {
                String value = jsonObject.getString(s);
                Field field = getFieldByName(s);

                if (field == null) {
                    LOG("Field " + s + " not found!");
                    return;
                }

                if (value.equals(field.get(null))) return;

                map.put(field, value);
                LOG("Save " + field.getName() + " with value: " + value);

            } catch (Throwable t) {
                LOG("Couldn't parse " + s + " key!");
            }
        });

        LOG("Map size: " + map.size());

        map.forEach((field, s) -> {
            try {
                field.set(null, s);
                LOG("Set " + field.getName() + " field value: " + s);
            } catch (IllegalAccessException e) {
                LOG("Couldn't access " + field.getName() + " value " + s + " | Exception: " + e);
            }
        });
    }

    private static Field getFieldByName(String name) {
        if (fieldCache.containsKey(name)) {
            return fieldCache.get(name);
        }

        Field field;
        try {
            field = Build.class.getDeclaredField(name);
        } catch (NoSuchFieldException e) {
            try {
                field = Build.VERSION.class.getDeclaredField(name);
            } catch (NoSuchFieldException ex) {
                LOG("Field " + name + " not found!");
                return null;
            }
        }

        field.setAccessible(true);
        fieldCache.put(name, field);
        return field;
    }

    static void LOG(String msg) {
        Log.d("PIF", msg);
    }
}
