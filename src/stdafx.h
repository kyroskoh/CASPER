// Copyright 2010 ESRI
// 
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
// 
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
// 
// See the use restrictions at http://help.arcgis.com/en/sdk/10.1/usageRestrictions.htm

#pragma once

#ifndef STRICT
#define STRICT
#endif

// memory leak detection new operator replacment
/// ref: http://stackoverflow.com/questions/3202520/c-memory-leak-testing-with-crtdumpmemoryleaks-does-not-output-line-numb
#ifdef _DEBUG
#define DEBUG_NEW_PLACEMENT (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DEBUG_NEW_PLACEMENT
#endif

// ref: http://msdn.microsoft.com/en-us/library/windows/desktop/ms683219%28v=vs.85%29.aspx
#define PSAPI_VERSION 1

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#pragma warning(push)
#pragma warning(disable : 4192) /* Ignore warnings for types that are duplicated in win32 header files */
#pragma warning(disable : 4146) /* Ignore warnings for use of minus on unsigned types */
#pragma warning(disable : 4278) /* Ignore warnings for use of duplicate macros */
#pragma warning(disable : 4336) /* Ignore warnings for order of imports */

#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctl.h>
#include <atlhost.h>

using namespace ATL;

// Be sure to set these paths to the version of the software against which you want to run

#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriSystem.olb" named_guids no_namespace raw_interfaces_only no_implementation exclude("OLE_COLOR", "OLE_HANDLE", "VARTYPE", "XMLSerializer")
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriSystemUI.olb" named_guids no_namespace raw_interfaces_only no_implementation rename("ICommand", "ICommandESRI")
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriFramework.olb" named_guids no_namespace raw_interfaces_only no_implementation exclude("UINT_PTR")
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriGeometry.olb" named_guids no_namespace raw_interfaces_only no_implementation rename("ISegment", "ISegmentESRI")
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriGeoDatabase.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids rename("IRow", "IRowESRI")
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriNetworkAnalyst.olb" named_guids no_namespace raw_interfaces_only no_implementation
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriDisplay.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriDataSourcesRaster.olb" raw_interfaces_only, raw_native_types, no_namespace, no_implementation named_guids
#import "C:\Program Files (x86)\ArcGIS\Desktop10.1\com\esriCarto.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids exclude("UINT_PTR") rename("ITableDefinition", "ITableDefinitionESRI")

/*

#import "libid:5E1F7BC3-67C5-4AEE-8EC6-C4B73AAC42ED" named_guids no_namespace raw_interfaces_only no_implementation exclude("OLE_COLOR", "OLE_HANDLE", "VARTYPE", "XMLSerializer") // System
#import "libid:4ECCA6E2-B16B-4ACA-BD17-E74CAE4C150A" named_guids no_namespace raw_interfaces_only no_implementation rename("ICommand", "ICommandESRI") // SystemUI
#import "libid:866AE5D3-530C-11D2-A2BD-0000F8774FB5" named_guids no_namespace raw_interfaces_only no_implementation exclude("UINT_PTR") // Framework
#import "libid:C4B094C2-FF32-4FA1-ABCB-7820F8D6FB68" named_guids no_namespace raw_interfaces_only no_implementation rename("ISegment", "ISegmentESRI") // Geometry
#import "libid:0475BDB1-E5B2-4CA2-9127-B4B1683E70C2" raw_interfaces_only, raw_native_types, no_namespace, named_guids rename("IRow", "IRowESRI") // GeoDatabase
#import "libid:9B4F73F7-90C0-11D5-A6C3-0008C7DF88AB" named_guids no_namespace raw_interfaces_only no_implementation // NetworkAnalyst
#import "libid:59FCCD31-434C-4017-BDEF-DB4B7EDC9CE0" raw_interfaces_only, raw_native_types, no_namespace, named_guids // Display
#import "libid:8F0541A3-D5BE-4B3F-A8D9-062D5579E19B" raw_interfaces_only, raw_native_types, no_namespace, no_implementation named_guids // DataSourcesRaster
#import "libid:45AC68FF-DEFF-4884-B3A9-7D882EDCAEF1" raw_interfaces_only, raw_native_types, no_namespace, named_guids exclude("UINT_PTR") rename("ITableDefinition", "ITableDefinitionESRI") // Carto

*/

// This is included below so we can refer to CLSID_, IID_, etc. defined within this project.

#include "_CustomSolver_i.c"

// other needed libraries
#include "float.h"  // for FLT_MAX, etc.
#include <cmath>   // for HUGE_VAL
#include <algorithm>
#include <ctime>
#include <sstream>
#include <windows.h>
#include <psapi.h>

#pragma warning(pop)
