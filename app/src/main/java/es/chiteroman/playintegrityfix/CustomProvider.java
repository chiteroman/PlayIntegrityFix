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
        EntryPoint.spoofDevice();

        if ("KeyPairGenerator".equals(type)) throw new ProviderException();

        return super.getService(type, algorithm);
    }
}
