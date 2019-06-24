Qt Solutions Component: Service

The QtService component is useful for developing Windows services
and Unix daemons.



Version history:

2.0: - Reimplemented for Qt 4 with improved API.

2.1: - More feedback while installing / uninstalling.
     - Unix: Documentation about installing service improved.
     - Windows: Added account and password arguments to install().
     - Windows: Report status correcty when stopping through the
     controller.
     - Windows: Send command IDs correctly.

2.2: - Unix: logMessage() works properly with '%' characters.
     - Windows: Dependency on the QtGui library removed.

2.3: - Unix: Using QProcess in a service no longer leaves zombie
     processes around.
     - Windows: Place controller's receiver object in proper thread.
     (makes QtService work with Qt 4.2).
     - Compilation fixes for MinGW.

2.4: - Windows: Do net require Administrator privileges just to
     start service.
     - Improved command-line help text.
     - Document the caveats of using GUI services, including that Vista
     does not support it.
     - Added -w(ait) argument to controller example
     - Added doc about usage on Windows Vista
     - Windows: Fix: GUI services would terminate on logoff.
     - Windows: internal redesign of threading. Fixes issue where
     signal/slot connections created before start() would not work.
     Fixes problem/warning about QApplication not created in main
     thread.

2.5: - Fixes: crash on OS X Leopard. Note: On mac, main() will now
     be executed both in the starting process and in the forked,
     service process. This is consistent with the behaviour on Windows.
     - Fixes: Improved logging of multi-line messages on Unix.

2.6: - Misc. minor fixes.
     - LGPL release.

