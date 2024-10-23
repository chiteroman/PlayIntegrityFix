package es.chiteroman.playintegrityfix;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import org.json.JSONObject;
import org.lsposed.hiddenapibypass.HiddenApiBypass;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public final class EntryPoint {
    public static final String TAG = "PIF";
    private static final Map<Field, String> map = new HashMap<>();
    private static final String signatureData = """
            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
            F7Xt
            """;

    private static void spoofProvider() {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            Field keyStoreSpi = keyStore.getClass().getDeclaredField("keyStoreSpi");

            keyStoreSpi.setAccessible(true);

            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) keyStoreSpi.get(keyStore);

            keyStoreSpi.setAccessible(false);

        } catch (Throwable t) {
            Log.e(TAG, "Couldn't get keyStoreSpi field!", t);
        }

        Provider provider = Security.getProvider("AndroidKeyStore");

        Provider customProvider = new CustomProvider(provider);

        Security.removeProvider("AndroidKeyStore");
        Security.insertProviderAt(customProvider, 1);
    }

    private static void spoofSignature() {
        Signature spoofedSignature = new Signature(Base64.decode(signatureData, Base64.DEFAULT));
        Parcelable.Creator<PackageInfo> originalCreator = PackageInfo.CREATOR;
        Parcelable.Creator<PackageInfo> customCreator = new CustomPackageInfoCreator(originalCreator, spoofedSignature);

        try {
            Field creatorField = findField(PackageInfo.class, "CREATOR");
            creatorField.setAccessible(true);
            creatorField.set(null, customCreator);
        } catch (Exception e) {
            Log.e(TAG, "Couldn't replace PackageInfoCreator: " + e);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            HiddenApiBypass.addHiddenApiExemptions("Landroid/os/Parcel;", "Landroid/content/pm", "Landroid/app");
        }

        try {
            Field cacheField = findField(PackageManager.class, "sPackageInfoCache");
            cacheField.setAccessible(true);
            Object cache = cacheField.get(null);
            if (cache != null) {
                Method clearMethod = cache.getClass().getMethod("clear");
                clearMethod.invoke(cache);
            }
        } catch (Exception e) {
            Log.e(TAG, "Couldn't clear PackageInfoCache: " + e);
        }

        try {
            Field creatorsField = findField(Parcel.class, "mCreators");
            creatorsField.setAccessible(true);
            Map<?, ?> mCreators = (Map<?, ?>) creatorsField.get(null);
            if (mCreators != null) mCreators.clear();
        } catch (Exception e) {
            Log.e(TAG, "Couldn't clear Parcel mCreators: " + e);
        }

        try {
            Field creatorsField = findField(Parcel.class, "sPairedCreators");
            creatorsField.setAccessible(true);
            Map<?, ?> sPairedCreators = (Map<?, ?>) creatorsField.get(null);
            if (sPairedCreators != null) sPairedCreators.clear();
        } catch (Exception e) {
            Log.e(TAG, "Couldn't clear Parcel sPairedCreators: " + e);
        }
    }

    private static Field findField(Class<?> currentClass, String fieldName) throws NoSuchFieldException {
        while (currentClass != null && !currentClass.equals(Object.class)) {
            try {
                return currentClass.getDeclaredField(fieldName);
            } catch (NoSuchFieldException e) {
                currentClass = currentClass.getSuperclass();
            }
        }
        throw new NoSuchFieldException("Field '" + fieldName + "' not found in class hierarchy of " + Objects.requireNonNull(currentClass).getName());
    }

    private static Field getBuildField(String name) {
        Field field;
        try {
            field = Build.class.getField(name);
        } catch (NoSuchFieldException e) {
            try {
                field = Build.VERSION.class.getField(name);
            } catch (NoSuchFieldException ex) {
                return null;
            }
        }
        return field;
    }

    public static void init(String json, boolean spoofProvider, boolean spoofSignature) {
        if (spoofProvider) {
            spoofProvider();
        } else {
            Log.i(TAG, "Don't spoof Provider");
        }

        if (spoofSignature) {
            spoofSignature();
        } else {
            Log.i(TAG, "Don't spoof signature");
        }

        if (TextUtils.isEmpty(json)) {
            Log.e(TAG, "Json is empty!");
            return;
        }

        JSONObject jsonObject;
        try {
            jsonObject = new JSONObject(json);
        } catch (Throwable t) {
            Log.e(TAG, "init", t);
            return;
        }

        jsonObject.keys().forEachRemaining(key -> {
            Field field = getBuildField(key);
            if (field == null) return;
            try {
                String value = jsonObject.getString(key);
                if (value.isBlank()) {
                    Log.w(TAG, "Field '" + key + "' have an empty value!");
                } else {
                    map.put(field, value);
                }
            } catch (Throwable t) {
                Log.e(TAG, "init", t);
            }
        });

        Log.i(TAG, "Parsed " + map.size() + " fields from JSON");

        spoofFields();
    }

    public static void spoofFields() {
        map.forEach((field, value) -> {
            try {
                field.setAccessible(true);
                String oldValue = (String) field.get(null);
                if (value.equals(oldValue)) {
                    field.setAccessible(false);
                    return;
                }
                field.set(null, value);
                field.setAccessible(false);
                Log.i(TAG, "Set '" + field.getName() + "' to '" + value + "'");
            } catch (Throwable t) {
                Log.e(TAG, "spoofFields", t);
            }
        });
    }
}
