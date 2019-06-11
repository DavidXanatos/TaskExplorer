# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## ToDo
### Major
- more columns for process tree
- save window configuration, open tabs sorting etc
- perf mon
- general tab, system and task
- task infos: memory, tokens , windows, environment
- graphs
- gpu usage
- tools
- .net stack tracking
- disk IO tab
- add window enumeration and listing

- add sub menus to all lists

-- module view
--- add unload

-- thread view
--- terminate
--- suspend/resume
--- calcen I/O ?
--- analyze
--- affinity
--- []critical
--- (permissions)
--- windows
--- priority
--- io priority
--- page priority

-- handleview
--- close 
--- []inherit
--- []protect
--- event
---- set
---- reset
---- pulse
--- Semaphore
---- Acquire
---- Release


### Minor

- windows service pid change etc
- handle tab details and filter
- threads tab is missing stuff



## [0.0.7] - 2019-06-10
### Added
- added network stats
- added disk stats
- added CPU Usage graph
- added Memory Usage graph
- added FileIO and DIskIO graph
- added Network graph
- added Samba graph
- added GUI/USER Object graph
- added Handles graph

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