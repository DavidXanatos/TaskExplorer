/*
* Copyright 2022-2025 David Xanatos, xanasoft.com
*
* This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

// Error codes
#define UPD_ERROR_INVALID		(-1)	// invalid parameter
#define UPD_ERROR_SCAN			(-2)	// failed to scan existing installation
#define UPD_ERROR_HASH			(-3)	// downloaded file hash mismatch
#define UPD_ERROR_DOWNLOAD		(-4)	// failed to download a file
#define UPD_ERROR_SIGN			(-5)	// update signature is invalid
#define UPD_ERROR_INTERNAL		(-6)	// internal error
#define UPD_ERROR_CANCELED		(-7)	// operation canceled
#define UPD_ERROR_EXEC			(-8)	// execution failed
#define UPD_ERROR_GET			(-9)	// failed to get update information
#define UPD_ERROR_LOAD			(-10)	// failed to load update file

#define UPDATE_FILE			"update.json"