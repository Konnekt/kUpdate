#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <commctrl.h>
#include <process.h>
#include <Wininet.h>
#include <stdstring.h>
#include <direct.h>
#include <fstream>
#include <vector>
#include <map>
#include <deque>
#include <sys/types.h>
#include <SYS\STAT.H>
#include <io.h>
#include <math.h>
#include <SYS\UTIME.H>
#pragma comment(lib , "comctl32.lib")

using namespace std;
#include <md5.h>

#include <Stamina\Exception.h>
#include <Stamina\Internet.h>
#include <Stamina\SimXml.h>
#include <Stamina\Helpers.h>


//#include "include\time64.h"
//#include "include\func.h"
//#include "include\simxml.h"
//#include "include\win_registry.h"
//#include "include\richhtml.h"
//#include "include\fontpicker.h"
//#include "include\crc32.h"

#include "resource.h"
#include "resrc1.h"

#include "konnekt/plug_export.h"
#include "konnekt/ui.h"
#include "konnekt/plug_func.h"
#include "konnekt/kUpdate.h"

#include <Stamina/RichHTML.h>
#include <Stamina/Helpers.h>
#include <Stamina/WinHelper.h>
#include <Stamina/MD5.h>
#include <Stamina/FileResource.h>
#include <Stamina/CRC32.h>

using namespace Stamina;

#pragma hdrstop
