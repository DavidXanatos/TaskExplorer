# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## ToDo
### Major
- graphs in process tree
- gpu usage
- tools
- .net stack tracking

- add service/drivers info window
- add wait analyze feature

- add security/permissions windows 
--	PhCreateSecurityPage (job)
--	PhEditSecurity (Service Control Manager, Service, Token, )


### missing/empty tabs
- memory
- tokens
- Job
- Gdi objects
- .NET assemblies and performance
- general system tab
- add process and system statistics tab

### Minor

- windows service pid change etc
- process persistence
- add file only filter to handles
- disable detail view if more than one item is selected or allow to see combined details
- disable a lot of sub menuitems when more than 1 item is selected
- add processor affinity dialog

- add window properties window / details area
- added context menu to services/drivers view
- add option to create a process dump



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