#include "commonheaders.h"

#define PSKSIZE (4096+1)
#define RSASIZE (4096+1)

static BOOL bChangeSortOrder = false;

static BOOL hasKey(pUinKey ptr)
{
	BOOL ret = 0;
	if (ptr->mode == MODE_NATIVE) {
		LPSTR str = db_get_sa(ptr->hContact, MODULENAME, "PSK");
		ret = (str != nullptr); SAFE_FREE(str);
	}
	else if (ptr->mode == MODE_RSAAES) {
		DBVARIANT dbv;
		dbv.type = DBVT_BLOB;
		if (db_get(ptr->hContact, MODULENAME, "rsa_pub", &dbv) == 0) {
			ret = 1;
			db_free(&dbv);
		}
	}
	return ret;
}

static void TC_InsertItem(HWND hwnd, WPARAM wparam, TCITEM *tci)
{
	LPWSTR tmp = mir_a2u(tci->pszText);
	tci->pszText = (LPSTR)TranslateW(tmp);
	SNDMSG(hwnd, TCM_INSERTITEMW, wparam, (LPARAM)tci);
	mir_free(tmp);
}

static void LV_InsertColumn(HWND hwnd, WPARAM wparam, LVCOLUMN *lvc)
{
	LPWSTR tmp = mir_a2u(lvc->pszText);
	lvc->pszText = (LPSTR)TranslateW(tmp);
	SNDMSG(hwnd, LVM_INSERTCOLUMNW, wparam, (LPARAM)lvc);
	mir_free(tmp);
}

static int LV_InsertItem(HWND hwnd, LVITEMW *lvi)
{
	return SNDMSG(hwnd, LVM_INSERTITEMW, 0, (LPARAM)lvi);
}

static void LV_SetItemText(HWND hwnd, WPARAM wparam, int subitem, LPWSTR text)
{
	LV_ITEMW lvi = {};
	lvi.iSubItem = subitem;
	lvi.pszText = text;
	SNDMSG(hwnd, LVM_SETITEMTEXTW, wparam, (LPARAM)&lvi);
}

static void LV_SetItemTextA(HWND hwnd, WPARAM wparam, int subitem, LPCSTR text)
{
	wchar_t *p = mir_a2u(text);
	LV_SetItemText(hwnd, wparam, subitem, p);
	mir_free(p);
}

static void LV_GetItemTextA(HWND hwnd, WPARAM wparam, int iSubItem, LPSTR text, int cchTextMax)
{
	LV_ITEM lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.iSubItem = iSubItem;
	lvi.cchTextMax = cchTextMax;
	lvi.pszText = text;
	SNDMSG(hwnd, LVM_GETITEMTEXTW, wparam, (LPARAM)&lvi);
	lvi.pszText = mir_u2a((LPWSTR)text);
	mir_strcpy(text, lvi.pszText);
	mir_free(lvi.pszText);
}

static LPARAM getListViewParam(HWND hLV, UINT iItem)
{
	LVITEM lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.iItem = iItem;
	lvi.mask = LVIF_PARAM;
	ListView_GetItem(hLV, &lvi);
	return lvi.lParam;
}

static void setListViewIcon(HWND hLV, UINT iItem, pUinKey ptr)
{
	LVITEM lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.iItem = iItem;
	switch (ptr->tmode) {
	case MODE_NATIVE:
	case MODE_RSAAES:
		lvi.iImage = ICO_ST_DIS + ptr->tstatus;
		break;
	case MODE_PGP:
		lvi.iImage = ICO_OV_PGP;
		break;
	case MODE_GPG:
		lvi.iImage = ICO_OV_GPG;
		break;
	}
	lvi.mask = LVIF_IMAGE;
	ListView_SetItem(hLV, &lvi);
}

static void setListViewMode(HWND hLV, UINT iItem, UINT iMode)
{
	extern LPCSTR sim231[];
	char tmp[256];
	strncpy(tmp, Translate(sim231[iMode]), sizeof(tmp) - 1);
	LV_SetItemTextA(hLV, iItem, 3, tmp);
}

static void setListViewStatus(HWND hLV, UINT iItem, UINT iStatus)
{
	extern LPCSTR sim232[];
	char tmp[128];
	strncpy(tmp, Translate(sim232[iStatus]), sizeof(tmp) - 1);
	LV_SetItemTextA(hLV, iItem, 4, tmp);
}

static UINT getListViewPSK(HWND hLV, UINT iItem)
{
	char str[128];
	LV_GetItemTextA(hLV, iItem, 5, str, _countof(str));
	return strncmp(str, Translate("PSK"), sizeof(str)) == 0;
}

static void setListViewPSK(HWND hLV, UINT iItem, UINT iStatus)
{
	char str[128];
	strncpy(str, (iStatus) ? Translate("PSK") : "-", sizeof(str) - 1);
	LV_SetItemTextA(hLV, iItem, 5, str);
}

static UINT getListViewPUB(HWND hLV, UINT iItem)
{
	char str[128];
	LV_GetItemTextA(hLV, iItem, 5, str, _countof(str));
	return strncmp(str, Translate("PUB"), sizeof(str)) == 0;
}

static void setListViewPUB(HWND hLV, UINT iItem, UINT iStatus)
{
	char str[128];
	strncpy(str, (iStatus) ? Translate("PUB") : "-", sizeof(str) - 1);
	LV_SetItemTextA(hLV, iItem, 5, str);

	LPSTR sha = nullptr;
	if (iStatus) {
		DBVARIANT dbv;
		dbv.type = DBVT_BLOB;
		pUinKey ptr = (pUinKey)getListViewParam(hLV, iItem);
		if (db_get(ptr->hContact, MODULENAME, "rsa_pub", &dbv) == 0) {
			int len;
			mir_exp->rsa_get_hash((uint8_t*)dbv.pbVal, dbv.cpbVal, (uint8_t*)str, &len);
			sha = mir_strdup(to_hex((uint8_t*)str, len));
			db_free(&dbv);
		}
	}
	if (sha) {
		LV_SetItemTextA(hLV, iItem, 6, sha);
		mir_free(sha);
	}
	else LV_SetItemTextA(hLV, iItem, 6, "");
}

static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	pUinKey p1 = pUinKey(lParam1), p2 = pUinKey(lParam2);
	wchar_t t1[NAMSIZE], t2[NAMSIZE];
	int s = 0, d = 0, m = 1;
	DBVARIANT dbv1 = { 0 }, dbv2 = { 0 };

	if (lParamSort & 0x100) {
		lParamSort &= 0xFF;
		m = -1;
	}

	switch (lParamSort) {
	case 0x01:
	case 0x11:
	case 0x21:
		return mir_wstrcmp(Clist_GetContactDisplayName(p1->hContact), Clist_GetContactDisplayName(p2->hContact)) * m;

	case 0x02:
	case 0x12:
	case 0x22:
		getContactUin(p1->hContact, t1);
		getContactUin(p2->hContact, t2);
		return mir_wstrcmp(t1, t2) * m;

	case 0x03:
		s = p1->tmode;
		d = p2->tmode;
		return (s - d) * m;

	case 0x13:
		if (!db_get_s(p1->hContact, MODULENAME, "pgp_abbr", &dbv1)) {
			if (!db_get_s(p2->hContact, MODULENAME, "pgp_abbr", &dbv2)) {
				s = (dbv1.type == DBVT_ASCIIZ);
				d = (dbv2.type == DBVT_ASCIIZ);
				if (s && d) {
					s = mir_strcmp(dbv1.pszVal, dbv2.pszVal);
					d = 0;
				}
				db_free(&dbv1);
			}
			db_free(&dbv2);
			return (s - d) * m;
		}
		else
			return 0;

	case 0x23:
		if (!db_get_s(p1->hContact, MODULENAME, "gpg", &dbv1)) {
			s = (dbv1.type == DBVT_ASCIIZ);
			if (!db_get_s(p2->hContact, MODULENAME, "gpg", &dbv2)) {
				d = (dbv2.type == DBVT_ASCIIZ);
				if (s && d) {
					s = mir_strcmp(dbv1.pszVal, dbv2.pszVal);
					d = 0;
				}
				db_free(&dbv1);
			}
			db_free(&dbv2);
			return (s - d) * m;
		}
		else
			return 0;

	case 0x04:
		s = p1->tstatus;
		d = p2->tstatus;
		return (s - d) * m;

	case 0x05:
		if (!db_get_s(p1->hContact, MODULENAME, "PSK", &dbv1)) {
			s = (dbv1.type == DBVT_ASCIIZ);
			if (!db_get_s(p2->hContact, MODULENAME, "PSK", &dbv2)) {
				d = (dbv2.type == DBVT_ASCIIZ);
				db_free(&dbv2);
			}
			db_free(&dbv1);
			return (s - d) * m;
		}
		else
			return 0;
	}
	return 0;
}

void ListView_Sort(HWND hLV, LPARAM lParamSort)
{
	// restore sort column
	char t[32];
	mir_snprintf(t, "os%02x", (UINT)lParamSort & 0xF0);
	if ((lParamSort & 0x0F) == 0)
		lParamSort = (int)g_plugin.getByte(t, lParamSort + 1);

	g_plugin.setByte(t, (uint8_t)lParamSort);

	// restore sort order
	mir_snprintf(t, "os%02x", (UINT)lParamSort);
	int m = g_plugin.getByte(t, 0);
	if (bChangeSortOrder) { m = !m; g_plugin.setByte(t, m); }

	ListView_SortItems(hLV, &CompareFunc, lParamSort | (m << 8));
}

BOOL ShowSelectKeyDlg(HWND hParent, LPSTR KeyPath)
{
	OPENFILENAME ofn = { sizeof(ofn) };
	ofn.hwndOwner = hParent;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NONETWORKBUTTON;

	ofn.lpstrFile = KeyPath;
	char temp[MAX_PATH];
	mir_snprintf(temp, "%s (*.asc)%c*.asc%c%s (*.*)%c*.*%c%c", Translate("ASC files"), 0, 0, Translate("All files"), 0, 0, 0);
	ofn.lpstrFilter = temp;
	ofn.lpstrTitle = Translate("Open Key File");
	if (!GetOpenFileName(&ofn)) return FALSE;

	return TRUE;
}

LPCSTR priv_beg = "-----BEGIN PGP PRIVATE KEY BLOCK-----";
LPCSTR priv_end = "-----END PGP PRIVATE KEY BLOCK-----";
LPCSTR publ_beg = "-----BEGIN PGP PUBLIC KEY BLOCK-----";
LPCSTR publ_end = "-----END PGP PUBLIC KEY BLOCK-----";

LPSTR LoadKeys(LPCSTR file, BOOL priv)
{
	FILE *f = fopen(file, "r");
	if (!f) return nullptr;

	fseek(f, 0, SEEK_END);
	int flen = ftell(f);
	fseek(f, 0, SEEK_SET);

	LPCSTR beg, end;
	if (priv) {
		beg = priv_beg;
		end = priv_end;
	}
	else {
		beg = publ_beg;
		end = publ_end;
	}

	LPSTR keys = (LPSTR)mir_alloc(flen + 1);
	int i = 0; BOOL b = false;
	while (fgets(keys + i, 128, f)) {
		if (!b && strncmp(keys + i, beg, mir_strlen(beg)) == 0)
			b = true;
		else if (b && strncmp(keys + i, end, mir_strlen(end)) == 0) {
			i += (int)mir_strlen(keys + i);
			b = false;
		}
		if (b)
			i += (int)mir_strlen(keys + i);
	}
	*(keys + i) = '\0';
	fclose(f);
	return keys;
}

BOOL SaveExportRSAKeyDlg(HWND hParent, LPSTR key, BOOL priv)
{
	char szFile[MAX_PATH] = "rsa_pub.asc";
	if (priv)
		mir_strcpy(szFile, "rsa_priv.asc");

	OPENFILENAME ofn = { sizeof(ofn) };
	char temp[MAX_PATH];
	mir_snprintf(temp, "%s (*.asc)%c*.asc%c%s (*.*)%c*.*%c%c", Translate("ASC files"), 0, 0, Translate("All files"), 0, 0, 0);
	ofn.lpstrFilter = temp;
	ofn.hwndOwner = hParent;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_NOREADONLYRETURN | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;
	ofn.lpstrFile = szFile;

	ofn.lpstrTitle = (priv) ? Translate("Save Private Key File") : Translate("Save Public Key File");
	if (!GetSaveFileName(&ofn))
		return FALSE;

	FILE *f = fopen(szFile, "wb");
	if (!f)
		return FALSE;
	fwrite(key, mir_strlen(key), 1, f);
	fclose(f);

	return TRUE;
}

BOOL LoadImportRSAKeyDlg(HWND hParent, LPSTR key, BOOL priv)
{
	char szFile[MAX_PATH] = "rsa_pub.asc";
	if (priv)
		mir_strcpy(szFile, "rsa_priv.asc");

	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hParent;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NONETWORKBUTTON;
	ofn.lpstrFile = szFile;
	char temp[MAX_PATH];
	mir_snprintf(temp, "%s (*.asc)%c*.asc%c%s (*.*)%c*.*%c%c", Translate("ASC files"), 0, 0, Translate("All files"), 0, 0, 0);
	ofn.lpstrFilter = temp;
	ofn.lpstrTitle = (priv) ? Translate("Load Private Key File") : Translate("Load Public Key File");
	if (!GetOpenFileName(&ofn))
		return FALSE;

	FILE *f = fopen(szFile, "rb");
	if (!f)
		return FALSE;

	fseek(f, 0, SEEK_END);
	int flen = ftell(f);
	if (flen > RSASIZE) {
		fclose(f);
		return FALSE;
	}
	fseek(f, 0, SEEK_SET);

	fread(key, flen, 1, f);
	fclose(f);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// first options tab

static void ResetGeneralDlg(HWND hDlg)
{
	SetDlgItemText(hDlg, IDC_KET, "10");
	SetDlgItemText(hDlg, IDC_OKT, "2");

	CheckDlgButton(hDlg, IDC_SFT, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_SOM, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_ASI, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_MCD, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_SCM, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_DGP, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_AIP, BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_MCM, BST_UNCHECKED);

	// rebuild list of contacts
	HWND hLV = GetDlgItem(hDlg, IDC_STD_USERLIST);
	ListView_DeleteAllItems(hLV);

	LVITEMW lvi = {};
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;

	for (auto &hContact : Contacts()) {
		auto *pa = Proto_GetAccount(Proto_GetBaseAccountName(hContact));
		if (!pa)
			continue;

		if (!isSecureProtocol(hContact) || isChatRoom(hContact))
			continue;

		pUinKey ptr = getUinKey(hContact);
		if (!ptr)
			continue;

		ptr->tmode = MODE_NATIVE;
		ptr->tstatus = STATUS_ENABLED;

		lvi.iItem++;
		lvi.iImage = ptr->tstatus;
		lvi.lParam = (LPARAM)ptr;
		lvi.pszText = Clist_GetContactDisplayName(hContact);
		int itemNum = LV_InsertItem(hLV, &lvi);

		LV_SetItemText(hLV, itemNum, 1, pa->tszAccountName);

		ptrW uid(Contact::GetInfo(CNF_UNIQUEID, hContact, pa->szModuleName));
		LV_SetItemText(hLV, itemNum, 2, uid);

		setListViewMode(hLV, itemNum, ptr->tmode);
		setListViewStatus(hLV, itemNum, ptr->tstatus);
		if (ptr->mode == MODE_NATIVE)
			setListViewPSK(hLV, itemNum, 0);
		else
			setListViewPUB(hLV, itemNum, 0);
		setListViewIcon(hLV, itemNum, ptr);
	}
}

static void RefreshGeneralDlg(HWND hDlg, BOOL iInit)
{
	char timeout[10];

	// Key Exchange Timeout
	UINT data = g_plugin.getWord("ket", 10);
	mir_itoa(data, timeout, 10);
	SetDlgItemText(hDlg, IDC_KET, timeout);

	// Offline Key Timeout
	data = g_plugin.getWord("okt", 2);
	mir_itoa(data, timeout, 10);
	SetDlgItemText(hDlg, IDC_OKT, timeout);

	GetFlags();

	CheckDlgButton(hDlg, IDC_SFT, (bSFT) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_SOM, (bSOM) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_ASI, (bASI) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_MCD, (bMCD) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_SCM, (bSCM) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_DGP, (bDGP) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_AIP, (bAIP) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_NOL, (bNOL) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_AAK, (bAAK) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_MCM, (bMCM) ? BST_CHECKED : BST_UNCHECKED);

	// Select {OFF,PGP,GPG}
	CheckDlgButton(hDlg, IDC_PGP, bPGP ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_GPG, bGPG ? BST_CHECKED : BST_UNCHECKED);

	// rebuild list of contacts
	HWND hLV = GetDlgItem(hDlg, IDC_STD_USERLIST);
	ListView_DeleteAllItems(hLV);

	LVITEMW lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;

	for (auto &hContact : Contacts()) {
		auto *pa = Proto_GetAccount(Proto_GetBaseAccountName(hContact));
		if (!pa)
			continue;

		pUinKey ptr = getUinKey(hContact);
		if (ptr && isSecureProtocol(hContact) && !isChatRoom(hContact)) {
			if (iInit) {
				ptr->tmode = ptr->mode;
				ptr->tstatus = ptr->status;
			}

			lvi.iItem++;
			lvi.iImage = ptr->tstatus;
			lvi.lParam = (LPARAM)ptr;
			lvi.pszText = Clist_GetContactDisplayName(hContact);
			int itemNum = LV_InsertItem(hLV, &lvi);

			LV_SetItemText(hLV, itemNum, 1, pa->tszAccountName);

			ptrW uid(Contact::GetInfo(CNF_UNIQUEID, hContact, pa->szModuleName));
			LV_SetItemText(hLV, itemNum, 2, uid);

			setListViewMode(hLV, itemNum, ptr->tmode);
			setListViewStatus(hLV, itemNum, ptr->tstatus);
			if (ptr->mode == MODE_NATIVE)
				setListViewPSK(hLV, itemNum, hasKey(ptr));
			else
				setListViewPUB(hLV, itemNum, hasKey(ptr));
			setListViewIcon(hLV, itemNum, ptr);
		}
	}
	ListView_Sort(hLV, 0);
}

static INT_PTR CALLBACK DlgProcSetPSK(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static char *buffer;
	switch (uMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);
		SendDlgItemMessage(hDlg, IDC_EDIT1, EM_LIMITTEXT, PSKSIZE - 1, 0);
		SetDlgItemTextW(hDlg, IDC_EDIT2, (LPWSTR)lParam);
		buffer = (LPSTR)lParam;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (GetDlgItemTextA(hDlg, IDC_EDIT1, buffer, PSKSIZE) < 8) {
				msgbox(hDlg, LPGEN("Password is too short!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
				return TRUE;
			}
			EndDialog(hDlg, IDOK);
			break;

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void ApplyGeneralSettings(HWND hDlg)
{
	char timeout[5];
	int tmp, i;

	// Key Exchange Timeout
	GetDlgItemText(hDlg, IDC_KET, timeout, 5);
	tmp = atoi(timeout); if (tmp > 65535) tmp = 65535;
	g_plugin.setWord("ket", tmp);
	mir_exp->rsa_set_timeout(g_plugin.getWord("ket", 10));
	mir_itoa(tmp, timeout, 10);
	SetDlgItemText(hDlg, IDC_KET, timeout);

	// Offline Key Timeout
	GetDlgItemText(hDlg, IDC_OKT, timeout, 5);
	tmp = atoi(timeout); if (tmp > 65535) tmp = 65535;
	g_plugin.setWord("okt", tmp);
	mir_itoa(tmp, timeout, 10);
	SetDlgItemText(hDlg, IDC_OKT, timeout);

	bSFT = (IsDlgButtonChecked(hDlg, IDC_SFT) == BST_CHECKED);
	bSOM = (IsDlgButtonChecked(hDlg, IDC_SOM) == BST_CHECKED);
	bASI = (IsDlgButtonChecked(hDlg, IDC_ASI) == BST_CHECKED);
	bMCD = (IsDlgButtonChecked(hDlg, IDC_MCD) == BST_CHECKED);
	bSCM = (IsDlgButtonChecked(hDlg, IDC_SCM) == BST_CHECKED);
	bDGP = (IsDlgButtonChecked(hDlg, IDC_DGP) == BST_CHECKED);
	bAIP = (IsDlgButtonChecked(hDlg, IDC_AIP) == BST_CHECKED);
	bNOL = (IsDlgButtonChecked(hDlg, IDC_NOL) == BST_CHECKED);
	bAAK = (IsDlgButtonChecked(hDlg, IDC_AAK) == BST_CHECKED);
	bMCM = (IsDlgButtonChecked(hDlg, IDC_MCM) == BST_CHECKED);

	SetFlags();

	// PGP &| GPG flags
	tmp = 0;
	i = (IsDlgButtonChecked(hDlg, IDC_PGP) == BST_CHECKED);
	if (i != bPGP) {
		bPGP = i; tmp++;
		g_plugin.setByte("pgp", bPGP);
	}
	i = (IsDlgButtonChecked(hDlg, IDC_GPG) == BST_CHECKED);
	if (i != bGPG) {
		bGPG = i; tmp++;
		g_plugin.setByte("gpg", bGPG);
	}
	if (tmp)
		msgbox(hDlg, LPGEN("The new settings will become valid when you restart Miranda NG!"), MODULENAME, MB_OK | MB_ICONINFORMATION);

	HWND hLV = GetDlgItem(hDlg, IDC_STD_USERLIST);
	i = ListView_GetNextItem(hLV, (UINT)-1, LVNI_ALL);
	while (i != -1) {
		pUinKey ptr = (pUinKey)getListViewParam(hLV, i);
		if (!ptr) continue;
		if (ptr->mode != ptr->tmode) {
			ptr->mode = ptr->tmode;
			db_set_b(ptr->hContact, MODULENAME, "mode", ptr->mode);
		}
		if (ptr->status != ptr->tstatus) {
			ptr->status = ptr->tstatus;
			if (ptr->status == STATUS_ENABLED)	db_unset(ptr->hContact, MODULENAME, "StatusID");
			else 				db_set_b(ptr->hContact, MODULENAME, "StatusID", ptr->status);
		}
		if (ptr->mode == MODE_NATIVE) {
			if (getListViewPSK(hLV, i)) {
				LPSTR tmp = db_get_sa(ptr->hContact, MODULENAME, "tPSK");
				db_set_s(ptr->hContact, MODULENAME, "PSK", tmp);
				mir_free(tmp);
			}
			else db_unset(ptr->hContact, MODULENAME, "PSK");

			db_unset(ptr->hContact, MODULENAME, "tPSK");
		}
		else if (ptr->mode == MODE_RSAAES) {
			if (!getListViewPUB(hLV, i))
				db_unset(ptr->hContact, MODULENAME, "rsa_pub");
		}
		i = ListView_GetNextItem(hLV, i, LVNI_ALL);
	}
}

static INT_PTR CALLBACK DlgProcOptionsGeneral(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static int iInit = TRUE;
	static HIMAGELIST hLarge, hSmall;
	int i, idx; pUinKey ptr;

	HWND hLV = GetDlgItem(hDlg, IDC_STD_USERLIST);

	switch (wMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);

		iInit = TRUE;
		ListView_SetExtendedListViewStyle(hLV, ListView_GetExtendedListViewStyle(hLV) | LVS_EX_FULLROWSELECT);

		hLarge = ImageList_Create(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), iBmpDepth, 1, 1);
		hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), iBmpDepth, 1, 1);
		for (i = 0; i < ICO_CNT; i++) {
			ImageList_AddIcon(hSmall, g_hICO[i]);
			ImageList_AddIcon(hLarge, g_hICO[i]);
		}

		ListView_SetImageList(hLV, hSmall, LVSIL_SMALL);
		ListView_SetImageList(hLV, hLarge, LVSIL_NORMAL);
		{
			static const char *szColHdr[] = { LPGEN("Nickname"), LPGEN("Account"), LPGEN("User ID"), LPGEN("Mode"), LPGEN("Status"), "", "SHA-1" };
			static int iColWidth[] = { 150, 70, 70, 60, 55, 35, 330 };

			LVCOLUMN lvc;
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			for (i = 0; i < _countof(szColHdr); i++) {
				lvc.iSubItem = i;
				lvc.pszText = (LPSTR)szColHdr[i];
				lvc.cx = iColWidth[i];
				LV_InsertColumn(hLV, i, &lvc);
			}
		}

		RefreshGeneralDlg(hDlg, TRUE);
		EnableWindow(hLV, true);

		iInit = FALSE;
		return TRUE;

	case WM_DESTROY:
		ImageList_Destroy(hSmall);
		ImageList_Destroy(hLarge);
		break;

	case WM_PAINT:
		if (!iInit)
			InvalidateRect(hDlg, nullptr, FALSE);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_ALWAYS:
		case ID_ENABLED:
		case ID_DISABLED:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				ptr->tstatus = LOWORD(wParam) - ID_DISABLED;
				setListViewStatus(hLV, idx, ptr->tstatus);
				setListViewIcon(hLV, idx, ptr);
			}
			break;

		case ID_SIM_NATIVE:
		case ID_SIM_PGP:
		case ID_SIM_GPG:
		case ID_SIM_RSAAES:
		case ID_SIM_RSA:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				ptr->tmode = LOWORD(wParam) - ID_SIM_NATIVE;
				setListViewMode(hLV, idx, ptr->tmode);
				setListViewIcon(hLV, idx, ptr);
			}
			break;

		case ID_SETPSK:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				char *buffer = (LPSTR)alloca(PSKSIZE + 1);
				strncpy_s(buffer, PSKSIZE, _T2A(Clist_GetContactDisplayName(ptr->hContact)), _TRUNCATE);
				int res = DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_PSK), nullptr, DlgProcSetPSK, (LPARAM)buffer);
				if (res == IDOK) {
					setListViewPSK(hLV, idx, 1);
					db_set_s(ptr->hContact, MODULENAME, "tPSK", buffer);
				}
			}
			break;

		case ID_DELPSK:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				setListViewPSK(hLV, idx, 0);
				db_unset(ptr->hContact, MODULENAME, "tPSK");
			}
			break;

		case ID_DELPUBL:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				setListViewPUB(hLV, idx, 0);
			}
			break;

		case ID_EXPPUBL:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				if (!ptr->keyLoaded) {
					createRSAcntx(ptr);
					loadRSAkey(ptr);
				}
				if (ptr->keyLoaded) {
					LPSTR buffer = (LPSTR)alloca(RSASIZE);
					mir_exp->rsa_export_pubkey(ptr->cntx, buffer);
					if (!SaveExportRSAKeyDlg(hDlg, buffer, 0))
						msgbox(hDlg, LPGEN("Can't export RSA public key!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
				}
			}
			return TRUE;

		case ID_IMPPUBL:
			idx = ListView_GetSelectionMark(hLV);
			ptr = (pUinKey)getListViewParam(hLV, idx);
			if (ptr) {
				createRSAcntx(ptr);
				LPSTR pub = (LPSTR)alloca(RSASIZE);
				if (!LoadImportRSAKeyDlg(hDlg, pub, 0)) return TRUE;
				if (mir_exp->rsa_import_pubkey(ptr->cntx, pub)) {
					int len;
					mir_exp->rsa_get_pubkey(ptr->cntx, (uint8_t*)pub, &len);
					db_set_blob(ptr->hContact, MODULENAME, "rsa_pub", pub, len);

					setListViewPUB(hLV, idx, 1);
				}
				else msgbox(hDlg, LPGEN("Can't import RSA public key!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
			}
			return TRUE;

		case ID_UPDATE_CLIST:
			// iInit = TRUE;
			//	RefreshGeneralDlg(hDlg,FALSE);
			//	iInit = FALSE;
			return TRUE;

		case IDC_RESET:
			if (!iInit)
				ResetGeneralDlg(hDlg);
			break;

		case IDC_ADV8:
		case IDC_ADV7:
		case IDC_ADV6:
		case IDC_ADV5:
		case IDC_ADV4:
		case IDC_ADV3:
		case IDC_ADV2:
		case IDC_ADV1:
		case IDC_ADV0:
		case IDC_GPG:
		case IDC_PGP:
		case IDC_NO_PGP:
		case IDC_NOL:
		case IDC_AAK:
		case IDC_MCM:
		case IDC_AIP:
		case IDC_SOM:
		case IDC_SFT:
		case IDC_ASI:
		case IDC_MCD:
		case IDC_KET:
		case IDC_SCM:
		case IDC_DGP:
		case IDC_OKT:
			break;

		default:
			return FALSE;
		}
		if (!iInit)
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
				iInit = TRUE;
				ApplyGeneralSettings(hDlg);
				RefreshContactListIcons();
				iInit = FALSE;
			}
			break;

		case IDC_STD_USERLIST:
			switch (((LPNMHDR)lParam)->code) {
			case NM_DBLCLK:
				if (LPNMLISTVIEW(lParam)->iSubItem == 2) {
					idx = LPNMLISTVIEW(lParam)->iItem;
					ptr = (pUinKey)getListViewParam(hLV, idx);
					if (ptr) {
						ptr->tmode++;
						if (!bPGP && ptr->tmode == MODE_PGP) ptr->tmode++;
						if (!bGPG && ptr->tmode == MODE_GPG) ptr->tmode++;
						if (ptr->tmode >= MODE_CNT) ptr->tmode = MODE_NATIVE;
						setListViewMode(hLV, idx, ptr->tmode);
						setListViewIcon(hLV, idx, ptr);
						SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
					}
				}
				if (LPNMLISTVIEW(lParam)->iSubItem == 3) {
					idx = LPNMLISTVIEW(lParam)->iItem;
					ptr = (pUinKey)getListViewParam(hLV, idx);
					if (ptr) {
						ptr->tstatus++; if (ptr->tstatus > (ptr->tmode == MODE_RSAAES ? 1 : 2)) ptr->tstatus = 0;
						setListViewStatus(hLV, idx, ptr->tstatus);
						setListViewIcon(hLV, idx, ptr);
						SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
					}
				}
				break;

			case LVN_COLUMNCLICK:
				bChangeSortOrder = true;
				ListView_Sort(hLV, LPNMLISTVIEW(lParam)->iSubItem + 0x01);
				bChangeSortOrder = false;

			case NM_RCLICK:
				LPNMLISTVIEW lpLV = (LPNMLISTVIEW)lParam;
				ptr = (pUinKey)getListViewParam(hLV, lpLV->iItem);
				if (ptr) {
					POINT p; GetCursorPos(&p);
					HMENU hMenu = nullptr;
					if (ptr->tmode == MODE_NATIVE || ptr->tmode == MODE_RSAAES) {
						switch (lpLV->iSubItem) {
						case 2: // mode
							hMenu = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE(IDM_CLIST2));
							break;
						case 3: // status
							hMenu = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE((ptr->tmode == MODE_NATIVE) ? IDM_CLIST01 : IDM_CLIST11));
							break;
						case 4: // PSK/PUB
						case 5: // SHA-1
							hMenu = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE((ptr->tmode == MODE_NATIVE) ? IDM_CLIST02 : IDM_CLIST12));
							break;
						default: // full menu
							hMenu = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE((ptr->tmode == MODE_NATIVE) ? IDM_CLIST0 : IDM_CLIST1));
							break;
						}
						CheckMenuItem(hMenu, ID_DISABLED + ptr->tstatus, MF_CHECKED);
						if (ptr->tmode == MODE_NATIVE) {
							if (!hasKey(ptr)) EnableMenuItem(hMenu, ID_DELPSK, MF_GRAYED);
						}
						else if (ptr->tmode == MODE_RSAAES) {
							if (!hasKey(ptr)) {
								EnableMenuItem(hMenu, ID_EXPPUBL, MF_GRAYED);
								EnableMenuItem(hMenu, ID_DELPUBL, MF_GRAYED);
							}
						}
					}
					if (!hMenu)
						hMenu = LoadMenu(g_plugin.getInst(), MAKEINTRESOURCE(IDM_CLIST2));
					TranslateMenu(hMenu);
					CheckMenuItem(hMenu, ID_SIM_NATIVE + ptr->tmode, MF_CHECKED);
					if (!bPGP) EnableMenuItem(hMenu, ID_SIM_PGP, MF_GRAYED);
					if (!bGPG) EnableMenuItem(hMenu, ID_SIM_GPG, MF_GRAYED);
					TrackPopupMenu(GetSubMenu(hMenu, 0), TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hDlg, nullptr);
					DestroyMenu(hMenu);
				}
				break;
			}
			break;
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Accounts options tab

static INT_PTR CALLBACK DlgProcSetPassphrase(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static LPSTR buffer;

	switch (uMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);
		SendDlgItemMessage(hDlg, IDC_PASSPHRASE, EM_LIMITTEXT, RSASIZE - 1, 0);
		buffer = (LPSTR)lParam;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemTextA(hDlg, IDC_PASSPHRASE, buffer, RSASIZE);
			EndDialog(hDlg, IDOK);
			break;

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void RefreshProtoDlg(HWND hDlg)
{
	HWND hLV = GetDlgItem(hDlg, IDC_PROTO);
	ListView_DeleteAllItems(hLV);

	LVITEMW lvi = { 0 };
	lvi.mask = LVIF_TEXT | LVIF_PARAM;

	for (int i = 0; i < arProto.getCount(); i++) {
		auto *pa = Proto_GetAccount(arProto[i]->name);
		if (pa == nullptr)
			continue;

		lvi.iItem = i + 1;
		lvi.pszText = pa->tszAccountName;
		lvi.lParam = (LPARAM)i;
		int itemNum = LV_InsertItem(hLV, &lvi);
		ListView_SetCheckState(hLV, itemNum, arProto[i]->inspecting);
	}

	SetDlgItemText(hDlg, IDC_SPLITON, "0");
	SetDlgItemText(hDlg, IDC_SPLITOFF, "0");
	EnableWindow(GetDlgItem(hDlg, IDC_SPLITON), false);
	EnableWindow(GetDlgItem(hDlg, IDC_SPLITOFF), false);

	uint8_t sha[64]; int len; mir_exp->rsa_get_keyhash(CPP_MODE_RSA, nullptr, nullptr, (uint8_t*)&sha, &len);
	LPSTR txt = mir_strdup(to_hex(sha, len));
	SetDlgItemText(hDlg, IDC_RSA_SHA, txt);
	mir_free(txt);
}

static void ApplyProtoSettings(HWND hDlg)
{
	LPSTR szNames = (LPSTR)alloca(2048); *szNames = '\0';

	HWND hLV = GetDlgItem(hDlg, IDC_PROTO);
	int i = ListView_GetNextItem(hLV, (UINT)-1, LVNI_ALL);
	while (i != -1) {
		pSupPro p = arProto[getListViewProto(hLV, i)];
		p->inspecting = ListView_GetCheckState(hLV, i);
		char tmp[128];
		mir_snprintf(tmp, "%s:%d:%d:%d;", p->name, p->inspecting, p->tsplit_on, p->tsplit_off);
		mir_strcat(szNames, tmp);
		p->split_on = p->tsplit_on;
		p->split_off = p->tsplit_off;
		i = ListView_GetNextItem(hLV, i, LVNI_ALL);
	}

	g_plugin.setString("protos", szNames);
}

static INT_PTR CALLBACK DlgProcOptionsProto(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static int iInit = TRUE;
	char buf[32];
	int idx;

	HWND hLV = GetDlgItem(hDlg, IDC_PROTO);

	switch (wMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);

		iInit = TRUE;
		ListView_SetExtendedListViewStyle(hLV, ListView_GetExtendedListViewStyle(hLV) | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

		LVCOLUMN lvc;
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.fmt = LVCFMT_LEFT;
		lvc.pszText = LPGEN("Name");
		lvc.cx = 150;
		LV_InsertColumn(hLV, 0, &lvc);

		RefreshProtoDlg(hDlg);
		EnableWindow(hLV, true);

		iInit = FALSE;
		return TRUE;

	case WM_PAINT:
		if (!iInit)
			InvalidateRect(hDlg, nullptr, FALSE);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_RSA_COPY:
			wchar_t txt[128];
			GetDlgItemTextW(hDlg, IDC_RSA_SHA, txt, _countof(txt));
			Utils_ClipboardCopy(txt);
			return TRUE;

		case IDC_RSA_EXP:
			{
				LPSTR pub = (LPSTR)alloca(RSASIZE);
				mir_exp->rsa_export_keypair(CPP_MODE_RSA, nullptr, pub, nullptr);
				if (!SaveExportRSAKeyDlg(hDlg, pub, 0))
					msgbox(hDlg, LPGEN("Can't export RSA public key!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
			}
			return TRUE;

		case IDC_RSA_EXPPRIV:
			{
				LPSTR passphrase = (LPSTR)alloca(RSASIZE);
				int res = DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_PASSPHRASE), nullptr, DlgProcSetPassphrase, (LPARAM)passphrase);
				if (res == IDOK) {
					LPSTR priv = (LPSTR)alloca(RSASIZE);
					mir_exp->rsa_export_keypair(CPP_MODE_RSA, priv, nullptr, passphrase);
					if (!SaveExportRSAKeyDlg(hDlg, priv, 1))
						msgbox(hDlg, LPGEN("Can't export RSA private key!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
				}
			}
			return TRUE;

		case IDC_RSA_IMPPRIV:
			{
				LPSTR priv = (LPSTR)alloca(RSASIZE);
				if (!LoadImportRSAKeyDlg(hDlg, priv, 1))
					return TRUE;

				LPSTR passphrase = (LPSTR)alloca(RSASIZE);
				int res = DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_PASSPHRASE), nullptr, DlgProcSetPassphrase, (LPARAM)passphrase);
				if (res == IDOK) {
					if (!mir_exp->rsa_import_keypair(CPP_MODE_RSA, priv, passphrase))
						msgbox(hDlg, LPGEN("Can't import RSA private key!"), MODULENAME, MB_OK | MB_ICONEXCLAMATION);
					else // обновить SHA-1 значение
						RefreshProtoDlg(hDlg);
				}
			}
			return TRUE;

		case IDC_SPLITON:
		case IDC_SPLITOFF:
			if (HIWORD(wParam) == EN_CHANGE) {
				idx = ListView_GetSelectionMark(hLV);
				if (idx == -1)
					break;

				idx = getListViewParam(hLV, idx);
				switch (LOWORD(wParam)) {
				case IDC_SPLITON:
					GetDlgItemText(hDlg, IDC_SPLITON, buf, 5);
					arProto[idx]->tsplit_on = atoi(buf);
					break;
				case IDC_SPLITOFF:
					GetDlgItemText(hDlg, IDC_SPLITOFF, buf, 5);
					arProto[idx]->tsplit_off = atoi(buf);
					break;
				}
			}
			if (!iInit)
				SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			break;
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
				iInit = TRUE;
				ApplyProtoSettings(hDlg);
				RefreshProtoDlg(hDlg);
				RefreshContactListIcons();
				SendMessage(GetParent(hDlg), WM_COMMAND, ID_UPDATE_CLIST, 0);
				iInit = FALSE;
			}
			break;

		case IDC_PROTO:
			if (((LPNMHDR)lParam)->code == (UINT)NM_CLICK) {
				idx = (int)getListViewParam(hLV, LPNMLISTVIEW(lParam)->iItem);
				if (idx == -1)
					break;

				EnableWindow(GetDlgItem(hDlg, IDC_SPLITON), true);
				EnableWindow(GetDlgItem(hDlg, IDC_SPLITOFF), true);
				mir_itoa(arProto[idx]->tsplit_on, buf, 10);	SetDlgItemText(hDlg, IDC_SPLITON, buf);
				mir_itoa(arProto[idx]->tsplit_off, buf, 10);	SetDlgItemText(hDlg, IDC_SPLITOFF, buf);
				SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
			}
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// PGP options

static BOOL bPGP9;

static void OnChange_KeyRings(HWND hDlg)
{
	BOOL bNoKR = (IsDlgButtonChecked(hDlg, IDC_NO_KEYRINGS) == BST_CHECKED);
	EnableWindow(GetDlgItem(hDlg, IDC_SET_KEYRINGS), !bNoKR);
	EnableWindow(GetDlgItem(hDlg, IDC_LOAD_PRIVKEY), bNoKR);
	if (bNoKR)
		SetDlgItemTextW(hDlg, IDC_KEYRING_STATUS, TranslateT("Keyrings disabled!"));
	else if (bPGP9)
		SetDlgItemTextW(hDlg, IDC_KEYRING_STATUS, TranslateT("This version not supported!"));
	else
		SetDlgItemTextW(hDlg, IDC_KEYRING_STATUS, bPGPkeyrings ? TranslateT("Keyrings loaded.") : TranslateT("Keyrings not loaded!"));
}

static void RefreshPGPDlg(HWND hDlg, BOOL iInit)
{
	int ver = pgp_get_version();
	bPGP9 = (ver >= 0x03050000);

	SetDlgItemTextW(hDlg, IDC_PGP_PRIVKEY, bPGPprivkey ? TranslateT("Private key loaded.") : TranslateT("Private key not loaded!"));

	if (bPGPloaded && ver) {
		wchar_t pgpVerStr[64];
		mir_snwprintf(pgpVerStr, TranslateT("PGP SDK v%i.%i.%i found."), ver >> 24, (ver >> 16) & 255, (ver >> 8) & 255);
		SetDlgItemTextW(hDlg, IDC_PGP_SDK, pgpVerStr);
	}
	else SetDlgItemTextW(hDlg, IDC_PGP_SDK, TranslateT("PGP SDK not found!"));

	// Disable keyrings use
	CheckDlgButton(hDlg, IDC_NO_KEYRINGS, (bUseKeyrings) ? BST_UNCHECKED : BST_CHECKED);
	OnChange_KeyRings(hDlg);

	// rebuild list of contacts
	HWND hLV = GetDlgItem(hDlg, IDC_PGP_USERLIST);
	ListView_DeleteAllItems(hLV);

	LVITEMW lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;

	wchar_t tmp[NAMSIZE];

	for (auto &hContact : Contacts()) {
		pUinKey ptr = getUinKey(hContact);
		if (ptr && ptr->mode == MODE_PGP && isSecureProtocol(hContact) && !isChatRoom(hContact)) {
			LPSTR szKeyID = g_plugin.getStringA(hContact, "pgp_abbr");

			lvi.iItem++;
			lvi.iImage = (szKeyID != nullptr);
			lvi.lParam = (LPARAM)ptr;
			lvi.pszText = Clist_GetContactDisplayName(hContact);
			int itemNum = LV_InsertItem(hLV, &lvi);

			getContactUin(hContact, tmp);
			LV_SetItemText(hLV, itemNum, 1, tmp);

			LV_SetItemTextA(hLV, itemNum, 2, (szKeyID) ? szKeyID : Translate("(none)"));
			SAFE_FREE(szKeyID);
		}
	}
	ListView_Sort(hLV, (LPARAM)0x10);
}

static void ApplyPGPSettings(HWND hDlg)
{
	bUseKeyrings = !(IsDlgButtonChecked(hDlg, IDC_NO_KEYRINGS) == BST_CHECKED);
	g_plugin.setByte("ukr", bUseKeyrings);

	char *priv = g_plugin.getStringA("tpgpPrivKey");
	if (priv) {
		bPGPprivkey = true;
		pgp_set_priv_key(priv);
		g_plugin.setString("pgpPrivKey", priv);
		mir_free(priv);
		g_plugin.delSetting("tpgpPrivKey");
	}
}

static INT_PTR CALLBACK DlgProcOptionsPGP(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static int iInit = TRUE;
	static HIMAGELIST hLarge, hSmall;
	int i;

	HWND hLV = GetDlgItem(hDlg, IDC_PGP_USERLIST);

	switch (wMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);

		iInit = TRUE;
		ListView_SetExtendedListViewStyle(hLV, ListView_GetExtendedListViewStyle(hLV) | LVS_EX_FULLROWSELECT);

		hLarge = ImageList_Create(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), iBmpDepth, 1, 1);
		hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), iBmpDepth, 1, 1);
		for (i = ICO_ST_DIS; i <= ICO_ST_TRY; i++) {
			ImageList_AddIcon(hSmall, g_hICO[i]);
			ImageList_AddIcon(hLarge, g_hICO[i]);
		}

		ListView_SetImageList(hLV, hSmall, LVSIL_SMALL);
		ListView_SetImageList(hLV, hLarge, LVSIL_NORMAL);
		{
			static const char *szColHdr[] = { LPGEN("Nickname"), LPGEN("User ID"), LPGEN("Key ID") };
			static int iColWidth[] = { 160, 150, 80 };
			LVCOLUMN lvc;
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			for (i = 0; _countof(szColHdr); i++) {
				lvc.iSubItem = i;
				lvc.pszText = (LPSTR)szColHdr[i];
				lvc.cx = iColWidth[i];
				LV_InsertColumn(hLV, i, &lvc);
			}
		}
		RefreshPGPDlg(hDlg, TRUE);
		iInit = FALSE;
		return TRUE;

	case WM_DESTROY:
		ImageList_Destroy(hSmall);
		ImageList_Destroy(hLarge);
		break;

	case WM_PAINT:
		if (!iInit)
			InvalidateRect(hDlg, nullptr, FALSE);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SET_KEYRINGS:
			{
				char PubRingPath[MAX_PATH], SecRingPath[MAX_PATH];
				PubRingPath[0] = '\0'; SecRingPath[0] = '\0';
				bPGPkeyrings = pgp_open_keyrings(PubRingPath, SecRingPath);
				if (bPGPkeyrings && PubRingPath[0] && SecRingPath[0]) {
					g_plugin.setString("pgpPubRing", PubRingPath);
					g_plugin.setString("pgpSecRing", SecRingPath);
				}
				SetDlgItemText(hDlg, IDC_KEYRING_STATUS, bPGPkeyrings ? Translate("Keyrings loaded.") : Translate("Keyrings not loaded!"));
			}
			return FALSE;

		case IDC_NO_KEYRINGS:
			OnChange_KeyRings(hDlg);
			break;

		case IDC_LOAD_PRIVKEY:
			{
				char KeyPath[MAX_PATH]; KeyPath[0] = '\0';
				if (ShowSelectKeyDlg(hDlg, KeyPath)) {
					char *priv = LoadKeys(KeyPath, true);
					if (priv) {
						g_plugin.setString("tpgpPrivKey", priv);
						mir_free(priv);
					}
					else {
						g_plugin.delSetting("tpgpPrivKey");
					}
				}
			}
			break;

		case ID_UPDATE_PLIST:
			iInit = TRUE;
			RefreshPGPDlg(hDlg, FALSE);
			iInit = FALSE;
			return TRUE;
		}
		if (!iInit)
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
				iInit = TRUE;
				ApplyPGPSettings(hDlg);
				RefreshPGPDlg(hDlg, FALSE);
				iInit = FALSE;
			}
			break;

		case IDC_PGP_USERLIST:
			switch (((LPNMHDR)lParam)->code) {
			case LVN_COLUMNCLICK:
				bChangeSortOrder = true;
				ListView_Sort(hLV, LPNMLISTVIEW(lParam)->iSubItem + 0x11);
				bChangeSortOrder = false;
				break;
			}
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// GPG options

static void RefreshGPGDlg(HWND hDlg, BOOL iInit)
{
	LPSTR path = g_plugin.getStringA("gpgExec");
	if (path) {
		SetDlgItemText(hDlg, IDC_GPGEXECUTABLE_EDIT, path);
		mir_free(path);
	}
	path = g_plugin.getStringA("gpgHome");
	if (path) {
		SetDlgItemText(hDlg, IDC_GPGHOME_EDIT, path);
		mir_free(path);
	}
	BOOL bGPGLogFlag = g_plugin.getByte("gpgLogFlag", 0);
	CheckDlgButton(hDlg, IDC_LOGGINGON_CBOX, (bGPGLogFlag) ? BST_CHECKED : BST_UNCHECKED);
	path = g_plugin.getStringA("gpgLog");
	if (path) {
		SetDlgItemText(hDlg, IDC_GPGLOGFILE_EDIT, path);
		mir_free(path);
	}
	CheckDlgButton(hDlg, IDC_SAVEPASS_CBOX, (bSavePass) ? BST_CHECKED : BST_UNCHECKED);
	BOOL bGPGTmpFlag = g_plugin.getByte("gpgTmpFlag", 0);
	CheckDlgButton(hDlg, IDC_TMPPATHON_CBOX, (bGPGTmpFlag) ? BST_CHECKED : BST_UNCHECKED);
	path = g_plugin.getStringA("gpgTmp");
	if (path) {
		SetDlgItemText(hDlg, IDC_GPGTMPPATH_EDIT, path);
		mir_free(path);
	}

	// rebuild list of contacts
	HWND hLV = GetDlgItem(hDlg, IDC_GPG_USERLIST);
	ListView_DeleteAllItems(hLV);

	LVITEMW lvi; memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;

	wchar_t tmp[NAMSIZE];

	for (auto &hContact : Contacts()) {
		pUinKey ptr = getUinKey(hContact);
		if (ptr && ptr->mode == MODE_GPG && isSecureProtocol(hContact) && !isChatRoom(hContact)) {
			if (iInit)
				ptr->tgpgMode = ptr->gpgMode;

			ptrW szKeyID(g_plugin.getWStringA(hContact, "gpg"));

			lvi.iItem++;
			lvi.iImage = (szKeyID != nullptr);
			lvi.lParam = (LPARAM)ptr;
			lvi.pszText = Clist_GetContactDisplayName(hContact);
			int itemNum = LV_InsertItem(hLV, &lvi);

			getContactUin(hContact, tmp);
			LV_SetItemText(hLV, itemNum, 1, tmp);

			LV_SetItemText(hLV, itemNum, 2, (szKeyID) ? szKeyID : TranslateT("(none)"));
			LV_SetItemText(hLV, itemNum, 3, (ptr->tgpgMode) ? TranslateT("ANSI") : TranslateT("UTF-8"));
			SAFE_FREE(szKeyID);
		}
	}
	ListView_Sort(hLV, (LPARAM)0x20);
}

static void ApplyGPGSettings(HWND hDlg)
{
	char tmp[256];

	GetDlgItemText(hDlg, IDC_GPGEXECUTABLE_EDIT, tmp, _countof(tmp));
	g_plugin.setString("gpgExec", tmp);
	GetDlgItemText(hDlg, IDC_GPGHOME_EDIT, tmp, _countof(tmp));
	g_plugin.setString("gpgHome", tmp);

	bSavePass = (IsDlgButtonChecked(hDlg, IDC_SAVEPASS_CBOX) == BST_CHECKED);
	g_plugin.setByte("gpgSaveFlag", bSavePass);

	BOOL bgpgLogFlag = (IsDlgButtonChecked(hDlg, IDC_LOGGINGON_CBOX) == BST_CHECKED);
	g_plugin.setByte("gpgLogFlag", bgpgLogFlag);
	GetDlgItemText(hDlg, IDC_GPGLOGFILE_EDIT, tmp, _countof(tmp));
	g_plugin.setString("gpgLog", tmp);
	if (bgpgLogFlag)	gpg_set_log(tmp);
	else gpg_set_log(nullptr);

	BOOL bgpgTmpFlag = (IsDlgButtonChecked(hDlg, IDC_TMPPATHON_CBOX) == BST_CHECKED);
	g_plugin.setByte("gpgTmpFlag", bgpgTmpFlag);
	GetDlgItemText(hDlg, IDC_GPGTMPPATH_EDIT, tmp, _countof(tmp));
	g_plugin.setString("gpgTmp", tmp);
	if (bgpgTmpFlag)	gpg_set_tmp(tmp);
	else gpg_set_tmp(nullptr);

	HWND hLV = GetDlgItem(hDlg, IDC_GPG_USERLIST);
	int i = ListView_GetNextItem(hLV, (UINT)-1, LVNI_ALL);
	while (i != -1) {
		pUinKey ptr = (pUinKey)getListViewParam(hLV, i);
		if (!ptr) continue;
		if (ptr->gpgMode != ptr->tgpgMode) {
			ptr->gpgMode = ptr->tgpgMode;
			if (ptr->gpgMode)	db_set_b(ptr->hContact, MODULENAME, "gpgANSI", 1);
			else              	db_unset(ptr->hContact, MODULENAME, "gpgANSI");
		}

		i = ListView_GetNextItem(hLV, i, LVNI_ALL);
	}
}

static INT_PTR CALLBACK DlgProcOptionsGPG(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	static int iInit = TRUE;
	static HIMAGELIST hLarge, hSmall;
	int i, idx; pUinKey ptr;

	HWND hLV = GetDlgItem(hDlg, IDC_GPG_USERLIST);

	switch (wMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);

		iInit = TRUE;
		ListView_SetExtendedListViewStyle(hLV, ListView_GetExtendedListViewStyle(hLV) | LVS_EX_FULLROWSELECT);

		hLarge = ImageList_Create(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), iBmpDepth, 1, 1);
		hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), iBmpDepth, 1, 1);
		for (i = ICO_ST_DIS; i <= ICO_ST_TRY; i++) {
			ImageList_AddIcon(hSmall, g_hICO[i]);
			ImageList_AddIcon(hLarge, g_hICO[i]);
		}

		ListView_SetImageList(hLV, hSmall, LVSIL_SMALL);
		ListView_SetImageList(hLV, hLarge, LVSIL_NORMAL);
		{
			static const char *szColHdr[] = { LPGEN("Nickname"), LPGEN("User ID"), LPGEN("Key ID"), "CP" };
			static int iColWidth[] = { 140, 120, 120, 40 };
			LVCOLUMN lvc;
			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			lvc.fmt = LVCFMT_LEFT;
			for (i = 0; i < _countof(szColHdr); i++) {
				lvc.iSubItem = i;
				lvc.pszText = (LPSTR)szColHdr[i];
				lvc.cx = iColWidth[i];
				LV_InsertColumn(hLV, i, &lvc);
			}
		}

		RefreshGPGDlg(hDlg, TRUE);
		iInit = FALSE;
		return TRUE;

	case WM_DESTROY:
		ImageList_Destroy(hSmall);
		ImageList_Destroy(hLarge);
		break;

	case WM_PAINT:
		if (!iInit)
			InvalidateRect(hDlg, nullptr, FALSE);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BROWSEEXECUTABLE_BTN:
			{
				char gpgexe[256];
				char filter[128];
				GetDlgItemText(hDlg, IDC_GPGEXECUTABLE_EDIT, gpgexe, _countof(gpgexe));

				// filter zusammensetzen
				mir_snprintf(filter, "%s (*.exe)%c*.exe%c%c", Translate("Executable Files"), 0, 0, 0);

				// OPENFILENAME initialisieren
				OPENFILENAME ofn = { sizeof(ofn) };
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = filter;
				ofn.lpstrFile = gpgexe;
				ofn.nMaxFile = _countof(gpgexe);
				ofn.lpstrTitle = Translate("Select GnuPG Executable");
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_HIDEREADONLY;

				if (GetOpenFileName(&ofn))
					SetDlgItemText(hDlg, IDC_GPGEXECUTABLE_EDIT, ofn.lpstrFile);
			}
			break;

		case ID_UPDATE_GLIST:
			iInit = TRUE;
			RefreshGPGDlg(hDlg, FALSE);
			iInit = FALSE;
			return TRUE;
		}
		if (!iInit)
			SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
				iInit = TRUE;
				ApplyGPGSettings(hDlg);
				RefreshGPGDlg(hDlg, FALSE);
				iInit = FALSE;
			}
			break;

		case IDC_GPG_USERLIST:
			switch (((LPNMHDR)lParam)->code) {
			case NM_DBLCLK:
				if (LPNMLISTVIEW(lParam)->iSubItem == 3) {
					idx = LPNMLISTVIEW(lParam)->iItem;
					ptr = (pUinKey)getListViewParam(hLV, idx);
					if (!ptr) break;
					ptr->tgpgMode++; ptr->tgpgMode &= 1;
					LV_SetItemText(hLV, LPNMLISTVIEW(lParam)->iItem, LPNMLISTVIEW(lParam)->iSubItem, (ptr->tgpgMode) ? TranslateT("ANSI") : TranslateT("UTF-8"));
					SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
				}
				break;

			case LVN_COLUMNCLICK:
				bChangeSortOrder = true;
				ListView_Sort(hLV, LPNMLISTVIEW(lParam)->iSubItem + 0x21);
				bChangeSortOrder = false;
			}
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Module entry point

int onRegisterOptions(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.szTitle.a = MODULENAME;

	odp.szGroup.a = LPGEN("Popups");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_POPUP);
	odp.pfnDlgProc = PopOptionsDlgProc;
	g_plugin.addOptions(wParam, &odp);

	odp.szGroup.a = LPGEN("Services");
	odp.szTab.a = LPGEN("General");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_TAB_GENERAL);
	odp.pfnDlgProc = DlgProcOptionsGeneral;
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Accounts");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_TAB_PROTO);
	odp.pfnDlgProc = DlgProcOptionsProto;
	g_plugin.addOptions(wParam, &odp);

	if (bPGP && bPGPloaded) {
		odp.szTab.a = "PGP";
		odp.pszTemplate = MAKEINTRESOURCE(IDD_TAB_PGP);
		odp.pfnDlgProc = DlgProcOptionsPGP;
		g_plugin.addOptions(wParam, &odp);
	}

	if (bGPG && bGPGloaded) {
		odp.szTab.a = "GPG";
		odp.pszTemplate = MAKEINTRESOURCE(IDD_TAB_GPG);
		odp.pfnDlgProc = DlgProcOptionsGPG;
		g_plugin.addOptions(wParam, &odp);
	}
	return 0;
}
