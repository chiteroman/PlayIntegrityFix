package es.chiteroman.playintegrityfix;

import java.security.Provider;
import java.security.ProviderException;
import java.util.Arrays;
import java.util.Locale;

public final class CustomProvider extends Provider {

    public CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());
        putAll(provider);
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.spoofDevice();

        if ("KeyPairGenerator".equals(type)) {

            if (Arrays.stream(Thread.currentThread().getStackTrace()).anyMatch(e -> e.getClassName().toLowerCase(Locale.US).contains("droidguard"))) {

                throw new ProviderException();

            }
        }

        return super.getService(type, algorithm);
    }
}
