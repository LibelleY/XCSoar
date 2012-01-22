/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2012 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Asset.hpp"
#include "Util/Macros.hpp"
#include "Profile/Profile.hpp"
#include "UtilsText.hpp"
#include "LogFile.hpp"
#include "UtilsSystem.hpp"
#include "LocalPath.hpp"
#include "IO/FileHandle.hpp"
#include "Sizes.h"

#if defined(WIN32) && (!defined(__GNUC__) || defined(_WIN32_WCE))
#include <windows.h>
#include <winioctl.h>
#endif

#include <windef.h> /* for MAX_PATH */

// Registration Data
TCHAR asset_number[MAX_LOADSTRING] = _T(""); //4G17DW31L0HY");

#ifdef HAVE_MODEL_TYPE
ModelType global_model_type = MODELTYPE_PNA_PNA;
#endif

gcc_const
static bool
IsAlphaNum(TCHAR c)
{
  return (c >= _T('A') && c <= _T('Z')) ||
    (c >= _T('a') && c <= _T('z')) ||
    (c >= _T('0') && c <= _T('9'));
}

static bool
SetAssetNumber(const TCHAR *p)
{
  size_t length = 0;
  while (length < ARRAY_SIZE(asset_number) - 1 && *p != _T('\0')) {
    if (IsAlphaNum(*p))
      asset_number[length++] = *p;
    ++p;
  }

  if (length < 3) {
    asset_number[0] = _T('\0');
    return false;
  }

  return true;
}

static bool
ReadCompaqID(void)
{
#if defined(_WIN32_WCE)
  PROCESS_INFORMATION pi;

  if (CreateProcess(_T("\\windows\\CreateAssetFile.exe"), NULL, NULL, NULL,
                    FALSE, 0, NULL, NULL, NULL, &pi)) {
    WaitForSingleObject(pi.hProcess, 1000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  FileHandle file(_T("\\windows\\cpqAssetData.dat"), _T("rb"));
  if (!file.IsOpen() || !file.Seek(976, SEEK_SET))
    return false;

  TCHAR buffer[ARRAY_SIZE(asset_number)];
  size_t length = file.Read(buffer, ARRAY_SIZE(buffer) - 1, sizeof(buffer[0]));
  buffer[length] = _T('\0');
  return SetAssetNumber(buffer);
#else
  return false;
#endif
}

static bool
ReadUUID(void)
{
#if defined(_WIN32_WCE) && defined(IOCTL_HAL_GET_DEVICEID) && defined(FILE_DEVICE_HAL)
  BOOL fRes;

#define GUIDBuffsize 100
  unsigned char GUIDbuffer[GUIDBuffsize];

  int eLast = 0;
  int i;
  unsigned long uNumReturned=0;
  int iBuffSizeIn=0;
  unsigned long temp, Asset;

  GUID Guid;

  // approach followed: http://blogs.msdn.com/jehance/archive/2004/07/12/181116.aspx
  // 1) send 16 byte buffer - some older devices need this
  // 2) if buffer is wrong size, resize buffer accordingly and retry
  // 3) take first 16 bytes of buffer and process.  Buffer returned may be any size
  // First try exactly 16 bytes, some older PDAs require exactly 16 byte buffer

  asset_number[0]= '\0';

  iBuffSizeIn = sizeof(Guid);
  memset(GUIDbuffer, 0, iBuffSizeIn);
  fRes = KernelIoControl(IOCTL_HAL_GET_DEVICEID, 0, 0, GUIDbuffer,
                         iBuffSizeIn, &uNumReturned);
  if(fRes == false) {
    // try larger buffer
    eLast = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER != eLast)
      return false;
    else {
      // wrong buffer
      iBuffSizeIn = uNumReturned;
      memset(GUIDbuffer, 0, iBuffSizeIn);
      fRes = KernelIoControl(IOCTL_HAL_GET_DEVICEID, 0, 0, GUIDbuffer,
                             iBuffSizeIn, &uNumReturned);
      eLast = GetLastError();

      if(fRes == false)
        return false;
    }
  }

  // here assume we have data in GUIDbuffer of length uNumReturned
  memcpy(&Guid, GUIDbuffer, sizeof(Guid));

  temp = Guid.Data2;
  temp = temp << 16;
  temp += Guid.Data3;

  Asset = temp ^ Guid.Data1;

  temp = 0;
  for(i = 0; i < 4; i++) {
    temp = temp << 8;
    temp += Guid.Data4[i];
  }

  Asset = Asset ^ temp;

  temp = 0;
  for(i = 0; i < 4; i++) {
    temp = temp << 8;
    temp += Guid.Data4[i + 4];
  }

  Asset = Asset ^ temp;

  TCHAR buffer[20];
  _stprintf(asset_number, _T("%08X%08X"), Asset, Guid.Data1);
  return SetAssetNumber(buffer);
#else
  return false;
#endif
}

/**
 * Finds the unique ID of this PDA
 */
void ReadAssetNumber(void)
{
  TCHAR buffer[ARRAY_SIZE(asset_number)];
  if (Profile::Get(szProfileLoggerID, buffer, ARRAY_SIZE(buffer)) &&
      SetAssetNumber(buffer)) {
    LogStartUp(_T("Asset ID: %s (reg)"), asset_number);
  } else if (ReadCompaqID()) {
    LogStartUp(_T("Asset ID: %s (compaq)"), asset_number);
  } else if (ReadUUID()) {
    LogStartUp(_T("Asset ID: %s (uuid)"), asset_number);
  } else {
    _tcscpy(asset_number, _T("AAA"));
    LogStartUp(_T("Asset ID: %s (fallback)"), asset_number);
  }
}

void
InitAsset()
{
#ifdef HAVE_MODEL_TYPE
  Profile::GetEnum(szProfileAppInfoBoxModel, global_model_type);
#endif
}
