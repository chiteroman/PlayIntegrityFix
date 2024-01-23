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
    private static final Map<String, Field> fieldCache = new HashMap<>();
    private static JSONObject jsonObject = new JSONObject();

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
                Object value = jsonObject.get(s);
                setFieldValue(s, value);
            } catch (Throwable ignored) {
            }
        });
    }

    private static void setFieldValue(String name, Object value) {
        if (name == null || value == null || name.isEmpty()) return;

        Field field = getField(name);
        if (field == null) return;

        try {
            Object oldValue = field.get(null);
            if (!value.equals(oldValue)) {
                field.set(null, value);
                LOG("Set [" + name + "] field value to [" + value + "]");
            }
        } catch (IllegalAccessException e) {
            LOG("Couldn't modify field: " + e);
        }
    }

    private static Field getField(String name) {
        if (fieldCache.containsKey(name)) {
            return fieldCache.get(name);
        }

        Field field = null;
        try {
            field = Build.class.getDeclaredField(name);
        } catch (NoSuchFieldException e) {
            try {
                field = Build.VERSION.class.getDeclaredField(name);
            } catch (NoSuchFieldException ex) {
                LOG("Couldn't find field: " + e);
                return null;
            }
        }

        field.setAccessible(true);
        fieldCache.put(name, field);
        return field;
    }

    public static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }
}
