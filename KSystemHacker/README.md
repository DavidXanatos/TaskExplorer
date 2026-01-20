# KSystemHacker (Legacy)

This directory contains a legacy kernel-mode driver project based on the original Process Hacker (KPH) driver.

## Status

* **Not used by TaskExplorer releases**
* **Not packaged or installed** by the official installer
* Kept in the repository for historical reference and development purposes

## Current Driver

TaskExplorer now vendors the System Informer stack. The actively used kernel driver is located at:

```
ProcessHacker/KSystemInformer
```

and is built and shipped as part of the System Informer integration.

If you are building TaskExplorer from source and looking for the runtime driver, refer to `KSystemInformer` instead of this project.

---