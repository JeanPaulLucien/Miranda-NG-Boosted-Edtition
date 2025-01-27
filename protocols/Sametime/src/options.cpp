#include "StdAfx.h"
#include "sametime.h"


#define DEFAULT_ID		(0x1800)

#define NUM_IDS		24

wchar_t* client_names[NUM_IDS] = {
	L"Official Binary Library",
	L"Official Java Applet",
	L"Official Binary Application", 
	L"Official Java Application",
	L"Notes 6.5",
	L"Notes 7.0",
	L"Notes 8.0 Basic",
	L"Notes 8.5.2",
	L"Sametime Connect 8.0",
	L"Sametime Connect 8.5.2",
	L"ICT",
	L"NotesBuddy",
	L"NotesBuddy 4.15",
	L"Sanity",
	L"Perl",
	L"PMR Alert",
	L"Trillian (SourceForge)",
	L"Trillian (IBM)",
	L"Meanwhile Library",
	L"Meanwhile (Python)",
	L"Meanwhile (Gaim)",
	L"Meanwhile (Adium)",
	L"Meanwhile (Kopete)",
	L"Custom"
};
int client_ids[NUM_IDS] = {
	0x1000,
	0x1001,
	0x1002,
	0x1003,
	0x1200,
	0x1210,
	0x1215,
	0x1240,
	0x1306,
	0x130D,
	0x1300,
	0x1400,
	0x1405,
	0x1600,
	0x1625,
	0x1650,
	0x16aa,
	0x16bb,
	0x1700,
	0x1701,
	0x1702,
	0x1703,
	0x1704,
	0xFFFF
};


#define DEFAULT_CV_MAJOR		(0x001e)
#define DEFAULT_CV_MINOR		(0x1f4b)

#define NUM_CVS		5

wchar_t* CV_names[NUM_CVS] = {
	L"Sametime (Use old client version)",
	L"Sametime (Miranda default)",
	L"Sametime 8",
	L"Sametime 8.5.1",
	L"Sametime 8.5.2"
};
int CV_major[NUM_CVS] = {
	MW_PROTOCOL_VERSION_MAJOR,
	0x001e,
	0x001e,
	0x001e,
	0x001e
};
int CV_minor[NUM_CVS] = {
	MW_PROTOCOL_VERSION_MINOR,
	0x196f,
	0x1f4b,
	0x213f,
	0x2149
};



static INT_PTR CALLBACK DlgProcOptNet(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CSametimeProto* proto = (CSametimeProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG: {
		TranslateDialogDefault(hwndDlg);

		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		proto = (CSametimeProto*)lParam;

		uint16_t client_ver = proto->GetClientVersion();
		if (client_ver) {
			wchar_t verbuf[100];
			mir_snwprintf(verbuf, TranslateT("Client protocol version: %03d.%03d"), (client_ver & 0xFF00) >> 8, client_ver & 0xFF);
			SetDlgItemText(hwndDlg, IDC_ST_CLIENTVER, verbuf);
		}

		uint16_t server_ver = proto->GetServerVersion();
		if (server_ver) {
			wchar_t verbuf[100];
			mir_snwprintf(verbuf, TranslateT("Server protocol version: %03d.%03d"), (server_ver & 0xFF00) >> 8, server_ver & 0xFF);
			SetDlgItemText(hwndDlg, IDC_ST_SERVERVER, verbuf);
		}

		wchar_t *s = mir_utf8decodeW(proto->options.server_name); SetDlgItemText(hwndDlg, IDC_ED_SNAME, s); mir_free(s);
		s = mir_utf8decodeW(proto->options.id); SetDlgItemText(hwndDlg, IDC_ED_NAME, s); mir_free(s);
		s = mir_utf8decodeW(proto->options.pword); SetDlgItemText(hwndDlg, IDC_ED_PWORD, s); mir_free(s);

		SetDlgItemInt(hwndDlg, IDC_ED_PORT, proto->options.port, FALSE);
		CheckDlgButton(hwndDlg, IDC_CHK_GETSERVERCONTACTS, proto->options.get_server_contacts ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHK_ADDCONTACTS, proto->options.add_contacts ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHK_IDLEAWAY, proto->options.idle_as_away ? BST_CHECKED : BST_UNCHECKED);

		SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_RESETCONTENT, 0, 0);
		int pos = 0;
		bool found = false;

		for (int i = 0; i < NUM_IDS; i++) {
			pos = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_ADDSTRING, -1, (LPARAM)client_names[i]);
			SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_SETITEMDATA, pos, client_ids[i]);
			if (client_ids[i] == proto->options.client_id) {
				found = true;
				SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_SETCURSEL, pos, 0);
				SetDlgItemInt(hwndDlg, IDC_ED_CLIENTID, client_ids[i], FALSE);
				if (i != sizeof(client_ids) / sizeof(int)-1) {
					HWND hw = GetDlgItem(hwndDlg, IDC_ED_CLIENTID);
					EnableWindow(hw, false);
				}
			}
		}

		if (!found) {
			SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_SETCURSEL, pos, 0); // pos is last item, i.e. custom
			SetDlgItemInt(hwndDlg, IDC_ED_CLIENTID, proto->options.client_id, FALSE);
		}


		SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_RESETCONTENT, 0, 0);
		pos = 0;

		for (int i = 0; i < NUM_CVS; i++) {
			pos = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_ADDSTRING, -1, (LPARAM)CV_names[i]);
			SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_SETITEMDATA, pos, i);
			if (CV_major[i] == proto->options.client_versionMajor && CV_minor[i] == proto->options.client_versionMinor) {
				found = true;
				SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_SETCURSEL, pos, 0);
			}
		}

		switch (proto->options.err_method) {
			case ED_POP: CheckDlgButton(hwndDlg, IDC_RAD_ERRPOP, BST_CHECKED); break;
			case ED_MB: CheckDlgButton(hwndDlg, IDC_RAD_ERRMB, BST_CHECKED); break;
			case ED_BAL: CheckDlgButton(hwndDlg, IDC_RAD_ERRBAL, BST_CHECKED); break;
		}

		if (proto->options.encrypt_session)
			CheckDlgButton(hwndDlg, IDC_RAD_ENC, BST_CHECKED);
		else
			CheckDlgButton(hwndDlg, IDC_RAD_NOENC, BST_CHECKED);

		return FALSE;
	}

	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == GetFocus())
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);

		if (HIWORD(wParam) == CBN_SELCHANGE) {
			switch (LOWORD(wParam)) {
			case IDC_CMB_CLIENT:
			{
				int sel = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_GETCURSEL, 0, 0);
				int id = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_GETITEMDATA, sel, 0);
				bool custom = (id == client_ids[sizeof(client_ids) / sizeof(int)-1]);

				if (!custom)
				   SetDlgItemInt(hwndDlg, IDC_ED_CLIENTID, id, FALSE);
				else
				   SetDlgItemInt(hwndDlg, IDC_ED_CLIENTID, DEFAULT_ID, FALSE);

				HWND hw = GetDlgItem(hwndDlg, IDC_ED_CLIENTID);
				EnableWindow(hw, custom);

				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				return TRUE;
			}
			case IDC_CMB_CLIENTVN:
			{
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				return TRUE;
			}
			}
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDC_BTN_UPLOADCONTACTS:
				{
					EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_UPLOADCONTACTS), FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_IMPORTCONTACTS), FALSE);

					proto->ExportContactsToServer();

					SendMessage(hwndDlg, WMU_STORECOMPLETE, 0, 0);
				}
				return TRUE;

			case IDC_BTN_IMPORTCONTACTS:
				{
					wchar_t import_filename[MAX_PATH]; import_filename[0] = 0;

					OPENFILENAME ofn = { 0 };
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFile = import_filename;
					ofn.hwndOwner = hwndDlg;
					ofn.Flags = CC_FULLOPEN;
					ofn.nMaxFile = MAX_PATH;
					ofn.lpstrFilter = L"All\0*.*\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = nullptr;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = nullptr;
					ofn.Flags = OFN_PATHMUSTEXIST;

					if (GetOpenFileName(&ofn) == TRUE) {
						HWND hBut = GetDlgItem(hwndDlg, IDC_BTN_UPLOADCONTACTS);
						EnableWindow(hBut, FALSE);
						hBut = GetDlgItem(hwndDlg, IDC_BTN_IMPORTCONTACTS);
						EnableWindow(hBut, FALSE);

						proto->ImportContactsFromFile(ofn.lpstrFile);

						SendMessage(hwndDlg, WMU_STORECOMPLETE, 0, 0);
					}
				}
				return TRUE;

			case IDC_CHK_GETSERVERCONTACTS:
			case IDC_CHK_ENCMESSAGES:
			case IDC_RAD_ERRMB:
			case IDC_RAD_ERRBAL:
			case IDC_RAD_ERRPOP:
			case IDC_CHK_USERCP:
			case IDC_CHK_ADDCONTACTS:
			case IDC_CHK_IDLEAWAY:
			case IDC_RAD_ENC:
			case IDC_RAD_NOENC:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				return TRUE;
			case IDC_RAD_ANSI:
			case IDC_RAD_UTF8:
			case IDC_RAD_OEM:
			case IDC_RAD_UTF7:
			case IDC_RAD_USERCP:
				HWND hw = GetDlgItem(hwndDlg, IDC_CHK_USERCP);
				EnableWindow(hw, BST_UNCHECKED == IsDlgButtonChecked(hwndDlg, IDC_RAD_USERCP));

				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				return TRUE;
			}
		}
		break;

	case WMU_STORECOMPLETE:
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_UPLOADCONTACTS), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_IMPORTCONTACTS), TRUE);
		return TRUE;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == PSN_APPLY) {
			wchar_t ws[2048];

			GetDlgItemText(hwndDlg, IDC_ED_SNAME, ws, LSTRINGLEN);
			mir_strcpy(proto->options.server_name, T2Utf(ws));
			GetDlgItemText(hwndDlg, IDC_ED_NAME, ws, LSTRINGLEN);
			mir_strcpy(proto->options.id, T2Utf(ws));
			GetDlgItemText(hwndDlg, IDC_ED_PWORD, ws, LSTRINGLEN);
			mir_strcpy(proto->options.pword, T2Utf(ws));

			BOOL translated;
			int port = GetDlgItemInt(hwndDlg, IDC_ED_PORT, &translated, FALSE);
			if (translated)
				proto->options.port = port;

			proto->options.get_server_contacts = (IsDlgButtonChecked(hwndDlg, IDC_CHK_GETSERVERCONTACTS) != FALSE);

			int sel = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_GETCURSEL, 0, 0);
			int id = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENT, CB_GETITEMDATA, sel, 0);

			if (id == client_ids[sizeof(client_ids) / sizeof(int)-1]) {
				BOOL trans;
				id = GetDlgItemInt(hwndDlg, IDC_ED_CLIENTID, &trans, FALSE);
				if (trans)
					proto->options.client_id = id;
			}
			else proto->options.client_id = id;

			sel = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_GETCURSEL, 0, 0);
			int CVpos = SendDlgItemMessage(hwndDlg, IDC_CMB_CLIENTVN, CB_GETITEMDATA, sel, 0);
			proto->options.client_versionMajor = CV_major[CVpos];
			proto->options.client_versionMinor = CV_minor[CVpos];

			if (IsDlgButtonChecked(hwndDlg, IDC_RAD_ERRMB)) proto->options.err_method = ED_MB;
			else if (IsDlgButtonChecked(hwndDlg, IDC_RAD_ERRBAL)) proto->options.err_method = ED_BAL;
			else if (IsDlgButtonChecked(hwndDlg, IDC_RAD_ERRPOP)) proto->options.err_method = ED_POP;

			proto->options.add_contacts = (IsDlgButtonChecked(hwndDlg, IDC_CHK_ADDCONTACTS) != FALSE);
			proto->options.encrypt_session = (IsDlgButtonChecked(hwndDlg, IDC_RAD_ENC) != FALSE);
			proto->options.idle_as_away = (IsDlgButtonChecked(hwndDlg, IDC_CHK_IDLEAWAY) != FALSE);

			proto->SaveOptions();

			return TRUE;
		}
		break;

	case WM_DESTROY:
		break;
	}

	return FALSE;
}

int CSametimeProto::OptInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.flags = ODPF_BOLDGROUPS | ODPF_UNICODE | ODPF_DONTTRANSLATE;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTNET);
	odp.szTitle.w = m_tszUserName;
	odp.szGroup.w = LPGENW("Network");
	odp.pfnDlgProc = DlgProcOptNet;
	odp.dwInitParam = (LPARAM)this;
	g_plugin.addOptions(wParam, &odp);
	return 0;
}

void CSametimeProto::LoadOptions()
{
	DBVARIANT dbv;

	if (!getUString(0, "ServerName", &dbv)) {
		strncpy(options.server_name, dbv.pszVal, LSTRINGLEN);
		db_free(&dbv);
	}
	if (!getUString(0, "stid", &dbv)) {
		strncpy(options.id, dbv.pszVal, LSTRINGLEN);
		db_free(&dbv);
	}
	if (!getUString(0, "Password", &dbv)) {
		strncpy(options.pword, dbv.pszVal, LSTRINGLEN);
		db_free(&dbv);
	}

	options.port = db_get_dw(0, m_szModuleName, "ServerPort", DEFAULT_PORT);
	options.encrypt_session = (db_get_b(0, m_szModuleName, "EncryptSession", 0) == 1);

	options.client_id = db_get_dw(0, m_szModuleName, "ClientID", DEFAULT_ID);
	options.client_versionMajor = db_get_dw(0, m_szModuleName, "ClientVersionMajor", DEFAULT_CV_MAJOR);
	uint32_t defaultCVMinor;
	if (db_get_b(0, m_szModuleName, "UseOldClientVer", 0) == 1){
		// if sb have checked old deprecated option 'Use old client version' respect it
		defaultCVMinor = 0x001d;
	} else {
		defaultCVMinor = DEFAULT_CV_MINOR;
	}
	options.client_versionMinor = db_get_dw(0, m_szModuleName, "ClientVersionMinor", defaultCVMinor);

	// one time options conversion
	if (db_get_b(0, m_szModuleName, "UseOldClientVer", -1) != -1){
		db_unset(0, m_szModuleName, "UseOldClientVer");
		db_set_dw(0, m_szModuleName, "ClientVersionMajor", options.client_versionMajor);
		db_set_dw(0, m_szModuleName, "ClientVersionMinor", options.client_versionMinor);
	}

	options.get_server_contacts = (db_get_b(0, m_szModuleName, "GetServerContacts", 1) == 1);
	options.add_contacts = (db_get_b(0, m_szModuleName, "AutoAddContacts", 0) == 1);
	options.idle_as_away = (db_get_b(0, m_szModuleName, "IdleAsAway", 1) == 1);

	// if popups not installed, will be changed to 'ED_BAL' (balloons) in main.cpp, modules loaded
	options.err_method = (ErrorDisplay)db_get_b(0, m_szModuleName, "ErrorDisplay", ED_POP);

	debugLogA("LoadOptions() loaded: ServerName:len=[%d], id:len=[%d], pword:len=[%d]", options.server_name == nullptr ? -1 : mir_strlen(options.server_name), options.id == nullptr ? -1 : mir_strlen(options.id), options.pword == nullptr ? -1 : mir_strlen(options.pword));
	debugLogA("LoadOptions() loaded: port=[%d], encrypt_session=[%d], ClientID=[%d], ClientVersionMajor=[%d], ClientVersionMinor=[%d]", options.port, options.encrypt_session, options.client_id, options.client_versionMajor, options.client_versionMinor);
	debugLogA("LoadOptions() loaded: get_server_contacts=[%d], add_contacts=[%d], idle_as_away=[%d], err_method=[%d]", options.get_server_contacts, options.add_contacts, options.idle_as_away, options.err_method);
}

void CSametimeProto::SaveOptions()
{
	setUString("ServerName", options.server_name);

	setUString("stid", options.id);
	setUString("Password", options.pword);

	setDword("ServerPort", options.port);
	setByte("GetServerContacts", options.get_server_contacts ? 1 : 0);
	setDword("ClientID", options.client_id);
	setDword("ClientVersionMajor", options.client_versionMajor);
	setDword("ClientVersionMinor", options.client_versionMinor);
	
	setByte("ErrorDisplay", options.err_method);
	setByte("AutoAddContacts", options.add_contacts ? 1 : 0);
	setByte("EncryptSession", options.encrypt_session ? 1 : 0);
	setByte("IdleAsAway", options.idle_as_away ? 1 : 0);
}
