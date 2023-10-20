# System Informer Kernel

- optimizes retrieval of information from the system
- enables broader inspection into the system
- informs of system activity in realtime
- assists in removal of malware

## Security

[Security Policies and Procedures](../SECURITY.md)

Because the information exposed through the driver enables, by design, broader
access into the system. Access is strictly limited to verified callers.
Access is restricted based on the state of the calling process. This involves
signing, privilege, and other state checks. If the client does not meet the
state requirements they are denied access.

Any binaries built with the intention of loading into System Informer must
have a `.sig` from the key pair integrated into the build process and driver.
Or be signed by Microsoft or an anti-malware vendor. Loading of unsigned
modules will restrict access to the driver. Third-party plugins are supported,
however when they are loaded access to the driver will be restricted as they
are not signed.

The driver tracks verified clients, restricts access by other actors on the
system, and denies access if the process is tampered with. The intention is to
discourage exploitation of the client when the driver is active. If tampering
or exploitation is detected the client is denied access.

## Development

Developers may suppress protections and state requirements by disabling secure
boot (if applicable) then enabling debug and test signing mode. Doing this will
permit the client process to be debugged and enables use of test signing keys.
```
bcdedit /debug on
bcdedit /set testsigning on
```

Developers may generate their own key pair for use in their environment.

1. execute `tools\CustomSignTool\bin\Release64\CustomSignTool.exe createkeypair kph.key public.key`
2. copy `kph.key` into `tools\CustomSignTool\resources`
3. copy the bytes for `public.key` into the `KphpPublicKeys` array in [verify.c](verify.c)
    - `KphKeyTypeProd` does not require debug and test signing enablement
    - `KphKeyTypeTest` requires debug and test signing enablement
4. regenerate dynamic data `tools\CustomBuildTool\bin\Release\CustomBuildTool.exe -dyndata`
5. rebuild the kernel _and_ user components

It's important to regenerate the dynamic data and build the kernel and user
components after creating and placing the new key pair. The driver uses the
public key to validate the dynamic data and user mode binaries. Neglecting to
do this will result in failures to load or connect to the driver.

Once these steps are completed builds of System Informer components will
generate a `.sig` file next to the output binaries. And the developer built
driver will use the specified key when doing verification checks. Any plugins
not built through the regular build process must also have their own `.sig`.
