#pragma once

#define VERSION_MJR		2
#define VERSION_MIN 	0
#define VERSION_REV 	1
#define VERSION_UPD 	0

#ifndef STR
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

#if VERSION_UPD > 0
  #define VERSION_BIN VERSION_MJR,VERSION_MIN,VERSION_REV,VERSION_UPD
  #define VERSION_STR STR(VERSION_MJR.VERSION_MIN.VERSION_REV.VERSION_UPD)
#else
  #define VERSION_BIN VERSION_MJR,VERSION_MIN,VERSION_REV
  #define VERSION_STR STR(VERSION_MJR.VERSION_MIN.VERSION_REV)
#endif

#define MY_PRODUCT_NAME_STRING  "TaskExplorer"
#define MY_COMPANY_NAME_STRING  "xanasoft.com"

//
// The name the daemon takes on the local transport unless told otherwise - a
// named pipe on Windows, a unix socket on Linux.
//
// Here, with the other names this product is known by, rather than with the
// protocol: nothing in a packet carries it, and it is not something the two
// ends negotiate. It is what the service is *called* - the same kind of fact as
// the product name above it - and both the daemon choosing what to be called
// and a viewer looking for it on this machine read the one definition, which is
// what stops two spellings of it drifting apart in two mains.
//
//
// What an instance of the daemon is called by default.
//
// One name for two things that have to agree: the local pipe or unix socket it
// listens on, and the service it is installed as. It is what the service has
// always been called, so an installation that never touches the setting is
// untouched by the setting existing.
//
#define MY_DAEMON_NAME_STRING   "TaskExplorerServer"
#define MY_COPYRIGHT_STRING     "Copyright (C) 2019-2026 David Xanatos (xanasoft.com)"

