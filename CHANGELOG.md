# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## ToDo
### Major
- settings dialog !!!
- gpu usage
- .net stack tracking
- add wait analyze feature

- add window properties window / details area
- add more generic system infos

### missing/empty tabs
- memory
- tokens
- services tab
- Gdi objects
- .NET assemblies and performance
- ras tab

### Minor
- add file only filter to handles
- disable detail view if more than one item is selected or allow to see combined details
- disable a lot of sub menuitems when more than 1 item is selected
- pause refresh + refresh now
- select reasonable default columns
- add run as this user menu option

- tools
-- shutdown cpmputer 
-- logg of user
-- find open file

## missing dialogs & co
- add option to create a process dump
- add service info window

## known issues:
- run as (TI) feature




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
- ProcessHacker Library integration
- Global and per process Listing of open sockets/connections

### Changed
- Improved Process Tree performance

## [0.0.1] - 2019-05-30
### Added
- ProcessHacker Library integration