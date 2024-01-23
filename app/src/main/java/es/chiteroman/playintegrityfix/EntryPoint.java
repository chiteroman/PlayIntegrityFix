package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.Log;

import org.json.JSONObject;

import java.lang.reflect.Field;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public final class EntryPoint {
    private static final Map<String, String> map = new HashMap<>();

    public static void init(String json) {

        spoofProvider();

        try {
            JSONObject jsonObject = new JSONObject(json);

            jsonObject.keys().forEachRemaining(s -> {
                try {
                    String value = jsonObject.getString(s);
                    map.put(s, value);
                    LOG("Save " + s + " with value: " + value);

                } catch (Throwable t) {
                    LOG("Couldn't parse " + s + " key!");
                }
            });

            LOG("Map size: " + map.size());
            spoofFields();

        } catch (Throwable t) {
            LOG("Error loading json file: " + t);
        }
    }

    private static void spoofProvider() {
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

    public static void spoofFields() {
        map.forEach((name, value) -> {
            try {
                Field field = getFieldByName(name);
                if (field == null) {
                    LOG("Field " + name + " not found!");
                    return;
                }

                if (value.equals(field.get(null))) return;
                field.set(null, value);
                LOG("Set " + name + " field value: " + value);
            } catch (IllegalAccessException e) {
                LOG("Couldn't access " + name + " value " + value + " | Exception: " + e);
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
