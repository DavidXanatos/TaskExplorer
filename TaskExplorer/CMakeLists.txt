#
# TaskExplorer - the application itself.
#
# Sources are split three ways:
#   TE_SOURCES        - portable core + GUI
#   TE_SOURCES_WIN    - the Win32/ProcessHacker backend and the GUI views that
#                       have no meaning outside Windows
#   TE_SOURCES_LINUX  - the /proc + systemd backend (API/Linux)
#
# A GUI file counts as "portable" if it already carries #ifdef WIN32 guards
# around its Windows-specific parts; files that are Windows top to bottom are
# listed under TE_SOURCES_WIN instead of being guarded wholesale.
#

set(TE_SOURCES
    main.cpp
    stdafx.cpp
    stdafx.h
    version.h

    SVC/TaskService.cpp
    SVC/TaskService.h

    # ---- portable API layer (abstract bases + shared logic) ----
    API/AbstractInfo.cpp
    API/AbstractInfo.h
    API/AbstractTask.cpp
    API/AbstractTask.h
    API/AssemblyList.cpp
    API/AssemblyList.h
    API/DNSEntry.cpp
    API/DNSEntry.h
    API/DriverInfo.cpp
    API/DriverInfo.h
    API/HandleInfo.cpp
    API/HandleInfo.h
    API/HeapInfo.cpp
    API/HeapInfo.h
    API/MemDumper.cpp
    API/MemDumper.h
    API/MemoryInfo.cpp
    API/MemoryInfo.h
    API/MiscStats.h
    API/ModuleInfo.cpp
    API/ModuleInfo.h
    API/PersistentPreset.cpp
    API/PersistentPreset.h
    API/ProcessInfo.cpp
    API/ProcessInfo.h
    API/ServiceInfo.cpp
    API/ServiceInfo.h
    API/SocketInfo.cpp
    API/SocketInfo.h
    API/StackTrace.cpp
    API/StackTrace.h
    API/StringInfo.h
    API/SystemAPI.cpp
    API/SystemAPI.h
    API/ThreadInfo.cpp
    API/ThreadInfo.h
    API/WndInfo.cpp
    API/WndInfo.h
    API/Finders/AbstractFinder.cpp
    API/Finders/AbstractFinder.h
    API/Monitors/DiskMonitor.cpp
    API/Monitors/DiskMonitor.h
    API/Monitors/GpuMonitor.cpp
    API/Monitors/GpuMonitor.h
    API/Monitors/NetMonitor.cpp
    API/Monitors/NetMonitor.h

    # ---- portable support code ----
    Common/Buffer.cpp
    Common/IncrementalPlot.cpp
    Common/IncrementalPlot.h
    Common/Strings.cpp
    Common/TabPanel.cpp
    Common/TabPanel.h
    Common/Variant.cpp
    Common/XVariant.cpp

    # ---- GUI ----
    GUI/AffinityDialog.cpp
    GUI/AffinityDialog.h
    GUI/CustomItemDelegate.h
    GUI/GraphBar.cpp
    GUI/GraphBar.h
    GUI/MemoryEditor.cpp
    GUI/MemoryEditor.h
    GUI/MultiErrorDialog.cpp
    GUI/MultiErrorDialog.h
    GUI/OnlineUpdater.cpp
    GUI/OnlineUpdater.h
    GUI/NewService.cpp
    GUI/NewService.h
    GUI/PersistenceConfig.cpp
    GUI/PersistenceConfig.h
    GUI/ProcessPicker.cpp
    GUI/ProcessPicker.h
    GUI/ProcessTree.cpp
    GUI/ProcessTree.h
    GUI/RunAsDialog.cpp
    GUI/RunAsDialog.h
    GUI/RunDialog.cpp
    GUI/RunDialog.h
    GUI/SettingsWindow.cpp
    GUI/SettingsWindow.h
    GUI/StatsView.cpp
    GUI/StatsView.h
    GUI/TaskExplorer.cpp
    GUI/TaskExplorer.h
    GUI/TaskView.cpp
    GUI/TaskView.h

    GUI/Filters/ProcessFilterModel.cpp
    GUI/Filters/ProcessFilterModel.h

    GUI/Models/DnsModel.cpp
    GUI/Models/DnsModel.h
    GUI/Models/DriverModel.cpp
    GUI/Models/DriverModel.h
    GUI/Models/HandleModel.cpp
    GUI/Models/HandleModel.h
    GUI/Models/HeapModel.cpp
    GUI/Models/HeapModel.h
    GUI/Models/MemoryModel.cpp
    GUI/Models/MemoryModel.h
    GUI/Models/ModuleModel.cpp
    GUI/Models/ModuleModel.h
    GUI/Models/ProcessModel.cpp
    GUI/Models/ProcessModel.h
    GUI/Models/ServiceModel.cpp
    GUI/Models/ServiceModel.h
    GUI/Models/SocketModel.cpp
    GUI/Models/SocketModel.h
    GUI/Models/StringModel.cpp
    GUI/Models/StringModel.h
    GUI/Models/ThreadModel.cpp
    GUI/Models/ThreadModel.h
    GUI/Models/WindowModel.cpp
    GUI/Models/WindowModel.h

    GUI/Search/HandleSearch.cpp
    GUI/Search/HandleSearch.h
    GUI/Search/MemorySearch.cpp
    GUI/Search/MemorySearch.h
    GUI/Search/ModuleSearch.cpp
    GUI/Search/ModuleSearch.h
    GUI/Search/SearchWindow.cpp
    GUI/Search/SearchWindow.h
    GUI/Search/StringView.cpp
    GUI/Search/StringView.h

    GUI/SystemInfo/CPUView.cpp
    GUI/SystemInfo/CPUView.h
    GUI/SystemInfo/DiskView.cpp
    GUI/SystemInfo/DiskView.h
    GUI/SystemInfo/DnsCacheView.cpp
    GUI/SystemInfo/DnsCacheView.h
    GUI/SystemInfo/GPUView.cpp
    GUI/SystemInfo/GPUView.h
    GUI/SystemInfo/NetworkView.cpp
    GUI/SystemInfo/NetworkView.h
    GUI/SystemInfo/RAMView.cpp
    GUI/SystemInfo/RAMView.h
    GUI/SystemInfo/ServiceListWidget.cpp
    GUI/SystemInfo/ServiceListWidget.h
    GUI/SystemInfo/ServicesView.cpp
    GUI/SystemInfo/ServicesView.h
    GUI/SystemInfo/SystemInfoView.cpp
    GUI/SystemInfo/SystemInfoView.h
    GUI/SystemInfo/SystemInfoWindow.cpp
    GUI/SystemInfo/SystemInfoWindow.h
    GUI/SystemInfo/SystemView.cpp
    GUI/SystemInfo/SystemView.h
    GUI/SystemInfo/KernelInfo/DriversView.cpp
    GUI/SystemInfo/KernelInfo/DriversView.h

    GUI/TaskInfo/EnvironmentView.cpp
    GUI/TaskInfo/EnvironmentView.h
    GUI/TaskInfo/HandlesView.cpp
    GUI/TaskInfo/HandlesView.h
    GUI/TaskInfo/MemoryView.cpp
    GUI/TaskInfo/MemoryView.h
    GUI/TaskInfo/ModulesView.cpp
    GUI/TaskInfo/ModulesView.h
    GUI/TaskInfo/ProcessView.cpp
    GUI/TaskInfo/ProcessView.h
    GUI/TaskInfo/SocketsView.cpp
    GUI/TaskInfo/SocketsView.h
    GUI/TaskInfo/StackView.cpp
    GUI/TaskInfo/StackView.h
    GUI/TaskInfo/TaskInfoView.cpp
    GUI/TaskInfo/TaskInfoView.h
    GUI/TaskInfo/TaskInfoWindow.cpp
    GUI/TaskInfo/TaskInfoWindow.h
    GUI/TaskInfo/ThreadsView.cpp
    GUI/TaskInfo/ThreadsView.h
    GUI/TaskInfo/WindowsView.cpp
    GUI/TaskInfo/WindowsView.h

    # ---- forms & resources ----
    Forms/NewService.ui
    Forms/RunAsDialog.ui
    Forms/RunDialog.ui
    Forms/SettingsWindow.ui
    Resources/TaskExplorer.qrc
)

#
# Windows-only: the entire Win32 backend plus the GUI surfaces that expose
# Windows concepts with no Linux counterpart (tokens, jobs, GDI, .NET, RPC,
# kernel pool/atom/object tables, Sandboxie, the KTaskExplorer driver UI).
#
set(TE_SOURCES_WIN
    API/Windows/DnsResolver.cpp
    API/Windows/DnsResolver.h
    API/Windows/ProcessHacker.cpp
    API/Windows/ProcessHacker.h
    API/Windows/RpcEndpoint.cpp
    API/Windows/RpcEndpoint.h
    API/Windows/RunAsTI.cpp
    API/Windows/SandboxieAPI.cpp
    API/Windows/SandboxieAPI.h
    API/Windows/SidResolver.cpp
    API/Windows/SidResolver.h
    API/Windows/SymbolProvider.cpp
    API/Windows/SymbolProvider.h
    API/Windows/WinAdmin.cpp
    API/Windows/WinAdmin.h
    API/Windows/WindowsAPI.cpp
    API/Windows/WindowsAPI.h
    API/Windows/WindowsHelpers.cpp
    API/Windows/WinDriver.cpp
    API/Windows/WinDriver.h
    API/Windows/WinDumper.cpp
    API/Windows/WinDumper.h
    API/Windows/WinGDI.cpp
    API/Windows/WinGDI.h
    API/Windows/WinHandle.cpp
    API/Windows/WinHandle.h
    API/Windows/WinHeap.cpp
    API/Windows/WinHeap.h
    API/Windows/WinHelper.cpp
    API/Windows/WinHelper.h
    API/Windows/WinJob.cpp
    API/Windows/WinJob.h
    API/Windows/WinMemIO.cpp
    API/Windows/WinMemIO.h
    API/Windows/WinMemory.cpp
    API/Windows/WinMemory.h
    API/Windows/WinModule.cpp
    API/Windows/WinModule.h
    API/Windows/WinPoolEntry.cpp
    API/Windows/WinPoolEntry.h
    API/Windows/WinProcess.cpp
    API/Windows/WinProcess.h
    API/Windows/WinService.cpp
    API/Windows/WinService.h
    API/Windows/WinSocket.cpp
    API/Windows/WinSocket.h
    API/Windows/WinThread.cpp
    API/Windows/WinThread.h
    API/Windows/WinToken.cpp
    API/Windows/WinToken.h
    API/Windows/WinWnd.cpp
    API/Windows/WinWnd.h

    API/Windows/Finders/WinHandleFinder.cpp
    API/Windows/Finders/WinModuleFinder.cpp
    API/Windows/Finders/WinStringFinder.cpp

    API/Windows/Monitors/EtwEventMonitor.cpp
    API/Windows/Monitors/FwEventMonitor.cpp
    API/Windows/Monitors/WinDbgMonitor.cpp
    API/Windows/Monitors/WinDiskMonitor.cpp
    API/Windows/Monitors/WinGpuMonitor.cpp
    API/Windows/Monitors/WinNetMonitor.cpp

    API/Windows/ProcessHacker/appsup.cpp
    API/Windows/ProcessHacker/AssemblyEnum.cpp
    API/Windows/ProcessHacker/clrsup.c
    API/Windows/ProcessHacker/db.cpp
    API/Windows/ProcessHacker/DotNet.cpp
    API/Windows/ProcessHacker/DotNetCounters.c
    API/Windows/ProcessHacker/fw.cpp
    API/Windows/ProcessHacker/GpuCounters.c
    API/Windows/ProcessHacker/ksyscall.c
    API/Windows/ProcessHacker/memprv.cpp
    API/Windows/ProcessHacker/ndis.cpp
    API/Windows/ProcessHacker/PhSvc.cpp
    API/Windows/ProcessHacker/pool.cpp
    API/Windows/ProcessHacker/ProcMtgn.cpp
    API/Windows/ProcessHacker/RunAs.cpp
    API/Windows/ProcessHacker/storage.cpp
    API/Windows/ProcessHacker/syssccpu.cpp

    API/Windows/SystemHacker/SystemHacker.cpp
    API/Windows/SystemHacker/xph.c

    API/Windows/WinHelpers/FindTool.cpp
    API/Windows/WinHelpers/WindowFromPointEx.cpp
    API/Windows/WinHelpers/FuncHook/HookUtils.cpp
    API/Windows/WinHelpers/FuncHook/Detours/creatwth.cpp
    API/Windows/WinHelpers/FuncHook/Detours/detours.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disasm.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disolarm.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disolarm64.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disolia64.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disolx64.cpp
    API/Windows/WinHelpers/FuncHook/Detours/disolx86.cpp
    API/Windows/WinHelpers/FuncHook/Detours/image.cpp
    API/Windows/WinHelpers/FuncHook/Detours/modules.cpp

    API/Windows/WinHelpers/InjectDll/injdll32.c

    GUI/DriverWindow.cpp
    GUI/DriverWindow.h
    GUI/SecurityExplorer.cpp
    GUI/SecurityExplorer.h
    GUI/WaitChainDialog.cpp
    GUI/WaitChainDialog.h
    GUI/WndFinder.cpp
    GUI/WsWatchDialog.cpp
    GUI/WsWatchDialog.h

    GUI/Models/GDIModel.cpp
    GUI/Models/GDIModel.h
    GUI/Models/NtObjectModel.cpp
    GUI/Models/NtObjectModel.h
    GUI/Models/PoolModel.cpp
    GUI/Models/PoolModel.h
    GUI/Models/RpcModel.cpp
    GUI/Models/RpcModel.h

    GUI/SystemInfo/RpcView.cpp
    GUI/SystemInfo/RpcView.h
    GUI/SystemInfo/WinSvcShutdown.cpp
    GUI/SystemInfo/WinSvcShutdown.h
    GUI/SystemInfo/WinSvcTrigger.cpp
    GUI/SystemInfo/WinSvcTrigger.h
    GUI/SystemInfo/WinSvcWindow.cpp
    GUI/SystemInfo/WinSvcWindow.h
    GUI/SystemInfo/KernelInfo/AtomView.cpp
    GUI/SystemInfo/KernelInfo/AtomView.h
    GUI/SystemInfo/KernelInfo/NtObjectView.cpp
    GUI/SystemInfo/KernelInfo/NtObjectView.h
    GUI/SystemInfo/KernelInfo/PoolView.cpp
    GUI/SystemInfo/KernelInfo/PoolView.h
    GUI/SystemInfo/KernelInfo/RunObjView.cpp
    GUI/SystemInfo/KernelInfo/RunObjView.h

    GUI/TaskInfo/DebugView.cpp
    GUI/TaskInfo/DebugView.h
    GUI/TaskInfo/DotNetView.cpp
    GUI/TaskInfo/DotNetView.h
    GUI/TaskInfo/GDIView.cpp
    GUI/TaskInfo/GDIView.h
    GUI/TaskInfo/HeapView.cpp
    GUI/TaskInfo/HeapView.h
    GUI/TaskInfo/JobView.cpp
    GUI/TaskInfo/JobView.h
    GUI/TaskInfo/SbieView.cpp
    GUI/TaskInfo/SbieView.h
    GUI/TaskInfo/TokenView.cpp
    GUI/TaskInfo/TokenView.h

    Forms/DriverWindow.ui
    Forms/SecurityExplorer.ui
    Forms/WinSvcShutdown.ui
    Forms/WinSvcTrigger.ui
    Forms/WinSvcWindow.ui
)

#
# Linux backend. Mirrors the class layout of API/Windows so the abstract layer
# in API/*.h stays the single source of truth for both platforms.
#
set(TE_SOURCES_LINUX
    # Linux-only GUI: the counterparts of the Windows Job and Token views.
    GUI/TaskInfo/CGroupView.cpp
    GUI/TaskInfo/CGroupView.h
    GUI/TaskInfo/SecurityView.cpp
    GUI/TaskInfo/SecurityView.h

    API/Linux/LinuxAPI.cpp
    API/Linux/LinuxAPI.h
    API/Linux/LinuxProcess.cpp
    API/Linux/LinuxProcess.h
    API/Linux/LinuxThread.cpp
    API/Linux/LinuxThread.h
    API/Linux/LinuxHandle.cpp
    API/Linux/LinuxHandle.h
    API/Linux/LinuxSocket.cpp
    API/Linux/LinuxSocket.h
    API/Linux/LinuxModule.cpp
    API/Linux/LinuxModule.h
    API/Linux/LinuxService.cpp
    API/Linux/LinuxService.h
    API/Linux/LinuxDriver.cpp
    API/Linux/LinuxDriver.h
    API/Linux/LinuxMemory.cpp
    API/Linux/LinuxMemory.h
    API/Linux/LinuxMemIO.cpp
    API/Linux/LinuxMemIO.h
    API/Linux/LinuxHeap.cpp
    API/Linux/LinuxHeap.h
    API/Linux/LinuxWnd.cpp
    API/Linux/LinuxWnd.h
    API/Linux/LinuxDumper.cpp
    API/Linux/LinuxDumper.h
    API/Linux/LinuxHelper.cpp
    API/Linux/LinuxHelper.h
    API/Linux/LinuxElf.cpp
    API/Linux/LinuxElf.h
    API/Linux/X11Helper.cpp
    API/Linux/X11Helper.h
    API/Linux/ProcFs.cpp
    API/Linux/ProcFs.h
    API/Linux/SockDiag.cpp
    API/Linux/SockDiag.h
    API/Linux/UdevMonitor.cpp
    API/Linux/UdevMonitor.h

    API/Linux/Finders/LinuxHandleFinder.cpp
    API/Linux/Finders/LinuxHandleFinder.h
    API/Linux/Finders/LinuxModuleFinder.cpp
    API/Linux/Finders/LinuxModuleFinder.h
    API/Linux/Finders/LinuxStringFinder.cpp
    API/Linux/Finders/LinuxStringFinder.h

    API/Linux/Monitors/LinuxDiskMonitor.cpp
    API/Linux/Monitors/LinuxDiskMonitor.h
    API/Linux/Monitors/LinuxGpuMonitor.cpp
    API/Linux/Monitors/LinuxGpuMonitor.h
    API/Linux/Monitors/LinuxNetMonitor.cpp
    API/Linux/Monitors/LinuxNetMonitor.h
)

if(WIN32)
    list(APPEND TE_SOURCES ${TE_SOURCES_WIN})
elseif(UNIX AND NOT APPLE)
    list(APPEND TE_SOURCES ${TE_SOURCES_LINUX})
endif()

qt_add_executable(TaskExplorer WIN32 ${TE_SOURCES})

target_include_directories(TaskExplorer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/API"
)

set_property(TARGET TaskExplorer PROPERTY AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/Forms")

set_property(TARGET TaskExplorer PROPERTY AUTORCC_OPTIONS
    --root /)

target_link_libraries(TaskExplorer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    MiscHelpers
    qextwidgets
    qhexedit
    qwt
    qtservice
    qtsingleapp
)

if(WIN32)
    target_include_directories(TaskExplorer PRIVATE
        "${CMAKE_SOURCE_DIR}/ProcessHacker/phlib/include"
        "${CMAKE_SOURCE_DIR}/ProcessHacker/kphlib/include"
        "${CMAKE_SOURCE_DIR}/ProcessHacker/phnt/include"
    )
    target_link_libraries(TaskExplorer PRIVATE
        ntdll userenv uxtheme aclui winsta iphlpapi ws2_32 phlib
        netapi32 rasapi32 taskschd shlwapi cfgmgr32 gdi32 wbemuuid
        dnsapi rpcrt4 thirdparty windowscodecs comdlg32
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_sources(TaskExplorer PRIVATE
            API/Windows/WinHelpers/InjectDll/injdll64.c
            API/Windows/WinHelpers/InjectDll/injdll32_64.c
        )
    else()
        target_sources(TaskExplorer PRIVATE
            API/Windows/WinHelpers/InjectDll/injdll32_32.c
        )
    endif()
    target_sources(TaskExplorer PRIVATE Resources/TaskExplorer.rc)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(TaskExplorer PRIVATE Qt6::DBus)

    # Optional; see the top-level CMakeLists. Without it X11Helper compiles to
    # stubs and the window views stay empty.
    if(TE_HAVE_X11)
        target_compile_definitions(TaskExplorer PRIVATE TE_HAVE_X11)
        target_link_libraries(TaskExplorer PRIVATE X11::X11)
    endif()
endif()

target_precompile_headers(TaskExplorer PRIVATE stdafx.h)

#
# Translations. lrelease only - the .ts files are maintained by hand and by
# Qt Linguist, so the build must not run lupdate over them.
#
qt_add_lrelease(TaskExplorer
    TS_FILES
        taskexplorer_de.ts
        taskexplorer_en.ts
        taskexplorer_it.ts
        taskexplorer_ja.ts
        taskexplorer_pl.ts
        taskexplorer_pt.ts
        taskexplorer_tr.ts
        taskexplorer_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE TE_QM_FILES
)

add_custom_command(TARGET TaskExplorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TE_OUTPUT_DIR}/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TE_QM_FILES} "${TE_OUTPUT_DIR}/translations"
    VERBATIM
)

#
# Distribution logos for the Linux system-info panel.
#
# Deliberately loose files next to the executable rather than compiled into the
# .qrc, so a logo can be added for a new distribution without rebuilding. See
# Resources/DistroLogos/README.txt.
#
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET TaskExplorer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/DistroLogos"
                "${TE_OUTPUT_DIR}/DistroLogos"
        COMMENT "Copying distribution logos"
        VERBATIM
    )
endif()

#
# Bundle Qt next to the executables.
#
# Runs after TaskHelper too, so both binaries are scanned for what they need -
# the helper links elfutils, which the GUI does not. Ordered by a dependency on
# the TaskHelper target so it cannot run before that binary exists.
#
# The script is incremental: it copies only what has changed, so this costs
# almost nothing on rebuilds despite Qt being large.
#
if(UNIX AND NOT APPLE AND TE_BUNDLE_QT)
    add_dependencies(TaskExplorer TaskHelper)

    add_custom_command(TARGET TaskExplorer POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/Installer/deploy-linux.sh"
                "${TE_OUTPUT_DIR}"
                "${TE_QT_PREFIX}"
                "${TE_OUTPUT_DIR}/TaskExplorer"
                "${TE_OUTPUT_DIR}/TaskHelper"
        COMMENT "Bundling Qt runtime into ${TE_OUTPUT_DIR}"
        VERBATIM
    )
endif()
