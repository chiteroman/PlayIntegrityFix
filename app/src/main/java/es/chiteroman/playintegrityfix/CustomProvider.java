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
        EntryPoint.spoofFields();

        EntryPoint.LOG(String.format("Service: '%s' | Algorithm: '%s'", type, algorithm));

        if ("AndroidKeyStore".equals(algorithm)) {
            Service service = super.getService(type, algorithm);
            EntryPoint.LOG(service.toString());
            throw new ProviderException();
        }

        return super.getService(type, algorithm);
    }
}
