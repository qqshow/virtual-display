/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name: notify.c


Abstract:


Author:

     Eliyas Yakub   Nov 23, 1999

Environment:

    User mode only.

Revision History:

    Modified to use linked list for deviceInfo
    instead of arrays. (5/12/2000)

--*/
#define UNICODE
#define _UNICODE
#define INITGUID

//
// Annotation to indicate to prefast that this is nondriver user-mode code.
//
#include <DriverSpecs.h>
__user_code

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <setupapi.h>
#include <dbt.h>
#include <winioctl.h>
#include <strsafe.h>
#include "public.h"
#include "notify.h"
//#include "ljb_vmon.h"
#include <dontuse.h>

BOOL
HandlePowerBroadcast(
    HWND hWnd,
    WPARAM wParam,
    LPARAM lParam);

//
// Global variables
//
HINSTANCE   hInst;
HWND        hWndList;
TCHAR       szTitle[]=TEXT("Virtual Monitor Test Application");
LIST_ENTRY  ListHead;
HDEVNOTIFY  hInterfaceNotification;
TCHAR       OutText[500];
UINT        ListBoxIndex = 0;
GUID        InterfaceGuid;// = LJB_MONITOR_INTERFACE_GUID;
BOOLEAN     Verbose= FALSE;
PDEVICE_INFO   gDeviceInfo = NULL;

#if (DBG)
#define DBG_PRINT(x)    MyDebugPrint x
#else
#define DBG_PRINT(x)
#endif

VOID
MyDebugPrint(
    __in_z __drv_formatString(printf) LPWSTR format,
    ...
    )
    {
    va_list arg;

    va_start (arg, format);
    (VOID) _vsnwprintf_s(
        OutText,
        sizeof(OutText),
        _TRUNCATE,
        format,
        arg
        );
    va_end (arg);

    OutputDebugString(OutText);
    }

_inline BOOLEAN
IsValid(
    ULONG No
    )
{
    PLIST_ENTRY thisEntry;
    PDEVICE_INFO deviceInfo;

    if(0==(No)) return TRUE; //special case

    for(thisEntry = ListHead.Flink; thisEntry != &ListHead;
                        thisEntry = thisEntry->Flink)
    {
            deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
            if((No) == deviceInfo->SerialNum) {
                return TRUE;
        }
    }
    return FALSE;
}

void makebmp(BYTE* pBits, long width, long height, HDC hdc, HWND hWnd)
{
    int                 iRet;
    RECT                rect;
    BOOL                Status;
    BITMAPINFO          BitmapInfo;
    BITMAPINFO * CONST  pBMI = &BitmapInfo;

    memset(pBMI, 0, sizeof(BITMAPINFO));
    pBMI->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    pBMI->bmiHeader.biWidth = width;
    pBMI->bmiHeader.biHeight = -height;  // negative means top down!!!!
    pBMI->bmiHeader.biPlanes = 1;
    pBMI->bmiHeader.biBitCount = 32;
    pBMI->bmiHeader.biCompression = BI_RGB;//BI_BITFIELDS;//BI_RGB;
    pBMI->bmiHeader.biSizeImage = width*height*4;
    pBMI->bmiHeader.biXPelsPerMeter = 0;
    pBMI->bmiHeader.biYPelsPerMeter = 0;
    pBMI->bmiHeader.biClrUsed = 0;
    pBMI->bmiHeader.biClrImportant = 0;

    //memcpy(pBMI+ sizeof(BITMAPINFOHEADER), bmiColors, sizeof(RGBQUAD)*3);
    Status = GetWindowRect(
        hWnd,
        &rect
        );

    if (Status == 0)
    {
        DBG_PRINT((TEXT("?"__FUNCTION__":Failed to get rect?\n")));
    }
    else
    {
        iRet = StretchDIBits(
            hdc,
            0,//rect.left,//0,
            0,//rect.top,//0,
            width,//rect.right - rect.left,//width,
            height,//rect.bottom - rect.top,//height,
            0,
            0,
            width,
            height,
            pBits,
            pBMI,
            DIB_RGB_COLORS,//DIB_PAL_COLORS,
            SRCCOPY
            );
    }
}

int PASCAL
WinMain (
    __in HINSTANCE hInstance,
    __in_opt HINSTANCE hPrevInstance,
    __in_opt LPSTR lpCmdLine,
    __in int nShowCmd
    )
{
    static    TCHAR szAppName[]=TEXT("LJB_VMON Notify");
    HWND      hWnd;
    MSG       msg;
    WNDCLASS  wndclass;

    UNREFERENCED_PARAMETER( lpCmdLine );

    InterfaceGuid = LJB_MONITOR_INTERFACE_GUID;
    hInst=hInstance;

    if (!hPrevInstance)
    {
         wndclass.style        =  CS_HREDRAW | CS_VREDRAW;
         wndclass.lpfnWndProc  =  WndProc;
         wndclass.cbClsExtra   =  0;
         wndclass.cbWndExtra   =  0;
         wndclass.hInstance    =  hInstance;
         wndclass.hIcon        =  LoadIcon (NULL, IDI_APPLICATION);
         wndclass.hCursor      =  LoadCursor(NULL, IDC_ARROW);
         wndclass.hbrBackground=  GetStockObject(WHITE_BRUSH);
         wndclass.lpszMenuName =  TEXT("GenericMenu");
         wndclass.lpszClassName=  szAppName;

         RegisterClass(&wndclass);
    }

    hWnd = CreateWindow(
        szAppName,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL);

    ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);

    while (GetMessage (&msg, NULL, 0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (0);
}


LRESULT
FAR PASCAL
WndProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    DWORD nEventType = (DWORD)wParam;
    PDEV_BROADCAST_HDR p = (PDEV_BROADCAST_HDR) lParam;
    DEV_BROADCAST_DEVICEINTERFACE filter;
    // For WM_PAINT
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_PAINT:
        DBG_PRINT((TEXT(" Received WM_PAINT message.\n")));
        if (gDeviceInfo != NULL && gDeviceInfo->BitmapBuffer != NULL &&
            wParam == LPARAM_NOTIFY_FRAME_UPDATE)
        {
            //hdc = BeginPaint(hWndList, &ps);
            hdc = GetDC(hWndList);
            makebmp(
                gDeviceInfo->BitmapBuffer,  // pBits,
                gDeviceInfo->Width,         //width,
                gDeviceInfo->Height,        //height,
                hdc,
                hWndList
                );
            ReleaseDC(
                hWndList,
                hdc
                );
            //EndPaint(hWndList, &ps);
        }
        return DefWindowProc(hWnd,message, wParam, lParam);

    case WM_COMMAND:
            HandleCommands(hWnd, message, wParam, lParam);
            return 0;

    case WM_CREATE:
            //
            // Load and set the icon of the program
            //
            SetClassLongPtr(hWnd, GCLP_HICON,
                (LONG_PTR)LoadIcon((HINSTANCE)lParam,MAKEINTRESOURCE(IDI_CLASS_ICON)));

            hWndList = CreateWindow (TEXT("listbox"),
                         NULL,
                         WS_CHILD|WS_VISIBLE|LBS_NOTIFY |
                         WS_VSCROLL | WS_BORDER,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         hWnd,
                         (HMENU)ID_EDIT,
                         hInst,
                         NULL);

            filter.dbcc_size = sizeof(filter);
            filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            filter.dbcc_classguid = InterfaceGuid;
            hInterfaceNotification = RegisterDeviceNotification(hWnd, &filter, 0);

            InitializeListHead(&ListHead);
            EnumExistingDevices(hWnd);
            return 0;

    case WM_SIZE:
        MoveWindow(hWndList, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;

    case WM_SETFOCUS:
        SetFocus(hWndList);
        return 0;

    case WM_DEVICECHANGE:
        //
        // The DBT_DEVNODES_CHANGED broadcast message is sent
        // everytime a device is added or removed. This message
        // is typically handled by Device Manager kind of apps,
        // which uses it to refresh window whenever something changes.
        // The lParam is always NULL in this case.
        //
        if(DBT_DEVNODES_CHANGED == wParam) {
            DBG_PRINT((TEXT("Received DBT_DEVNODES_CHANGED broadcast message")));
            return 0;
        }

        //
        // All the events we're interested in come with lParam pointing to
        // a structure headed by a DEV_BROADCAST_HDR.  This is denoted by
        // bit 15 of wParam being set, and bit 14 being clear.
        //
        if((wParam & 0xC000) == 0x8000)
        {
            if (!p)
                return 0;

            if (p->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                HandleDeviceInterfaceChange(hWnd, nEventType, (PDEV_BROADCAST_DEVICEINTERFACE) p);
            }
            else
            if (p->dbch_devicetype == DBT_DEVTYP_HANDLE)
            {
                HandleDeviceChange(hWnd, nEventType, (PDEV_BROADCAST_HANDLE) p);
            }
        }
        return 0;

    case WM_POWERBROADCAST:
        HandlePowerBroadcast(hWnd, wParam, lParam);
        return 0;

    case WM_CLOSE:
        Cleanup(hWnd);
        UnregisterDeviceNotification(hInterfaceNotification);
        return  DefWindowProc(hWnd,message, wParam, lParam);

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd,message, wParam, lParam);
}


LRESULT
HandleCommands(
    HWND     hWnd,
    UINT     uMsg,
    WPARAM   wParam,
    LPARAM   lParam
    )
{
    PDIALOG_RESULT result = NULL;

    UNREFERENCED_PARAMETER( uMsg );
    UNREFERENCED_PARAMETER( lParam );

    switch (wParam) {
    case IDM_OPEN:
        Cleanup(hWnd); // close all open handles
        EnumExistingDevices(hWnd);
        break;

    case IDM_CLOSE:
        Cleanup(hWnd);
        break;

    case IDM_HIDE:
        result = (PDIALOG_RESULT)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
        if(result && result->SerialNum && IsValid(result->SerialNum))
        {
            DWORD bytes;
            PDEVICE_INFO deviceInfo = NULL;
            PLIST_ENTRY thisEntry;

            //
            // Find out the deviceInfo that matches this SerialNum.
            // We need the deviceInfo to get the handle to the device.
            //
            for(thisEntry = ListHead.Flink; thisEntry != &ListHead;
                thisEntry = thisEntry->Flink)
            {
                deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
                if(result->SerialNum == deviceInfo->SerialNum) {
                    break;
                }
                deviceInfo = NULL;
            }

            //
            // If found send I/O control
            //

            if (deviceInfo && !DeviceIoControl (deviceInfo->hDevice,
                      IOCTL_TOASTER_DONT_DISPLAY_IN_UI_DEVICE,
                      NULL, 0,
                      NULL, 0,
                      &bytes, NULL)) {
                   MessageBox(hWnd, TEXT("Request Failed or Invalid Serial No"),
                               TEXT("Error"), MB_OK);
            }
        }
        break;

    case IDM_PLUGIN:
        result = (PDIALOG_RESULT)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG), hWnd, DlgProc);
        if(result)
        {
            if(!result->SerialNum || !OpenBusInterface(result->SerialNum, result->DeviceId, PLUGIN))
            {
               MessageBox(hWnd, TEXT("Invalid Serial Number or OpenBusInterface Failed"), TEXT("Error"), MB_OK);
            }
        }
        break;

    case IDM_UNPLUG:
        result = (PDIALOG_RESULT)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
        if(result && IsValid(result->SerialNum))
        {
            if(!OpenBusInterface(result->SerialNum, NULL, UNPLUG))
            {
                MessageBox(
                    hWnd,
                    TEXT("Invalid Serial Number or OpenBusInterface Failed"),
                    TEXT("Error"),
                    MB_OK
                    );
            }
        }
        break;

    case IDM_EJECT:
        result = (PDIALOG_RESULT)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
        if(result && IsValid(result->SerialNum))
        {
            if(!OpenBusInterface(result->SerialNum, NULL, EJECT))
            {
                MessageBox(
                    hWnd,
                    TEXT("Invalid Serial Number or OpenBusInterface  Failed"),
                    TEXT("Error"),
                    MB_OK
                    );
            }
        }
        break;

    case IDM_CLEAR:
        SendMessage(hWndList, LB_RESETCONTENT, 0, 0);
        ListBoxIndex = 0;
        break;

    case IDM_IOCTL:
        SendIoctlToFilterDevice();
        break;

    case IDM_VERBOSE:
        {
            HMENU hMenu = GetMenu(hWnd);

            Verbose = !Verbose;
            if(Verbose)
            {
                CheckMenuItem(hMenu, (UINT)wParam, MF_CHECKED);
            } else
            {
                CheckMenuItem(hMenu, (UINT)wParam, MF_UNCHECKED);
            }
        }
        break;

    case IDM_EXIT:
        PostQuitMessage(0);
        break;

    default:
        break;
    }

    if(result)
    {
        HeapFree(GetProcessHeap(), 0, result);
    }

    return TRUE;
}

INT_PTR CALLBACK
DlgProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    BOOL            success;
    PDIALOG_RESULT  dialogResult = NULL;

    UNREFERENCED_PARAMETER( lParam );

    switch(message)
    {
    case WM_INITDIALOG:
        SetDlgItemText(hDlg, IDC_DEVICEID, BUS_HARDWARE_IDS);
        return TRUE;

    case WM_COMMAND:
        switch( wParam)
        {
        case ID_OK:
            dialogResult = HeapAlloc(GetProcessHeap(),
                                    HEAP_ZERO_MEMORY,
                                    (sizeof(DIALOG_RESULT) + MAX_PATH * sizeof(WCHAR)));
            if(dialogResult)
            {
                dialogResult->DeviceId = (PWCHAR)((PCHAR)dialogResult + sizeof(DIALOG_RESULT));
                dialogResult->SerialNum = GetDlgItemInt(hDlg,IDC_SERIALNO, &success, FALSE );
                GetDlgItemText(hDlg, IDC_DEVICEID, dialogResult->DeviceId, MAX_PATH-1 );
            }
            EndDialog(hDlg, (UINT_PTR)dialogResult);
            return TRUE;

        case ID_CANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

BOOL
HandleDeviceInterfaceChange(
    HWND hWnd,
    DWORD evtype,
    PDEV_BROADCAST_DEVICEINTERFACE dip
    )
{
    DEV_BROADCAST_HANDLE    filter;
    PDEVICE_INFO            deviceInfo = NULL;
    HRESULT                 hr;

    switch (evtype)
    {
    case DBT_DEVICEARRIVAL:
        //
        // New device arrived. Open handle to the device
        // and register notification of type DBT_DEVTYP_HANDLE
        //

        deviceInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DEVICE_INFO));
        if(!deviceInfo)
            return FALSE;

        InitializeListHead(&deviceInfo->ListEntry);
        InsertTailList(&ListHead, &deviceInfo->ListEntry);


        if(!GetDeviceDescription(dip->dbcc_name,
                                 deviceInfo->DeviceName,
                                 sizeof(deviceInfo->DeviceName),
                                 &deviceInfo->SerialNum)) {
            MessageBox(hWnd, TEXT("GetDeviceDescription failed"), TEXT("Error!"), MB_OK);
        }

        DBG_PRINT((
            TEXT("New device Arrived (Interface Change Notification): %ws"),
            deviceInfo->DeviceName));

        hr = StringCchCopy(deviceInfo->DevicePath, MAX_PATH, dip->dbcc_name);
        if(FAILED(hr)){
            // DeviceInfo will be freed later by the cleanup routine.
            break;
        }

        deviceInfo->hDevice = CreateFile(
            dip->dbcc_name,
            GENERIC_READ |GENERIC_WRITE, 0, NULL,
            OPEN_EXISTING, 0, NULL);
        if(deviceInfo->hDevice == INVALID_HANDLE_VALUE)
        {
            DBG_PRINT((TEXT("Failed to open the device: %ws"),
                deviceInfo->DeviceName));
            break;
        }

        DBG_PRINT((TEXT("Opened handled to the device: %ws"),
            deviceInfo->DeviceName));
        memset (&filter, 0, sizeof(filter)); //zero the structure
        filter.dbch_size = sizeof(filter);
        filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
        filter.dbch_handle = deviceInfo->hDevice;

        deviceInfo->hHandleNotification =
                            RegisterDeviceNotification(hWnd, &filter, 0);

        /*
         * create VMON main thread.
         */
        deviceInfo->hWndList = hWndList;
        deviceInfo->hParentWnd = hWnd;
        gDeviceInfo = deviceInfo;
        deviceInfo->VMONThread = CreateThread(
            NULL,
            0,      /* use default statck size */
            &LJB_VMON_Main,
            deviceInfo,
            0,       /* the thread runs after completion */
            &deviceInfo->VMONThreadId
            );

        break;

    case DBT_DEVICEREMOVECOMPLETE:
        DBG_PRINT((TEXT("Remove Complete (Interface Change Notification)")));
        break;

        //
        // Device Removed.
        //

    default:
        DBG_PRINT((TEXT("Unknown (Interface Change Notification)")));
        break;
    }
    return TRUE;
}

BOOL
HandleDeviceChange(
    HWND hWnd,
    DWORD evtype,
    PDEV_BROADCAST_HANDLE dhp
    )
{
    DEV_BROADCAST_HANDLE    filter;
    PDEVICE_INFO            deviceInfo = NULL;
    PLIST_ENTRY             thisEntry;

    //
    // Walk the list to get the deviceInfo for this device
    // by matching the handle given in the notification.
    //
    for(thisEntry = ListHead.Flink; thisEntry != &ListHead;
                        thisEntry = thisEntry->Flink)
    {
        deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
        if(dhp->dbch_hdevnotify == deviceInfo->hHandleNotification) {
            break;
        }
        deviceInfo = NULL;
    }

    if(!deviceInfo)
    {
        DBG_PRINT((
            TEXT("Error: spurious message, Event Type %x, Device Type %x"),
            evtype, dhp->dbch_devicetype));
        return FALSE;
    }

    switch (evtype)
    {
    case DBT_DEVICEQUERYREMOVE:
        DBG_PRINT((TEXT("Query Remove (Handle Notification)"),
            deviceInfo->DeviceName));

        // User is trying to disable, uninstall, or eject our device.
        // Close the handle to the device so that the target device can
        // get removed. Do not unregister the notification
        // at this point, because we want to know whether
        // the device is successfully removed or not.
        //
        if (deviceInfo->hDevice != INVALID_HANDLE_VALUE)
        {
            CloseHandle(deviceInfo->hDevice);
            deviceInfo->hDevice = INVALID_HANDLE_VALUE;
            DBG_PRINT((TEXT("Closed handle to device %ws"),
                deviceInfo->DeviceName));
        }
        break;

    case DBT_DEVICEREMOVECOMPLETE:

        DBG_PRINT((TEXT("Remove Complete (Handle Notification):%ws"),
            deviceInfo->DeviceName));
        //
        // Device is getting surprise removed. So close
        // the handle to device and unregister the PNP notification.
        //
        if (deviceInfo->hHandleNotification)
        {
            UnregisterDeviceNotification(deviceInfo->hHandleNotification);
            deviceInfo->hHandleNotification = NULL;
        }
        if (deviceInfo->hDevice != INVALID_HANDLE_VALUE)
        {
            CloseHandle(deviceInfo->hDevice);
            deviceInfo->hDevice = INVALID_HANDLE_VALUE;
            DBG_PRINT((TEXT("Closed handle to device %ws"),
                deviceInfo->DeviceName));
        }

        // Clean up
        if (deviceInfo->dev_ctx != NULL)
        {
            DBG_PRINT((TEXT(" Set exit_vmon_thread to TRUE.\n")));
            deviceInfo->dev_ctx->exit_vmon_thread = TRUE;
        }

        //
        // Unlink this deviceInfo from the list and free the memory
        //
        RemoveEntryList(&deviceInfo->ListEntry);
        HeapFree (GetProcessHeap(), 0, deviceInfo);
        break;

    case DBT_DEVICEREMOVEPENDING:
        DBG_PRINT((TEXT("Remove Pending (Handle Notification):%ws"),
            deviceInfo->DeviceName));
        //
        // Device is successfully removed so unregister the notification
        // and free the memory.
        //
        if (deviceInfo->hHandleNotification)
        {
            UnregisterDeviceNotification(deviceInfo->hHandleNotification);
            deviceInfo->hHandleNotification = NULL;
            deviceInfo->hDevice = INVALID_HANDLE_VALUE;
        }

        //
        // Unlink this deviceInfo from the list and free the memory
        //
        RemoveEntryList(&deviceInfo->ListEntry);
        HeapFree (GetProcessHeap(), 0, deviceInfo);
        break;

    case DBT_DEVICEQUERYREMOVEFAILED :
        DBG_PRINT((
            TEXT("Remove failed (Handle Notification):%ws"),
            deviceInfo->DeviceName));
        //
        // Remove failed. So reopen the device and register for
        // notification on the new handle. But first we should unregister
        // the previous notification.
        //
        if (deviceInfo->hHandleNotification)
        {
            UnregisterDeviceNotification(deviceInfo->hHandleNotification);
            deviceInfo->hHandleNotification = NULL;
        }
        deviceInfo->hDevice = CreateFile(
            deviceInfo->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL);
        if(deviceInfo->hDevice == INVALID_HANDLE_VALUE) {
            DBG_PRINT((
                TEXT("Failed to reopen the device: %ws"),
                deviceInfo->DeviceName));
            HeapFree (GetProcessHeap(), 0, deviceInfo);
            break;
        }

        //
        // Register handle based notification to receive pnp
        // device change notification on the handle.
        //
        memset (&filter, 0, sizeof(filter)); //zero the structure
        filter.dbch_size = sizeof(filter);
        filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
        filter.dbch_handle = deviceInfo->hDevice;

        deviceInfo->hHandleNotification =
                            RegisterDeviceNotification(hWnd, &filter, 0);
        DBG_PRINT((TEXT("Reopened device %ws"), deviceInfo->DeviceName));
        break;

    default:
        DBG_PRINT((TEXT("Unknown (Handle Notification)"),
            deviceInfo->DeviceName));
        break;

    }
    return TRUE;
}


BOOLEAN
EnumExistingDevices(
    HWND   hWnd
)
{
    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;
    DWORD                              error;
    DEV_BROADCAST_HANDLE                filter;
    PDEVICE_INFO                        deviceInfo =NULL;
    UINT                                i=0;
    HRESULT                             hr;

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&InterfaceGuid,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_DEVICEINTERFACE)); // Function class devices.
    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        goto Error;
    }

    //
    // Enumerate devices of toaster class
    //
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

    for(i=0;
        SetupDiEnumDeviceInterfaces(
            hardwareDeviceInfo,
            0, // No care about specific PDOs
            (LPGUID)&InterfaceGuid,
            i, //
            &deviceInterfaceData);
        i++)
    {

        //
        // Allocate a function class device data structure to
        // receive the information about this particular device.
        //

        //
        // First find out required length of the buffer
        //
        if(deviceInterfaceDetailData)
                HeapFree (GetProcessHeap(), 0, deviceInterfaceDetailData);

        if(!SetupDiGetDeviceInterfaceDetail(
                hardwareDeviceInfo,
                &deviceInterfaceData,
                NULL, // probing so no output buffer yet
                0, // probing so output buffer length of zero
                &requiredLength,
                NULL) && (error = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
        {
            goto Error;
        }
        predictedLength = requiredLength;

        deviceInterfaceDetailData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                               predictedLength);
        if (deviceInterfaceDetailData == NULL)
        {
            goto Error;
        }
        deviceInterfaceDetailData->cbSize =
                        sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);


        if (! SetupDiGetDeviceInterfaceDetail (
                   hardwareDeviceInfo,
                   &deviceInterfaceData,
                   deviceInterfaceDetailData,
                   predictedLength,
                   &requiredLength,
                   NULL))
        {
            goto Error;
        }

        deviceInfo = HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            sizeof(DEVICE_INFO));
        if (deviceInfo == NULL)
        {
            goto Error;
        }

        InitializeListHead(&deviceInfo->ListEntry);
        InsertTailList(&ListHead, &deviceInfo->ListEntry);

        //
        // Get the device details such as friendly name and SerialNum
        //
        if(!GetDeviceDescription(deviceInterfaceDetailData->DevicePath,
                                 deviceInfo->DeviceName,
                                 sizeof(deviceInfo->DeviceName),
                                 &deviceInfo->SerialNum))
        {
            goto Error;
        }

        DBG_PRINT((TEXT("Found device %ws"), deviceInfo->DeviceName));

        hr = StringCchCopy(deviceInfo->DevicePath, MAX_PATH, deviceInterfaceDetailData->DevicePath);
        if(FAILED(hr)){
            goto Error;
        }
        //
        // Open an handle to the device.
        //
        deviceInfo->hDevice = CreateFile (
                deviceInterfaceDetailData->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL, // no SECURITY_ATTRIBUTES structure
                OPEN_EXISTING, // No special create flags
                0, // No special attributes
                NULL);

        if (INVALID_HANDLE_VALUE == deviceInfo->hDevice) {
            DBG_PRINT((
                TEXT("Failed to open the device: %ws"),
                deviceInfo->DeviceName));
            continue;
        }

        DBG_PRINT((
            TEXT("Opened handled to the device: %ws"),
            deviceInfo->DeviceName));
        //
        // Register handle based notification to receive pnp
        // device change notification on the handle.
        //

        memset (&filter, 0, sizeof(filter)); //zero the structure
        filter.dbch_size = sizeof(filter);
        filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
        filter.dbch_handle = deviceInfo->hDevice;

        deviceInfo->hHandleNotification = RegisterDeviceNotification(hWnd, &filter, 0);

    }

    if(deviceInterfaceDetailData)
        HeapFree (GetProcessHeap(), 0, deviceInterfaceDetailData);

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return 0;

Error:

    MessageBox(hWnd, TEXT("EnumExisting Devices failed"), TEXT("Error!"), MB_OK);
    if(deviceInterfaceDetailData)
        HeapFree (GetProcessHeap(), 0, deviceInterfaceDetailData);

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    Cleanup(hWnd);
    return 0;
}

BOOLEAN Cleanup(HWND hWnd)
{
    PDEVICE_INFO    deviceInfo = NULL;
    PLIST_ENTRY     thisEntry;

    UNREFERENCED_PARAMETER(hWnd);

    while (!IsListEmpty(&ListHead))
    {
        thisEntry = RemoveHeadList(&ListHead);
        deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
        if (deviceInfo->hHandleNotification)
        {
            UnregisterDeviceNotification(deviceInfo->hHandleNotification);
            deviceInfo->hHandleNotification = NULL;
        }

        if (deviceInfo->hDevice != INVALID_HANDLE_VALUE &&
                deviceInfo->hDevice != NULL)
        {
            CloseHandle(deviceInfo->hDevice);
            deviceInfo->hDevice = INVALID_HANDLE_VALUE;
            DBG_PRINT((TEXT("Closed handle to device %ws"),
                deviceInfo->DeviceName));
        }
        HeapFree(GetProcessHeap(), 0, deviceInfo);
    }
    return TRUE;
}

BOOL
GetDeviceDescription(
    __in LPTSTR DevPath,
    __out_bcount_full(OutBufferLen) LPTSTR OutBuffer,
    __in ULONG OutBufferLen,
    __in PULONG SerialNum
)
{
    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    SP_DEVINFO_DATA                     deviceInfoData;
    DWORD                               dwRegType, error;

    hardwareDeviceInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        goto Error;
    }

    //
    // Enumerate devices of toaster class
    //
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

    SetupDiOpenDeviceInterface (hardwareDeviceInfo, DevPath,
                                 0, //
                                 &deviceInterfaceData);

    deviceInfoData.cbSize = sizeof(deviceInfoData);
    if(!SetupDiGetDeviceInterfaceDetail (
            hardwareDeviceInfo,
            &deviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            NULL,
            &deviceInfoData) && (error = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
    {
        goto Error;
    }
    //
    // Get the friendly name for this instance, if that fails
    // try to get the device description.
    //

    if(!SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo, &deviceInfoData,
                                     SPDRP_FRIENDLYNAME,
                                     &dwRegType,
                                     (BYTE*) OutBuffer,
                                     OutBufferLen,
                                     NULL))
    {
        if(!SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo, &deviceInfoData,
                                     SPDRP_DEVICEDESC,
                                     &dwRegType,
                                     (BYTE*) OutBuffer,
                                     OutBufferLen,
                                     NULL)){
            goto Error;

        }
    }

    //
    // Get the serial number of the device. The bus driver reports
    // the device serial number as UINumber in the devcaps.
    //
    if(!SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo,
                 &deviceInfoData,
                 SPDRP_UI_NUMBER,
                 &dwRegType,
                 (BYTE*) SerialNum,
                 sizeof(PULONG),
                 NULL))
    {
        DBG_PRINT((TEXT("SerialNum is not available for device: %ws"),
            OutBuffer));
    }


    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return TRUE;

Error:

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return FALSE;
}



BOOLEAN
OpenBusInterface (
    __in ULONG SerialNum,
    __in_opt LPWSTR DeviceId,
    __in USER_ACTION_TYPE Action
    )
{
    HANDLE                              hDevice=INVALID_HANDLE_VALUE;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;
    ULONG                               bytes;
    BUSENUM_UNPLUG_HARDWARE             unplug;
    BUSENUM_EJECT_HARDWARE              eject;
    PBUSENUM_PLUGIN_HARDWARE            hardware;
    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    BOOLEAN                             status = FALSE;
    HRESULT                             hr;
    //
    // Open a handle to the device interface information set of all
    // present toaster bus enumerator interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&GUID_DEVINTERFACE_VMON_BUS,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_DEVICEINTERFACE)); // Function class devices.

    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        return FALSE;
    }

    deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    if (!SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                 0, // No care about specific PDOs
                                 (LPGUID)&GUID_DEVINTERFACE_VMON_BUS,
                                 0, //
                                 &deviceInterfaceData)) {
        goto Clean0;
    }

    //
    // Allocate a function class device data structure to receive the
    // information about this particular device.
    //

    SetupDiGetDeviceInterfaceDetail (
            hardwareDeviceInfo,
            &deviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL);//not interested in the specific dev-node

    if(ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
        goto Clean0;
     }


     predictedLength = requiredLength;

    deviceInterfaceDetailData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                    predictedLength);

    if(deviceInterfaceDetailData) {
        deviceInterfaceDetailData->cbSize =
                        sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
    } else {
        goto Clean0;
    }


    if (! SetupDiGetDeviceInterfaceDetail (
               hardwareDeviceInfo,
               &deviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL)) {
        goto Clean1;
    }


    hDevice = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ, // Only read access
                        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL); // No template file

    if (INVALID_HANDLE_VALUE == hDevice) {
        goto Clean1;
    }

    //
    // Enumerate Devices
    //

    if(Action == PLUGIN) {
        int length =  (wcslen(DeviceId)+2)*sizeof(WCHAR); //in bytes

        bytes = sizeof (BUSENUM_PLUGIN_HARDWARE) + length;
        hardware = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);

        if(hardware) {
            memset(hardware, 0, bytes);
            hardware->Size = sizeof (BUSENUM_PLUGIN_HARDWARE);
            hardware->SerialNo = SerialNum;
        } else {
            goto Clean2;
        }

        //
        // copy the Device ID
        //
        hr = StringCchCopy(hardware->HardwareIDs, length/sizeof(WCHAR), DeviceId);
        if (SUCCEEDED(hr) && DeviceIoControl (hDevice,
                              IOCTL_BUSENUM_PLUGIN_HARDWARE ,
                              hardware, bytes,
                              NULL, 0,
                              &bytes, NULL)) {
              status = TRUE;
         }

         HeapFree (GetProcessHeap(), 0, hardware);

    }

    //
    // Removes a device if given the specific Id of the device. Otherwise this
    // ioctls removes all the devices that are enumerated so far.
    //

    if(Action == UNPLUG) {

        unplug.Size = bytes = sizeof (unplug);
        unplug.SerialNo = SerialNum;
        if (DeviceIoControl (hDevice,
                              IOCTL_BUSENUM_UNPLUG_HARDWARE,
                              &unplug, bytes,
                              NULL, 0,
                              &bytes, NULL)) {
            status = TRUE;
        }
    }

    //
    // Ejects a device if given the specific Id of the device. Otherwise this
    // ioctls ejects all the devices that are enumerated so far.
    //

    if(Action == EJECT)
    {

        eject.Size = bytes = sizeof (eject);
        eject.SerialNo = SerialNum;
        if (DeviceIoControl (hDevice,
                              IOCTL_BUSENUM_EJECT_HARDWARE,
                              &eject, bytes,
                              NULL, 0,
                              &bytes, NULL)) {
            status = TRUE;
        }
    }

Clean2:
    CloseHandle(hDevice);
Clean1:
    HeapFree (GetProcessHeap(), 0, deviceInterfaceDetailData);
Clean0:
    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return status;
}

BOOL
HandlePowerBroadcast(
    HWND hWnd,
    WPARAM wParam,
    LPARAM lParam)
{
    BOOL fRet = TRUE;

    UNREFERENCED_PARAMETER( hWnd );
    UNREFERENCED_PARAMETER( lParam );

    switch (wParam)
    {
        case PBT_APMQUERYSTANDBY:
            DBG_PRINT((TEXT("PBT_APMQUERYSTANDBY")));
            break;
        case PBT_APMQUERYSUSPEND:
            DBG_PRINT((TEXT("PBT_APMQUERYSUSPEND")));
            break;
        case PBT_APMSTANDBY :
            DBG_PRINT((TEXT("PBT_APMSTANDBY")));
            break;
        case PBT_APMSUSPEND :
            DBG_PRINT((TEXT("PBT_APMSUSPEND")));
            break;
        case PBT_APMQUERYSTANDBYFAILED:
            DBG_PRINT((TEXT("PBT_APMQUERYSTANDBYFAILED")));
            break;
        case PBT_APMRESUMESTANDBY:
            DBG_PRINT((TEXT("PBT_APMRESUMESTANDBY")));
            break;
        case PBT_APMQUERYSUSPENDFAILED:
            DBG_PRINT((TEXT("PBT_APMQUERYSUSPENDFAILED")));
            break;
        case PBT_APMRESUMESUSPEND:
            DBG_PRINT((TEXT("PBT_APMRESUMESUSPEND")));
            break;
        case PBT_APMBATTERYLOW:
            DBG_PRINT((TEXT("PBT_APMBATTERYLOW")));
            break;
        case PBT_APMOEMEVENT:
            DBG_PRINT((TEXT("PBT_APMOEMEVENT")));
            break;
        case PBT_APMRESUMEAUTOMATIC:
            DBG_PRINT((TEXT("PBT_APMRESUMEAUTOMATIC")));
            break;
        case PBT_APMRESUMECRITICAL:
            DBG_PRINT((TEXT("PBT_APMRESUMECRITICAL")));
            break;
        case PBT_APMPOWERSTATUSCHANGE:
            DBG_PRINT((TEXT("PBT_APMPOWERSTATUSCHANGE")));
            break;
        default:
            DBG_PRINT((TEXT("Default")));
            break;
    }
    return fRet;
}

void
SendIoctlToFilterDevice()
{
#define IOCTL_CUSTOM_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0, METHOD_BUFFERED, FILE_READ_DATA)

    HANDLE hControlDevice;
    ULONG  bytes;

    //
    // Open handle to the control device. Please note that even
    // a non-admin user can open handle to the device with
    // FILE_READ_ATTRIBUTES | SYNCHRONIZE DesiredAccess and send IOCTLs if the
    // IOCTL is defined with FILE_ANY_ACCESS. So for better security avoid
    // specifying FILE_ANY_ACCESS in your IOCTL defintions.
    // If the IOCTL is defined to have FILE_READ_DATA access rights, you can
    // open the device with GENERIC_READ and call DeviceIoControl.
    // If the IOCTL is defined to have FILE_WRITE_DATA access rights, you can
    // open the device with GENERIC_WRITE and call DeviceIoControl.
    //
    hControlDevice = CreateFile ( TEXT("\\\\.\\ToasterFilter"),
                        GENERIC_READ, // Only read access
                        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL); // No template file

    if (INVALID_HANDLE_VALUE == hControlDevice)
    {
        DBG_PRINT((TEXT("Failed to open ToasterFilter device")));
    }
    else
    {
        if (!DeviceIoControl (hControlDevice,
                              IOCTL_CUSTOM_CODE,
                              NULL, 0,
                              NULL, 0,
                              &bytes, NULL)) {
            DBG_PRINT((TEXT("Ioctl to ToasterFilter device failed\n")));
        } else {
            DBG_PRINT((TEXT("Ioctl to ToasterFilter device succeeded\n")));
        }
        CloseHandle(hControlDevice);
    }
    return;
}
