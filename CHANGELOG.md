# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).


## [1.2.6] - 2020-06-02

### Added
- Support for translations using the QtLinguist tool


## [1.2.5] - 2020-06-01

### Added
- Added debug view tab to see the debug output of individual process, when debug monitor is enabled
- Added kernel debug log option to xprocesshacker3 driver

### Changed
- Sandboxie support needs to be enabled in the settings, as having it always on interfears with updating sandboxie
- moved services tab to the general tab as a sub tab
- moved environment tab to the general tab as a sub tab
- merged system info tab kernel objects and main system tab
- moved a lot of usefull generic code to MiscHelpers.dll

### Fixed
- fixed tab menu checks
- fixed issue with system and task info window tabs
- fixed issue process name label forcing panel size
- fixed soem more minor ui glitches



## [1.2.1] - 2020-04-27

### Added
- the TCP/IP traffic graph now show additional plots with LAN traffic based on ETW data
- services can now be stoped from the process tree contect menu

### Changed
- statis column now sorts not alphabetically but by list color
- reorganized the tool bar a bit and added a few shortcuts
- switched back to the custom installer due to "compatybility" issues

### Fixed
- cpu affinity was not properly loaded from file
- fixed more tray opening issues
- fixed issue displaying .NET assembly informations
- fixed issues with list coloring when not allcolors were enabled


## [1.2.0] - 2020-04-20

### Added
- Option to configure process name display
- Pressing the refresh toolbar button now also clears the persistence when in hold mode
- Persistent Process Presets
-- CPU, IO, Memory Priorities and CPU Affinity can be set persitence actoss process starts
-- Processes are identifyed by path wildcard paths can be used
-- The mechanism can also kill undesired processes swiftly
- add pe file viewer
- Sandboxie support, sandboxed prosesses are marked in yellow and the box thay belong to is provided in the tooltip

### Changed
- more options on main window close
-- Exit confirmation dialog can now be disabled
- by default symbols are not auto downloaded, upon selecting a thread the user will be prompted whether to download them of the internet
- updated PHlib to version 3.0.3014
- updated some default collors
- switched to Inno Setup as instller

### Fixed
- fixed when opening from tray window sometimes being empty


## [1.1.0] - 2020-01-23

### Added
- added Dark Theme Support
- added ETW monitoring of the processProvider
-- allows to capture all process cration events henc elisting of very short lived processes
-- using ETW data to set image path and command line when the process closed before we could inspect it
- added option to keep processes listed indefinetly as long as thay have still running children.
- added functionality to find some types of hidden processes, also usefull to find some already terminated processes
- added tool bar button to switch between the tree view and a list view more convinient as the last choose list sort column is remembered

### Changed
- the handle tab is now present twice once as it was and once providing only an open file list

### Fixed
- handle types are now sorted properly i.e. "[All]" is first
- fixed bug where in the unifyed list view switching to tree view was not possible
- fixed issue with some values not being initialized in CWinMainModule
- fixed High DPI scaling issues


## [1.0.2] - 2019-12-24

### Added
- settign for reverse DNS to disable it when desired
- when flushing dns cache the dns cache retention is reset as well

### Changed
- most "unknown" values now shows teh numeric value encountered
- updated PHlib to version 3.0.2812
- handle types are now sorted alphabetically

### Fixed
- an issue with the DNS cache monitoring
- fixed issue with etw event tracking for UDP traffic
- fixed issue with thread service tag not being resolved properly


## [1.0.1] - 2019-11-15

### Changed
- improved file handle info retrival
- ewt monitoring button is now disabled when running without admin rights

### Fixed
- memory leak occuring when updating per process handle list
- fixed issue with service to process association



## [1.0] - 2019-10-18

### Added
- xprocesshacker.sys can not unprotect and re protect protected processes (light)
- using ETW Events to monitor what domains individual processes querry 
--	enabled more accurate remote hostname column display

### Changed
- cleaned up PH directory
- improved process display for the case when multiple processes are sellected
- now using https://github.com/microsoft/krabsetw to monitor ETW events
- reworked socket process association
- when opening finder the search term ist selected such it can be replaced quickly

### Fixed
- no longer trying to do reverse dns on adresses that returned no results



## [0.9.75] - 2019-09-29

### Added
- priority columns now show text instead of numbers (except base priority)
- added cert display to process security sub tab
- ctrl+e now expands all process tree items
- added driver config window
- added verbose error's dialog
- added more status informations
- ad empty ini file to zip release

### Changed
- reduced cpu usage of models
- reduced cpu usage of rate counters
- moved firewall status resolution to separate threa
- reworked thread enumeration to save cpu usage
- service and socket tabs are not longer updated when thay are not visible
- gpu per proces stat update is now performed on a as needed basis
- massivly reduced treeview cpu usage by adaping configuration

### Fixed
- fixed an issue when on successfuly changing priority still an error was reported
- when starting using UAC bypass the process ended up with lower priority,
-- fixed by now always settign higher priority on startup
- fixed bug with gpu usage column display
- fixed issue "bring in front" was always disable din the process tree
- fixed issue where thread start adresses were resolved multiple times unnececerly
- fixed crash issue when logging out users
- fixed service window not closing when ok was pressed
- fixed issue with service to process association
- fixed crash bug in reverse dns lookups on win 7


## [0.9.50] - 2019-09-24

### Added
- critical status added to processes state string
- critical processes / threads have an own list color
- trying to terminate a critical process or thread wil now display an additional confirmation mesage
- ctrl+c now copys the selected rows
- formating for copying panels can be set in settings
- added additional mitigation informations
- added additional informations to geneal process info
-- details sub tab
-- security sub tab
-- app subtab
- added job id to job tab
- added app infos to process general tab

### Changed
- resolving symbols for pool limits is only triggered once the kernel objects tab gets opened
- all priority settings have now an own groupe in the process tree
- no longer keeping a handle open to all threads when thay were not used recently
- mitigation informtions are not more verbose

### Fixed
- all unselected tabs are no longer unnececerly updated at startup
- issue with private bytes displaying the wrong value
- fixed crash bug in task menu action handling
- fixed a minor issue with sid resolving



## [0.9.25] - 2019-09-15

### Added
- added remote host names resolution for the socket's tabs
- added dns cache viever with 60 min persistence 
-- the dns cache feature correlates the cached data with open sockets and provides a remote host name more reliable than reverse dns lookups
- better formating when copying panels
- added column reset option to all lists
- added f5 full refresh options
- added security explorer
- all sub windows now save their geometry
- addes Working Set Watch fature to count page faults
- added a few more pool informations
- added running object table view to kernel objects
- added Wait Chain Traversal feature to detect deadlocks
- added option to open thread tokens

### Changed
- when a new process is seen in an ETW or FW event it is now created and some masic infos are loaded
- copy cell now can copy multiple cels
- when enabling/disablign columns a refresh is triggered right away to fill in the data (in caseuse has set a ver slow refresh rate)
- improved menu layout

### Fixed
- fixed on copy cell did not work properly with multiple items selected
- fixed on cppy panel and row copying empty(hiden) columns
- fixed process tree horizontal scroll bar position reset on selection in tree
- fixed NtQueryInformationFile deadlock in windows 7 when querying \Device\VolMgrControl
- fixed issue where some deltas caused a overflow when the counter reset


## [0.9.0b] - 2019-09-10

### fixed
- fixed crash isue on windows 7 systems when opening permissions tab


## [0.9.0] - 2019-09-09

### Added
- added windows firewall monitor to show blockes connection atempts
- added network column to processes, showing if a process is or was using network sockets
- added toolbar button to set persistence to 1h
- added toolbar menu to quickly change item persistence
- added kernel object tab to system panel, including the pool table and otehr informations
- added nt object browser sub tab
- added atom table view to the kernel objects tab

### Changed
- The system info Drivers tab is now moved to a sub tab of the new kernel objects tab
- the stack trace section of the thread window can now be colapsed

### Fixed
- fixed issue disabling network adapter graphs did not work
- fixed driver view module info was not loaded



## [0.8.5] - 2019-09-01

### Added
- multi graph widget (optional individual CPU plots and individual GPU Node plots)
- plot background/text/grid colirs can now be changed
- added close (WM_CLOSE) and quit command (WM_QUIT)
- added option for rates/deltas and cpu/gpu usage to show an empty string instead of '0'
- added option to highlicht the x top resource users per column
- reduced GUI cpu load by 20% by improved issuing of cell updates in the process tree model
- added window title and status columns
- added toolbar option to quickly adjust the refresh rate
- added options to tray menu

### Changed
- system plots now set the proper length
- all tool bar drop down buttons have now a default action
- now the xprocesshacker.sys is used by default

### Fixed
- fixed issues with changing graph length
- fixed bad color contrast of sellected items
- fixed a crash (race condition) when closing
- fixed issues with cycle based cpu usage calculation
- fixed major issue with process stat display
- fixed isue with PrivateBytesDelta column
- fixed issue with asynchroniouse username resolution
- fixed cpu time columns showing a wrong value
- fixed broken protection columns DEP and ASLR 
- fixed broken file info columns size and modification time



## [0.8.0] - 2019-08-26

### Added
- added listing of unloaded DLLs (shown in gray in modules tab)
- added "Services referencing" feature to modules tab -> column
- added optional CPU cycle based CPU usage calculation
- show merged informations when more than one process is sellected
- added search (highlight) feature to the stack trace list
- added Dangerous Flags from process hacker to the token tab
- added job limits informations tab to the job tab
- added search functionality to all remaining list/tree views

### Changed
- optimized cpu uage all models are now aware of hidden columns and dont query them
- improved tree and list model performance by mor than an order of magnitude
- some values, like per process gpu sats, are not longer queried when thair columns are hidden
- reworked the token handling to optimize performance and properly hanle situations when a Token gets replaced
- moved Sid Resolving to a dedicated worker thread

### Fixes
- issue with .NET tab not getting cleared when an other process was selected
- fixed issue not all open file references being shopwed when a handle value was reused
- fixed error in global memory search
- fixed issue in token panel with the integrity combo box



## [0.7.5] - 2019-08-19
### Added
- tooltips to process tree
- added tool-bar
- bring to front on tray single click
- added bring in front command to the process tree
- disks which don't support performance queries now will get an own read/write rates graph called "unsupported" in the disk plot using ETW data
- added option to simulate UDP pseudo connections using ETW data.
- added hard fault count and delta
- added process uptime informations
- added peak handles and threads columns
- added computer menu (lock, shutdown, reboot, etc...)
- added users menu (enum users, status, log off, etc...)
- added some menu icons

### Changed
- ETW is now disabled by default, its really only needed for socket data rates
- when minimized or hiden no more ui updates to save cpu
- better number formating, long numbers are now split in groups of 3
- now using SYSTEM_PROCESS_INFORMATION_EXTENSION for process disk rates when possible, this is much more reliable than ETW
- reduced cpu usage when updating thread info (more data are now loaded only on demand)
- reduced cpu usage of window enumeration by using NtUserBuildHwndList (on windows 10) instead of FindWindowEx and by caching more data
- reduced cpu usage by using SystemFullProcessInformation to enum processes when possible (elevation required), instead of using additional calls to get the same data
- reorganized task menus for better usability

### Fixed
- fixed issue when attaching a debugger
- fixed resize issue when collapsing the side panel
- fixed crash issue with text copy in service and driver views
- fixed issue in socket listing



## [0.7] - 2019-08-09
### Added
- added a custom drivers as some AV software does not like kprocesshacker.sys, just unpack one of the following and it will be used instead
-- self-signed xprocesshacker.sys driver in xprocesshacker_test-sign.zip
-- signed with a leaked cert in xprocesshacker_hack-sign.zip PW: leaked
- added GDI objects tab
- added CPU Info tab
- added Memory/RAM Info tab including page file info
- added Disk/IO Info tab
- added Network Info tab also containing RAS infos
- added GPU Info tab
- added open path option to process tree
- added free memory commands to tools menu
- added crash dump creation

### Changed
- improved disk usag graph to show percentage of disk utilization instead of just data rate
- double click on thray now toggles show/hife of the window
- moved "Show Kernel Services" from view menu to services sub menu
- reworked system info tab

### Fixed
- fixed column issue in process picker and job tab
- fixed total/kernel/user cpu columns showing the wrong values
- fixed potential rais condition when initialising LibPH
- fixed issue with settings dialog
- fixed race condition when deleting theAPI
- fixed crash issue on 32 bit platforms
- fixed issue causing the elevation status not being resolved



## [0.6] - 2019-07-31
### Added
- .NET stack tracking support
- .NET Tab with assemblies and performance infos
- panel search can now instead of only filtering also just highlight the results
- when encountering an access denided we now try to start an elevated worker and retry
- added option to edit service dependencies
- forked QTabBar and QTabWidget to provide a windows like multiRow operation mode

### Changed
- taskexplorer can now be started as elevated worker or 32 bit worker not just as a service
- improved stack trace display handling
- improved service info window

### Fixed
- memory view being unnececerly refreshed
- fixed dpi scling issue



## [0.5] - 2019-07-22
### Added
- added search filter to all panels by pressinf Ctrl+F
- find open file/handles/dll's
- find strings in program memory
- extended QHexEditor with the ability to search for unicode (UTF-16) strings
- added context menu to qhexeditor
- terminate tasks and close handles/sockets/windows using the del key
- added status bar infos
- add system info window in case one closed the system info panel
- disable system info tab settings when panel is collapsed

### Changed 
- reworked tree graph's for better performance

### Fixed
- fixed an issue where reused handles woule be colored as to be removed permanently
- fixed column order getting messed up in process tree when adding/removing columns
- QHexEditor does not longer allow to replace a string with a different length string when its not in insert mode
- fixed crash bug in CWinToken::InitStaticData
- fixed a many of small bugs preventing compilation of the UI on Linux



## [0.4] - 2019-07-15
### Added
- gpu usage statistics
- option to reset graph
- pause refresh + refresh now
- add option to fully refresh all services
- added option to inject a dll into any running process
- use profile directory to save settings
- option to customize graph bars from the graph bar context menu
- graph now have tool tips with detailed informations
- settings dialog with options and the ability to customize list colors

### Changed
- made most dialogs resizable
- select reasonable default columns

### Fixed
- a 32 bit version can not longer be started on a 64-bit system as it would not work correctly, however it tries to start a 64 bit version if avilable.
- fixed process service tab not working



## [0.3] - 2019-07-09
### Added
- tokens tab with advanced infos
- improved handle window
-- show job info window 
-- show token info window 
-- show task info window
-- open file lokation
-- open registry key
-- read/write section memory
-- type filter now enumerates all types
- added size info to section type handle

### Changed
- rewoked sid to username resolution now using a worker thread to improve performance
- CWinProcess does nto longer handle sid_user/token informations all is doem by CWinToken instead

### Fixed
- fixed issue with the first graph text not being displayed
- fixed an issue causing the client to wait for 10 sec on shutdown



## [0.2] - 2019-07-05
### Added
- memory tab, with options to dump the memory, free it or change access permissions
- advanced memory editor window
-- forked qhexedit2 https://github.com/DavidXanatos/qhexedit to ad missing functionality, edit, lock mode, etc...
-- added a QHexEditor class to qhexedit implementing a generic hex editor dialog with options and search capability

### Changed
- I/O stats does not longer show ETW values when the is not monitoring ETW events

### Fixed
- fixed Uptime column not being refreshed



## [0.1] - 2019-06-30
### Added
- service property window, including all pages form the extended services plugin
- cpu and memory usage in tray icon
- option to start elevanted without an UAC prompt
- auto run using windows registry
- build x86 binaries
- add option to create a process dump
- afility to run a program with the Token of an other program (run as this user)
- services tab showing services hosted by the selcted process
- add go to service key
- type filter to handles
- added missing handle actions
- add window properties details area

### Changed 
- paged memory usage is now extracted form pagefile informtions
- monitoring of ETW events can be disabled and re enabled in tool menu
- handle property detail are no a tree widget

### Fixed
- crash when querying samba datarate and getting null
- run as feature forks now
- fixed issue with CoInitialize



## [0.0.9] - 2019-06-24
### Added
- general system tab
- process and system stats
- job tab
- add process pid picker dialog
- add ras/vpn graph
- process and threads are listed for 5 sec after termination
- grid to all lists
- service column to process tree
- list colloring
- context menu to services view
- tools menu
- scm permissions
- processor affinity dialog
- organized columns menu for the process tree in sub menus
- added option to restart elevated
- added graphs to process tree, CPU, Memory, IO/DiskIO, Network
- linux style cpu usage i.e. 1 core = 100% so > 1 core -> > 100%
- create service dialog
- taskexplorer can now be started as service and listen for commands
- option to start programs as TrustedInstaller without using a service
- run as dialog

### Changed 
- graphs can now be resized with a splitter
- improved process tree sorting behavioure to be more like in process hacker
- samba stats now using NetStatisticsGet instead of speculating on ETW events
- improved global network traffic logging now using GetIfTable2 instead of ETW events
- improved MMapIO display now it works as expected and disk IO got its own graph
- all files list now works for non enevated users
- driver tab now uses NtQuerySystemInformation(SystemModuleInformation to enumerate drivers

### Fixed
- memory leak when running without unelevated and vieving all files list


## [0.0.8] - 2019-06-17
### Added
- window graph 
- window column in process tree
- adde dtreee to window tab
- environment vartiable view
- context menu to environment vartiable 
- all thread model columns
- all process model columns
- context menu to sockets view
- context menu to handles view
- context menu to modules view
- CAbstractTask Class to abstract threads and processes
- linked split tree selection
- context menu to threads view
- context menu to process tree
- double click on a process opens the Info panel in a new window
- context menu to windows view
- permissions dialog to threads processes and handles
- saving window position spliter positions column selection and more
- added sys tray icon
- general task tab 


### Changed

### Fixed
- failed to remove old modules
- fixed windows tree not opening

## [0.0.7] - 2019-06-10
### Added
- network stats
- disk stats
- CPU Usage graph
- Memory Usage graph
- FileIO and DIskIO graph
- Network graph
- Samba graph
- GUI/USER Object graph
- Handles graph

### Changed
- reworked the process tree handling for better performance
- added abstract tree model
- added abstract list model

### Fixed
- endpoint swap of incomming udp packets
- fixed memory leak during all files enumeration

## [0.0.6] - 2019-06-08
### Added
- module view

## [0.0.5] - 2019-06-04
### Added
- service list
- driver list

## [0.0.4] - 2019-06-02
### Added
- Thread list / Stack trace
- added All Files view

## [0.0.3] - 2019-06-01
### Added
- File / Handle list
- Disk/Network stat event listener


## [0.0.2] - 2019-05-31
### Added
- Global and per process Listing of open sockets/connections

### Changed
- Improved Process Tree performance

## [0.0.1] - 2019-05-30
### Added
- ProcessHacker Library integration
