package es.chiteroman.playintegrityfix;

import java.security.Provider;
import java.security.ProviderException;

public final class CustomProvider extends Provider {

    private static final String ANDROID_KEY_STORE = "AndroidKeyStore";
    private static final String EXCEPTION_MESSAGE = "AndroidKeyStore algorithm is not supported.";

    public CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());
        putAll(provider);
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.LOG(String.format("[Service] Type: '%s' | Algorithm: '%s'", type, algorithm));

        if (ANDROID_KEY_STORE.equals(algorithm)) {
            EntryPoint.spoofFields();
            throw new ProviderException(EXCEPTION_MESSAGE);
        }

        return super.getService(type, algorithm);
    }
}
