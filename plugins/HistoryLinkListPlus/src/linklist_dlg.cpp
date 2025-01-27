// History Linklist Plus
// Copyright (C) 2010 Thomas Wendel, gureedo
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#include "stdafx.h"

extern MWindowList hWindowList;
extern HCURSOR splitCursor;

MYCOLOURSET colourSet;
LISTOPTIONS options;
/*
MainDlgProc handles messages to the main dialog box
*/
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DIALOGPARAM *DlgParam = (DIALOGPARAM*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	HMENU listMenu;
	wchar_t filter[FILTERTEXT];
	POINT pt;

	switch (msg) {
	case WM_INITDIALOG:
		MCONTACT hContact;
		wchar_t title[256];
		RECT rc;

		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		DlgParam = (DIALOGPARAM*)lParam;
		listMenu = GetMenu(hDlg);
		TranslateDialogDefault(hDlg);
		TranslateMenu(listMenu);

		hContact = ((g_plugin.getByte(LINKLIST_SAVESPECIAL) == 0) ? NULL : DlgParam->hContact);
		if (g_plugin.getByte(hContact, LINKLIST_FIRST, 0) == 0) {
			// First use of this plugin! Set default size!
			g_plugin.setDword(hContact, "LinklistWidth", 400);
			g_plugin.setDword(hContact, "LinklistHeight", 450);
			g_plugin.setDword(hContact, "LinklistX", 0);
			g_plugin.setDword(hContact, "LinklistY", 0);

			g_plugin.setByte(hContact, LINKLIST_FIRST, 1);
		}

		DlgParam->splitterPosNew = g_plugin.getDword(hContact, LINKLIST_SPLITPOS, -1);

		GetWindowRect(GetDlgItem(hDlg, IDC_MAIN), &rc);
		DlgParam->minSize.cx = rc.right - rc.left;
		DlgParam->minSize.cy = rc.bottom - rc.top;

		GetWindowRect(GetDlgItem(hDlg, IDC_SPLITTER), &rc);
		pt.y = (rc.top + rc.bottom) / 2;
		pt.x = 0;
		ScreenToClient(hDlg, &pt);

		DlgParam->splitterPosOld = rc.bottom - 20 - pt.y;
		if (DlgParam->splitterPosNew == -1)
			DlgParam->splitterPosNew = DlgParam->splitterPosOld;

		Utils_RestoreWindowPosition(hDlg, hContact, LINKLIST_MODULE, "Linklist");

		SetClassLongPtr(hDlg, GCLP_HICON, (LONG_PTR)LoadIcon(g_plugin.getInst(), MAKEINTRESOURCE(IDI_LINKLISTICON)));
		WindowList_Add(hWindowList, hDlg, DlgParam->hContact);
		mir_snwprintf(title, L"%s [%s]", TranslateT("Linklist plugin"), Clist_GetContactDisplayName(DlgParam->hContact));
		SetWindowText(hDlg, title);
		GetFilterText(listMenu, filter, _countof(filter));
		SetDlgItemText(hDlg, IDC_STATUS, filter);

		mir_subclassWindow(GetDlgItem(hDlg, IDC_SPLITTER), SplitterProc);

		SendDlgItemMessage(hDlg, IDC_MAIN, EM_SETEVENTMASK, 0, (LPARAM)ENM_LINK);
		SendDlgItemMessage(hDlg, IDC_MAIN, EM_AUTOURLDETECT, TRUE, 0);
		// This is used in srmm... and I think he knew what he did... :)
		SendDlgItemMessage(hDlg, IDC_MAIN, EM_LIMITTEXT, (WPARAM)-1, 0);

		WriteLinkList(hDlg, WLL_ALL, (LISTELEMENT*)DlgParam->listStart, nullptr, 0);
		return TRUE;

		// open browser an load url if link is pressed
		// found at
		// http://www.tech-archive.net/Archive/Development/microsoft.public.win32.programmer.ui/2004-03/0133.html
		//
		// Popup menu on right mouse button is mainly taken from the miranda 
		// send/receive messaging plugin.
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == EN_LINK) {
			LPTSTR link;
			ENLINK *pENLink = (ENLINK*)lParam;

			uint8_t mouseEvent = g_plugin.getByte(LINKLIST_MOUSE_EVENT, 0xFF);

			switch (pENLink->msg) {
			case WM_MOUSEMOVE:
				if (mouseEvent != 0x01)
					break;
				memcpy(&DlgParam->chrg, &pENLink->chrg, sizeof(CHARRANGE));
				SendDlgItemMessage(hDlg, IDC_MAIN, EM_EXSETSEL, 0, (LPARAM)&pENLink->chrg);
				WriteMessage(hDlg, DlgParam->listStart, SendDlgItemMessage(hDlg, IDC_MAIN, EM_LINEFROMCHAR, -1, 0));
				break;
			case WM_LBUTTONUP:
				link = (LPTSTR)mir_alloc((pENLink->chrg.cpMax - pENLink->chrg.cpMin + 2) * sizeof(wchar_t));
				if (link == nullptr)
					break;
				SendDlgItemMessage(hDlg, IDC_MAIN, EM_EXSETSEL, 0, (LPARAM)&pENLink->chrg);
				SendDlgItemMessage(hDlg, IDC_MAIN, EM_GETSELTEXT, 0, (LPARAM)link);
				if (wcsstr(link, L"mailto:") != nullptr)
					ShellExecute(HWND_TOP, nullptr, link, nullptr, nullptr, SW_SHOWNORMAL);
				else {
					bool openNewWindow = g_plugin.getByte(LINKLIST_OPEN_WINDOW, 0xFF) != 0xFF;
					Utils_OpenUrlW(link, openNewWindow);
				}
				mir_free(link);
				break;

			case WM_RBUTTONDOWN:
				HMENU hPopup, hSubMenu;
				hPopup = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE(IDR_MENU2));
				hSubMenu = GetSubMenu(hPopup, 0);

				// Disable Menuoption if "mouse over" events are active
				mouseEvent = g_plugin.getByte(LINKLIST_MOUSE_EVENT, 0xFF);
				if (mouseEvent == 0x01)
					EnableMenuItem(hSubMenu, IDM_SHOWMESSAGE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				TranslateMenu(hSubMenu);
				link = (LPTSTR)mir_alloc((pENLink->chrg.cpMax - pENLink->chrg.cpMin + 4) * sizeof(wchar_t));
				if (link == nullptr)
					break;
				SendDlgItemMessage(hDlg, IDC_MAIN, EM_EXSETSEL, 0, (LPARAM)&pENLink->chrg);
				SendDlgItemMessage(hDlg, IDC_MAIN, EM_GETSELTEXT, 0, (LPARAM)link);

				pt.x = (short)LOWORD(pENLink->lParam);
				pt.y = (short)HIWORD(pENLink->lParam);
				ClientToScreen(((NMHDR*)lParam)->hwndFrom, &pt);

				switch (TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hDlg, nullptr)) {
				case IDM_LINK_OPEN:
					if (wcsstr(link, L"mailto:") != nullptr)
						ShellExecute(HWND_TOP, nullptr, link, nullptr, nullptr, SW_SHOWNORMAL);
					else
						Utils_OpenUrlW(link, false);
					break;

				case IDM_LINK_OPENNEW:
					if (wcsstr(link, L"mailto:") != nullptr)
						ShellExecute(HWND_TOP, nullptr, link, nullptr, nullptr, SW_SHOWNORMAL);
					else
						Utils_OpenUrlW(link);
					break;

				case IDM_LINK_COPY:
					Utils_ClipboardCopy(link);
					break;

				case IDM_SHOWMESSAGE:
					WriteMessage(hDlg, DlgParam->listStart, SendDlgItemMessage(hDlg, IDC_MAIN, EM_LINEFROMCHAR, -1, 0));
					break;
				}
				mir_free(link);
				DestroyMenu(hPopup);
			}
		}
		break;

	case WM_COMMAND:
		listMenu = GetMenu(hDlg);
		switch (wParam) {
		case IDM_SEARCH: // open Search Box
			if (DlgParam != nullptr) {
				HWND hWndSearchDlg = CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_SEARCH_DLG), hDlg, SearchDlgProc, (LPARAM)DlgParam);
				EnableMenuItem(listMenu, IDM_SEARCH, (MF_BYCOMMAND | MF_DISABLED | MF_GRAYED));
				ShowWindow(hWndSearchDlg, SW_SHOW);
				SetFocus(GetDlgItem(hWndSearchDlg, IDC_SEARCHSTRING));
			}
			break;

		case IDM_CLEARSEARCH: // clear search results
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			SetDlgItemText(hDlg, IDC_MAIN, L"");
			WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			break;

		case IDM_SAVE: // save button
			SaveEditAsStream(hDlg);
			SetFocus(GetDlgItem(hDlg, IDC_MAIN));
			break;

		case IDCANCEL: // Esc or Close pressed
		case IDM_CLOSE:
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;

		case IDM_DIR_IN: // view only incoming messages
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			if ((GetMenuState(listMenu, IDM_SEARCH, MF_BYCOMMAND) & MF_DISABLED))
				break; // not possible if search dialog is open
			SetDlgItemText(hDlg, IDC_MAIN, L"");
			if (GetMenuState(listMenu, IDM_DIR_IN, MF_BYCOMMAND) == MF_CHECKED) {
				CheckMenuItem(listMenu, IDM_DIR_IN, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			else {
				CheckMenuItem(listMenu, IDM_DIR_IN, MF_CHECKED);
				CheckMenuItem(listMenu, IDM_DIR_OUT, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			GetFilterText(GetMenu(hDlg), filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			break;

		case IDM_DIR_OUT: // view only outgoing messages
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			if ((GetMenuState(listMenu, IDM_SEARCH, MF_BYCOMMAND) & MF_DISABLED))
				break; // not possible if search dialog is open
			SetDlgItemText(hDlg, IDC_MAIN, L"");
			if (GetMenuState(listMenu, IDM_DIR_OUT, MF_BYCOMMAND) == MF_CHECKED) {
				CheckMenuItem(listMenu, IDM_DIR_OUT, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			else {
				CheckMenuItem(listMenu, IDM_DIR_OUT, MF_CHECKED);
				CheckMenuItem(listMenu, IDM_DIR_IN, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			break;

		case IDM_TYPE_WEB: // view only e-mail addresses
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			// not possible if search dialog is open
			if ((GetMenuState(listMenu, IDM_SEARCH, MF_BYCOMMAND) & MF_DISABLED))
				break;
			SetDlgItemText(hDlg, IDC_MAIN, L"");
			if (GetMenuState(listMenu, IDM_TYPE_WEB, MF_BYCOMMAND) == MF_CHECKED) {
				CheckMenuItem(listMenu, IDM_TYPE_WEB, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			else {
				CheckMenuItem(listMenu, IDM_TYPE_WEB, MF_CHECKED);
				CheckMenuItem(listMenu, IDM_TYPE_MAIL, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			break;

		case IDM_TYPE_MAIL: // view only URLs
			if ((GetMenuState(listMenu, IDM_SEARCH, MF_BYCOMMAND) & MF_DISABLED))
				break; // not possible if search dialog is open
			SetDlgItemText(hDlg, IDC_MAIN, L"");
			if (GetMenuState(listMenu, IDM_TYPE_MAIL, MF_BYCOMMAND) == MF_CHECKED) {
				CheckMenuItem(listMenu, IDM_TYPE_MAIL, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			else {
				CheckMenuItem(listMenu, IDM_TYPE_MAIL, MF_CHECKED);
				CheckMenuItem(listMenu, IDM_TYPE_WEB, MF_UNCHECKED);
				WriteLinkList(hDlg, GetFlags(listMenu), DlgParam->listStart, nullptr, 0);
			}
			GetFilterText(listMenu, filter, _countof(filter));
			SetDlgItemText(hDlg, IDC_STATUS, filter);
			break;
		}
		break;

		// Taken from srmm.
		// Btw: The longer I searched the source of this plugin
		// to learn how things work, the more I became a fan of
		// the programmer! 
	case WM_GETMINMAXINFO:
		RECT rcWindow, rcMain;
		GetWindowRect(hDlg, &rcWindow);
		GetWindowRect(GetDlgItem(hDlg, IDC_MAIN), &rcMain);
		{
			MINMAXINFO *mmi = (MINMAXINFO *)lParam;
			mmi->ptMinTrackSize.x = rcWindow.right - rcWindow.left - ((rcMain.right - rcMain.left) - DlgParam->minSize.cx);
			mmi->ptMinTrackSize.y = rcWindow.bottom - rcWindow.top - ((rcMain.bottom - rcMain.top) - DlgParam->minSize.cy);
		}
		break;

	case WM_SIZE:
		Utils_ResizeDialog(hDlg, g_plugin.getInst(), MAKEINTRESOURCEA(IDD_MAIN_DLG), LinklistResizer, (LPARAM)DlgParam);
		// To get some scrollbars if needed...
		RedrawWindow(GetDlgItem(hDlg, IDC_MAIN), nullptr, nullptr, RDW_INVALIDATE);
		RedrawWindow(GetDlgItem(hDlg, IDC_MESSAGE), nullptr, nullptr, RDW_INVALIDATE);
		break;

	case DM_LINKSPLITTER:
		int splitPosOld;

		GetClientRect(hDlg, &rc);
		pt.x = 0;
		pt.y = wParam;
		ScreenToClient(hDlg, &pt);

		splitPosOld = DlgParam->splitterPosNew;
		DlgParam->splitterPosNew = rc.bottom - pt.y;

		GetWindowRect(GetDlgItem(hDlg, IDC_MESSAGE), &rc);
		if (rc.bottom - rc.top + (DlgParam->splitterPosNew - splitPosOld) < 0)
			DlgParam->splitterPosNew = splitPosOld + 0 - (rc.bottom - rc.top);

		GetWindowRect(GetDlgItem(hDlg, IDC_MAIN), &rc);
		if (rc.bottom - rc.top - (DlgParam->splitterPosNew - splitPosOld) < DlgParam->minSize.cy)
			DlgParam->splitterPosNew = splitPosOld - DlgParam->minSize.cy + (rc.bottom - rc.top);
		SendMessage(hDlg, WM_SIZE, 0, 0);
		break;

	case WM_CLOSE:
		DestroyWindow(hDlg);
		break;

	case WM_DESTROY:
		if (DlgParam != nullptr) {
			hContact = ((g_plugin.getByte(LINKLIST_SAVESPECIAL, 0) == 0) ? NULL : DlgParam->hContact);
			Utils_SaveWindowPosition(hDlg, hContact, LINKLIST_MODULE, "Linklist");
			g_plugin.setDword(LINKLIST_SPLITPOS, DlgParam->splitterPosNew);
			RemoveList(DlgParam->listStart);
			mir_free(DlgParam);
		}
		// Remove entry from Window list
		WindowList_Remove(hWindowList, hDlg);
		break;
	}
	return FALSE;
}

/*
This function handles the search dialog messages
*/
INT_PTR CALLBACK SearchDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DIALOGPARAM *DlgParam = (DIALOGPARAM *)GetWindowLongPtr(hDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		SetWindowText(hDlg, TXT_SEARCH);
		CheckDlgButton(hDlg, IDC_DIR_ALL, BST_CHECKED);
		CheckDlgButton(hDlg, IDC_TYPE_ALL, BST_CHECKED);
		return TRUE;

	case WM_COMMAND:
		HWND hWndMain = WindowList_Find(hWindowList, DlgParam->hContact);

		switch (wParam) {
		case IDCLOSE:
		case IDCANCEL:
			EnableMenuItem(GetMenu(hWndMain), 101, MF_BYCOMMAND | MF_ENABLED);
			DestroyWindow(hDlg);
			return TRUE;

		case IDSEARCH:
			uint8_t flags = 0x00;
			wchar_t filter[FILTERTEXT];

			HWND hListDlg = WindowList_Find(hWindowList, DlgParam->hContact);
			if (hListDlg == nullptr)
				break;

			SetDlgItemText(hListDlg, IDC_MAIN, L"");
			if (IsDlgButtonChecked(hDlg, IDC_TYPE_WEB) == BST_UNCHECKED)
				flags |= WLL_MAIL;
			if (IsDlgButtonChecked(hDlg, IDC_TYPE_MAIL) == BST_UNCHECKED)
				flags |= WLL_URL;
			if (IsDlgButtonChecked(hDlg, IDC_DIR_IN) == BST_UNCHECKED)
				flags |= WLL_OUT;
			if (IsDlgButtonChecked(hDlg, IDC_DIR_OUT) == BST_UNCHECKED)
				flags |= WLL_IN;
			if (IsDlgButtonChecked(hDlg, IDC_WHOLE_MESSAGE) == BST_CHECKED)
				flags |= SLL_DEEP;

			int length = GetWindowTextLength(GetDlgItem(hDlg, IDC_SEARCHSTRING)) + 1;
			LPTSTR buffer = (LPTSTR)mir_alloc((length + 1) * sizeof(wchar_t));
			if (buffer) {
				GetDlgItemText(hDlg, IDC_SEARCHSTRING, buffer, length);
				WriteLinkList(hListDlg, flags, DlgParam->listStart, buffer, 0);
				mir_free(buffer);
			}
			mir_snwprintf(filter, L"%s: %s", TXT_FILTER, TXT_SEARCHFILTER);
			SetDlgItemText(hWndMain, IDC_STATUS, filter);
			break;
		}
		break;
	}
	return FALSE;
}

/*
This function handles the options dialog messages
*/
INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	uint8_t useDefault;

	switch (message) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);
		useDefault = g_plugin.getByte(LINKLIST_USE_DEF, 0xFF);
		if (useDefault == 0x01) {
			int mCol = GetMirandaColour(&colourSet);
			if (mCol == 0) {
				CheckDlgButton(hDlg, IDC_CHECK1, BST_CHECKED);
				EnableWindow(GetDlgItem(hDlg, IDC_INCOMING), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_OUTGOING), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_BACKGROUND), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_IN), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_OUT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_BG), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_TXT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTIN), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTOUT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTBG), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTTXT), FALSE);
			}
			else useDefault = 0x00;
		}

		if (useDefault == 0x00) {
			GetDBColour(&colourSet);
			CheckDlgButton(hDlg, IDC_CHECK1, BST_UNCHECKED);
			SendDlgItemMessage(hDlg, IDC_INCOMING, CPM_SETCOLOUR, 0, colourSet.incoming);
			SendDlgItemMessage(hDlg, IDC_OUTGOING, CPM_SETCOLOUR, 0, colourSet.outgoing);
			SendDlgItemMessage(hDlg, IDC_BACKGROUND, CPM_SETCOLOUR, 0, colourSet.background);
			SendDlgItemMessage(hDlg, IDC_TXT, CPM_SETCOLOUR, 0, colourSet.text);
		}

		// Init variables with current values from DB
		GetListOptions(&options);

		if (options.openNewWindow == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK2, BST_UNCHECKED);
		if (options.openNewWindow == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK2, BST_CHECKED);

		if (options.updateWindow == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK3, BST_UNCHECKED);
		if (options.updateWindow == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK3, BST_CHECKED);

		if (options.mouseEvent == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK4, BST_UNCHECKED);
		if (options.mouseEvent == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK4, BST_CHECKED);

		if (options.saveSpecial == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK5, BST_UNCHECKED);
		if (options.saveSpecial == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK5, BST_CHECKED);

		if (options.showDate == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK6, BST_UNCHECKED);
		if (options.showDate == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK6, BST_CHECKED);

		if (options.showLine == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK7, BST_UNCHECKED);
		if (options.showLine == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK7, BST_CHECKED);

		if (options.showTime == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK8, BST_UNCHECKED);
		if (options.showTime == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK8, BST_CHECKED);

		if (options.showDirection == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK9, BST_UNCHECKED);
		if (options.showDirection == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK9, BST_CHECKED);

		if (options.showType == 0x00)
			CheckDlgButton(hDlg, IDC_CHECK10, BST_UNCHECKED);
		if (options.showType == 0x01)
			CheckDlgButton(hDlg, IDC_CHECK10, BST_CHECKED);

		// Write example window
		WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_DEFAULT_IN:
			colourSet.incoming = IN_COL_DEF;
			SendDlgItemMessage(hDlg, IDC_INCOMING, CPM_SETCOLOUR, 0, colourSet.incoming);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_DEFAULT_OUT:
			colourSet.outgoing = OUT_COL_DEF;
			SendDlgItemMessage(hDlg, IDC_OUTGOING, CPM_SETCOLOUR, 0, colourSet.outgoing);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_DEFAULT_BG:
			colourSet.background = BG_COL_DEF;
			SendDlgItemMessage(hDlg, IDC_BACKGROUND, CPM_SETCOLOUR, 0, colourSet.background);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_DEFAULT_TXT:
			colourSet.text = TXT_COL_DEF;
			SendDlgItemMessage(hDlg, IDC_TXT, CPM_SETCOLOUR, 0, colourSet.text);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_INCOMING:
			colourSet.incoming = SendDlgItemMessage(hDlg, IDC_INCOMING, CPM_GETCOLOUR, 0, 0);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_OUTGOING:
			colourSet.outgoing = SendDlgItemMessage(hDlg, IDC_OUTGOING, CPM_GETCOLOUR, 0, 0);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_BACKGROUND:
			colourSet.background = SendDlgItemMessage(hDlg, IDC_BACKGROUND, CPM_GETCOLOUR, 0, 0);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_TXT:
			colourSet.text = SendDlgItemMessage(hDlg, IDC_TXT, CPM_GETCOLOUR, 0, 0);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;

		case IDC_CHECK1:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK1) == BST_UNCHECKED) {
				EnableWindow(GetDlgItem(hDlg, IDC_INCOMING), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_OUTGOING), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_BACKGROUND), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXT), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_IN), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_OUT), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_BG), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_TXT), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTIN), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTOUT), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTBG), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTTXT), TRUE);
				GetDBColour(&colourSet);
				SendDlgItemMessage(hDlg, IDC_INCOMING, CPM_SETCOLOUR, 0, colourSet.incoming);
				SendDlgItemMessage(hDlg, IDC_OUTGOING, CPM_SETCOLOUR, 0, colourSet.outgoing);
				SendDlgItemMessage(hDlg, IDC_BACKGROUND, CPM_SETCOLOUR, 0, colourSet.background);
				SendDlgItemMessage(hDlg, IDC_TXT, CPM_SETCOLOUR, 0, colourSet.text);
			}
			else {
				int mCol = GetMirandaColour(&colourSet);
				if (mCol == 1) {
					MessageBox(nullptr, TXT_NOSETTING, TXT_ERROR, MB_OK | MB_ICONEXCLAMATION);
					CheckDlgButton(hDlg, IDC_CHECK1, BST_UNCHECKED);
					break;
				}

				EnableWindow(GetDlgItem(hDlg, IDC_INCOMING), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_OUTGOING), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_BACKGROUND), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_IN), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_OUT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_BG), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULT_TXT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTIN), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTOUT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTBG), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_TXTTXT), FALSE);
			}
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK2:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK2) == BST_UNCHECKED)
				options.openNewWindow = 0;
			else
				options.openNewWindow = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);

			break;

		case IDC_CHECK3:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK3) == BST_UNCHECKED)
				options.updateWindow = 0;
			else
				options.updateWindow = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK4:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK4) == BST_UNCHECKED)
				options.mouseEvent = 0;
			else
				options.mouseEvent = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK5:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK5) == BST_UNCHECKED)
				options.saveSpecial = 0;
			else
				options.saveSpecial = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK6:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK6) == BST_UNCHECKED)
				options.showDate = 0;
			else
				options.showDate = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK7:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK7) == BST_UNCHECKED)
				options.showLine = 0;
			else
				options.showLine = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK8:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK8) == BST_UNCHECKED)
				options.showTime = 0;
			else
				options.showTime = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK9:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK9) == BST_UNCHECKED)
				options.showDirection = 0;
			else
				options.showDirection = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;

		case IDC_CHECK10:
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			if (IsDlgButtonChecked(hDlg, IDC_CHECK10) == BST_UNCHECKED)
				options.showType = 0;
			else
				options.showType = 1;

			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			WriteOptionExample(hDlg, colourSet.incoming, colourSet.outgoing, colourSet.background, colourSet.text, &options);
			break;
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == PSN_APPLY) {
			// Write Settings to Database
			if (IsDlgButtonChecked(hDlg, IDC_CHECK1) == BST_CHECKED)
				g_plugin.setByte(LINKLIST_USE_DEF, 0x01);
			else {
				g_plugin.setByte(LINKLIST_USE_DEF, 0x00);
				colourSet.incoming = SendDlgItemMessage(hDlg, IDC_INCOMING, CPM_GETCOLOUR, 0, 0);
				colourSet.outgoing = SendDlgItemMessage(hDlg, IDC_OUTGOING, CPM_GETCOLOUR, 0, 0);
				colourSet.background = SendDlgItemMessage(hDlg, IDC_BACKGROUND, CPM_GETCOLOUR, 0, 0);
				colourSet.text = SendDlgItemMessage(hDlg, IDC_TXT, CPM_GETCOLOUR, 0, 0);
				SetDBColour(&colourSet);
			}

			SetListOptions(&options);
		}
	}
	return FALSE;
}

/*
Progressbar
*/
LRESULT CALLBACK ProgressBarDlg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndPB;

	switch (message) {
	case WM_CREATE:
		hwndPB = CreateWindowEx(0, PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE, 0, 2, 343, 17, hwnd, nullptr, g_plugin.getInst(), nullptr);
		SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		return 0;

	case WM_COMMAND:
		if (wParam == 100)
			SendMessage(hwndPB, PBM_SETPOS, (WPARAM)lParam, 0);
		return 0;

	case WM_DESTROY:
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

/*
Splitter function.
Taken from srmm-plugin....
*/
LRESULT CALLBACK SplitterProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT rc;

	switch (msg) {
	case WM_NCHITTEST:
		return HTCLIENT;
		break;

	case WM_SETCURSOR:
		GetClientRect(hWnd, &rc);
		SetCursor(splitCursor);
		return TRUE;

	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		return 0;
		break;

	case WM_MOUSEMOVE:
		if (GetCapture() == hWnd) {
			GetClientRect(hWnd, &rc);
			SendMessage(GetParent(hWnd), DM_LINKSPLITTER, (WPARAM)(HIWORD(GetMessagePos()) + rc.bottom / 2), (LPARAM)hWnd);
		}
		return 0;

	case WM_LBUTTONUP:
		ReleaseCapture();
		return 0;
	}

	return mir_callNextSubclass(hWnd, SplitterProc, msg, wParam, lParam);
}
