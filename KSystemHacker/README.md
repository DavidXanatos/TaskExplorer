# KSystemHacker (Deprecated / Legacy)

This directory contains a legacy kernel-mode driver project based on the original Process Hacker (KPH) driver.

## Status

- **Deprecated and not used by TaskExplorer releases**
- **Not packaged or installed** by the official installer
- Contains **known vulnerabilities** and should not be used in production
- Kept only for historical / development reference

## Current Driver

TaskExplorer now vendors the System Informer stack. The actively used kernel driver is located at:

ProcessHacker/KSystemInformer

If you are building TaskExplorer from source, use `KSystemInformer` instead of this project.
