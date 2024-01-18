package es.chiteroman.playintegrityfix;

import java.security.Provider;
import java.security.ProviderException;

public final class CustomProvider extends Provider {

    public CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());

        putAll(provider);
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.LOG(String.format("[Service] Type: '%s' | Algorithm: '%s'", type, algorithm));

        if ("AndroidKeyStore".equals(algorithm)) {
            EntryPoint.spoofFields();
            throw new ProviderException();
        }

        return super.getService(type, algorithm);
    }
}
