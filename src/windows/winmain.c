/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <stddef.h>
#define __CRT_STRSAFE_IMPL
#include <strsafe.h>
#include "resource.h"
#include "feeder.h"
#include "util/port/socket.h"

#include <stdio.h>

#define WM_APP_TRAY (WM_APP + 1)

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam);

static HMENU hTrayMenu = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
	INITCOMMONCONTROLSEX icc;
	WNDCLASSEX wc;
	LPCTSTR MainWndClass = TEXT("OpenSky Feeder");
	HWND hWnd;
	HMENU hSysMenu;
	MSG msg;

	SOCK_init();
	FEEDER_init();

	/* initialise common controls. */
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	/* class for main window. */
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = &MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON),
	    IMAGE_ICON, 0, 0,
	    LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);
	wc.hCursor = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0,
	    LR_SHARED);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);
	wc.lpszClassName = MainWndClass;
	wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON),
	    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
	    GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);

	/* Register Window class */
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, TEXT("Error registering window class."), TEXT("Error"),
		    MB_ICONERROR | MB_OK);
		return 0;
	}

	/* create instance of main window. */
	hWnd = CreateWindowEx(0, MainWndClass, MainWndClass, WS_OVERLAPPEDWINDOW,
	    CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, NULL, NULL, hInstance, NULL);
	if (!hWnd) {
		/* window creation failed. */
		MessageBox(NULL, TEXT("Error creating main window."), TEXT("Error"),
		    MB_ICONERROR | MB_OK);
		return 0;
	}

	/* add "about" to the system menu. */
	hSysMenu = GetSystemMenu(hWnd, FALSE);
	InsertMenu(hSysMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hSysMenu, 6, MF_BYPOSITION, ID_HELP_ABOUT, TEXT("About"));

	/* add tray niData */
	NOTIFYICONDATA niData;
	ZeroMemory(&niData, sizeof niData);
	niData.cbSize = sizeof niData;
	niData.hWnd = hWnd;
	niData.uID = 0;
	niData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	niData.uCallbackMessage = WM_APP_TRAY;
	niData.uVersion = 0;
	niData.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APPICON),
	    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
	    GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);
	StringCchCopy(niData.szTip, ARRAYSIZE(niData.szTip), TEXT("OpenSky Feeder"));
	Shell_NotifyIcon(NIM_ADD, &niData);

	/* load menu for tray */
	HMENU hTrayParentMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
	hTrayMenu = GetSubMenu(hTrayParentMenu, 0);

	puts("Startup");

	// Show window and force a paint.
	//ShowWindow(hWnd, nCmdShow);
	//UpdateWindow(hWnd);

	// Main message loop.
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyMenu(hTrayParentMenu);
	Shell_NotifyIcon(NIM_DELETE, &niData);

	FEEDER_cleanup();
	SOCK_cleanup();

	return (int)msg.wParam;
}

static void showTrayContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hTrayMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y,
		0, hWnd, NULL);
}

enum VISIBILITY {
	VISIBILITY_TOGGLE,
	VISIBILITY_SHOW,
	VISIBILITY_HIDE
};

static void setMainWindowVisibility(HWND hWnd, enum VISIBILITY visibility)
{
	static BOOL visible = FALSE;
	switch (visibility) {
	case VISIBILITY_TOGGLE:
		visible = !visible;
		break;
	case VISIBILITY_SHOW:
		visible = TRUE;
		break;
	case VISIBILITY_HIDE:
		visible = FALSE;
	}
	ShowWindow(hWnd, visible ? SW_RESTORE : SW_HIDE);
}

// Window procedure for our main window.
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HINSTANCE hInstance;

	switch (msg) {
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case ID_HELP_ABOUT: {
			DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTDIALOG), hWnd,
				&AboutDialogProc);
			return 0;
		}

		case ID_FILE_EXIT: {
			DestroyWindow(hWnd);
			return 0;
		}

		case ID_SHOW_MAINWIN: {
			setMainWindowVisibility(hWnd, VISIBILITY_SHOW);
			return 0;
		}

		case ID_FILE_START: {
			FEEDER_configure();
			FEEDER_start();
			return 0;
		}

		case ID_FILE_STOP: {
			FEEDER_stop();
			return 0;
		}
		}
		break;
	}

	case WM_APP_TRAY: {
		switch (LOWORD(lParam)) {
		case WM_LBUTTONDBLCLK:
			setMainWindowVisibility(hWnd, VISIBILITY_TOGGLE);
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			showTrayContextMenu(hWnd);
			break;
		}
		return 0;
	}

	case WM_CLOSE: {
		setMainWindowVisibility(hWnd, VISIBILITY_HIDE);
		return 0;
	}

	case WM_GETMINMAXINFO: {
		MINMAXINFO *minMax = (MINMAXINFO*)lParam;
		minMax->ptMinTrackSize.x = 220;
		minMax->ptMinTrackSize.y = 110;

		return 0;
	}

	case WM_SYSCOMMAND: {
		switch (LOWORD(wParam)) {
		case ID_HELP_ABOUT: {
			DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTDIALOG), hWnd,
				&AboutDialogProc);
			return 0;
		}
		}
		break;
	}

	case WM_CREATE: {
		hInstance = ((LPCREATESTRUCT)lParam)->hInstance;
		return 0;
	}

	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Dialog procedure for our "about" dialog.
static INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
    LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL: {
			EndDialog(hwndDlg, (INT_PTR)LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		}
		break;
	}

	case WM_INITDIALOG:
		return (INT_PTR)TRUE;
	}

	return (INT_PTR)FALSE;
}
