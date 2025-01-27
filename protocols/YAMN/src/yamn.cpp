/*
 * This code implements miscellaneous usefull functions
 *
 * (c) majvan 2002-2004
 */

#include "stdafx.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

//Plugin registration CS
//Used if we add (register) plugin to YAMN plugins and when we browse through registered plugins
mir_cs PluginRegCS;

//AccountWriterCS
//We want to store number of writers of Accounts (number of Accounts used for writing)
//If we want to read all accounts (for saving to file) immidiatelly, we have to wait until no account is changing (no thread writing to account)
SCOUNTER *AccountWriterSO;

//NoExitEV
//Event that is signaled when there's a request to exit, so no new pop3 check should be performed
HANDLE ExitEV;

//WriteToFileEV
//If this is signaled, write accounts to file is performed. Set this event if you want to actualize your accounts and messages
HANDLE WriteToFileEV;

//Returns pointer to YAMN exported function
INT_PTR GetFcnPtrSvc(WPARAM wParam, LPARAM lParam);

//Returns pointer to YAMN variables
INT_PTR GetVariablesSvc(WPARAM wParam, LPARAM);

// Function every seconds decrements account counter of seconds and checks if they are 0
// If yes, creates a POP3 thread to check account
void CALLBACK TimerProc(HWND, UINT, UINT, uint32_t);

// Function called to check all accounts immidialtelly
// no params
INT_PTR ForceCheckSvc(WPARAM, LPARAM);

//thread is running all the time
//waits for WriteToFileEV and then writes all accounts to file
//uint32_t WINAPI FileWritingThread(PVOID);

// Function is called when Miranda notifies plugin that it is about to exit
// Ensures succesfull end of POP3 checking, sets event that no next checking should be performed
// If there's no writer to account (POP3 thread), saves the results to the file
//not used now, perhaps in the future


//int ExitProc(WPARAM wParam, LPARAM lParam);

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

INT_PTR GetFcnPtrSvc(WPARAM wParam, LPARAM)
{
	int i;

	for (i=0;i<sizeof(ProtoPluginExportedFcn)/sizeof(ProtoPluginExportedFcn[0]);i++)
		if (0==mir_strcmp((char *)wParam, ProtoPluginExportedFcn[i].ID))
			return (INT_PTR)ProtoPluginExportedFcn[i].Ptr;
	for (i=0;i<sizeof(ProtoPluginExportedSvc)/sizeof(ProtoPluginExportedSvc[0]);i++)
		if (0==mir_strcmp((char *)wParam, ProtoPluginExportedSvc[i].ID))
			return (INT_PTR)ProtoPluginExportedSvc[i].Ptr;
	for (i=0;i<sizeof(SynchroExportedFcn)/sizeof(SynchroExportedFcn[0]);i++)
		if (0==mir_strcmp((char *)wParam, SynchroExportedFcn[i].ID))
			return (INT_PTR)SynchroExportedFcn[i].Ptr;
	for (i=0;i<sizeof(AccountExportedFcn)/sizeof(AccountExportedFcn[0]);i++)
		if (0==mir_strcmp((char *)wParam, AccountExportedFcn[i].ID))
			return (INT_PTR)AccountExportedFcn[i].Ptr;
	for (i=0;i<sizeof(AccountExportedSvc)/sizeof(AccountExportedSvc[0]);i++)
		if (0==mir_strcmp((char *)wParam, AccountExportedSvc[i].ID))
			return (INT_PTR)AccountExportedSvc[i].Ptr;
	for (i=0;i<sizeof(MailExportedFcn)/sizeof(MailExportedFcn[0]);i++)
		if (0==mir_strcmp((char *)wParam, MailExportedFcn[i].ID))
			return (INT_PTR)MailExportedFcn[i].Ptr;
	for (i=0;i<sizeof(MailExportedSvc)/sizeof(MailExportedSvc[0]);i++)
		if (0==mir_strcmp((char *)wParam, MailExportedSvc[i].ID))
			return (INT_PTR)MailExportedSvc[i].Ptr;
	for (i=0;i<sizeof(FilterPluginExportedFcn)/sizeof(FilterPluginExportedFcn[0]);i++)
		if (0==mir_strcmp((char *)wParam, FilterPluginExportedFcn[i].ID))
			return (INT_PTR)FilterPluginExportedFcn[i].Ptr;
	for (i=0;i<sizeof(FilterPluginExportedSvc)/sizeof(FilterPluginExportedSvc[0]);i++)
		if (0==mir_strcmp((char *)wParam, FilterPluginExportedSvc[i].ID))
			return (INT_PTR)FilterPluginExportedSvc[i].Ptr;
	return (INT_PTR)NULL;
}

INT_PTR GetVariablesSvc(WPARAM wParam, LPARAM)
{
	return wParam==YAMN_VARIABLESVERSION ? (INT_PTR)&YAMNVar : (INT_PTR)NULL;
}

void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	CAccount *ActualAccount;
	DWORD Status, tid;

	//	we use event to signal, that running thread has all needed stack parameters copied
	HANDLE ThreadRunningEV = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (ThreadRunningEV == nullptr)
		return;

	//	if we want to close miranda, we get event and do not run checking anymore
	if (WAIT_OBJECT_0 == WaitForSingleObject(ExitEV, 0))
		return;

	//	Get actual status of current user in Miranda
	Status = CallService(MS_CLIST_GETSTATUSMODE, 0, 0);

	mir_cslock lck(PluginRegCS);
	for (PYAMN_PROTOPLUGINQUEUE ActualPlugin = FirstProtoPlugin; ActualPlugin != nullptr; ActualPlugin = ActualPlugin->Next) {
		if (WAIT_OBJECT_0 != SWMRGWaitToRead(ActualPlugin->Plugin->AccountBrowserSO, 0))  // we want to access accounts immiadtelly
			return;

		for (ActualAccount = ActualPlugin->Plugin->FirstAccount; ActualAccount != nullptr; ActualAccount = ActualAccount->Next) {
			if (ActualAccount->Plugin == nullptr || ActualAccount->Plugin->Fcn == nullptr)		//account not inited
				continue;

			if (WAIT_OBJECT_0 != SWMRGWaitToRead(ActualAccount->AccountAccessSO, 0))
				continue;

			BOOL isAccountCounting = 0;
			if ((ActualAccount->Flags & YAMN_ACC_ENA) &&
				(((ActualAccount->StatusFlags & YAMN_ACC_ST0) && (Status <= ID_STATUS_OFFLINE)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST1) && (Status == ID_STATUS_ONLINE)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST2) && (Status == ID_STATUS_AWAY)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST3) && (Status == ID_STATUS_DND)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST4) && (Status == ID_STATUS_NA)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST5) && (Status == ID_STATUS_OCCUPIED)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST6) && (Status == ID_STATUS_FREECHAT)) ||
					((ActualAccount->StatusFlags & YAMN_ACC_ST7) && (Status == ID_STATUS_INVISIBLE))))
			{
				if ((!ActualAccount->Interval && !ActualAccount->TimeLeft) || ActualAccount->Plugin->Fcn->TimeoutFcnPtr == nullptr)
					goto ChangeIsCountingStatusLabel;

				if (ActualAccount->TimeLeft) {
					ActualAccount->TimeLeft--;
					isAccountCounting = TRUE;
				}

				WindowList_BroadcastAsync(YAMNVar.MessageWnds, WM_YAMN_CHANGETIME, (WPARAM)ActualAccount, (LPARAM)ActualAccount->TimeLeft);
				if (!ActualAccount->TimeLeft) {
					struct CheckParam ParamToPlugin = {YAMN_CHECKVERSION, ThreadRunningEV, ActualAccount, YAMN_NORMALCHECK, (void *)nullptr, nullptr};

					ActualAccount->TimeLeft = ActualAccount->Interval;
					HANDLE NewThread = CreateThread(nullptr, 0, (YAMN_STANDARDFCN)ActualAccount->Plugin->Fcn->TimeoutFcnPtr, &ParamToPlugin, 0, &tid);
					if (NewThread == nullptr) {
						ReadDoneFcn(ActualAccount->AccountAccessSO);
						continue;
					}
					else {
						WaitForSingleObject(ThreadRunningEV, INFINITE);
						CloseHandle(NewThread);
					}
				}
			}

ChangeIsCountingStatusLabel:
			if (((ActualAccount->isCounting) != 0) != isAccountCounting) {
				ActualAccount->isCounting = isAccountCounting;
				uint16_t cStatus = g_plugin.getWord(ActualAccount->hContact, "Status");
				switch (cStatus) {
				case ID_STATUS_ONLINE:
				case ID_STATUS_OFFLINE:
					g_plugin.setWord(ActualAccount->hContact, "Status", isAccountCounting ? ID_STATUS_ONLINE : ID_STATUS_OFFLINE);
				default:
					break;
				}
			}
			ReadDoneFcn(ActualAccount->AccountAccessSO);
		}
		SWMRGDoneReading(ActualPlugin->Plugin->AccountBrowserSO);
	}
	CloseHandle(ThreadRunningEV);
}

INT_PTR ForceCheckSvc(WPARAM, LPARAM)
{
	CAccount *ActualAccount;
	DWORD tid;

	//we use event to signal, that running thread has all needed stack parameters copied
	HANDLE ThreadRunningEV = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (ThreadRunningEV == nullptr)
		return 0;
	//if we want to close miranda, we get event and do not run pop3 checking anymore
	if (WAIT_OBJECT_0 == WaitForSingleObject(ExitEV, 0))
		return 0;

	{
		mir_cslock lck(PluginRegCS);
		for (PYAMN_PROTOPLUGINQUEUE ActualPlugin = FirstProtoPlugin; ActualPlugin != nullptr; ActualPlugin = ActualPlugin->Next) {
			SWMRGWaitToRead(ActualPlugin->Plugin->AccountBrowserSO, INFINITE);
			for (ActualAccount = ActualPlugin->Plugin->FirstAccount; ActualAccount != nullptr; ActualAccount = ActualAccount->Next) {
				if (ActualAccount->Plugin->Fcn == nullptr)		//account not inited
					continue;

				if (WAIT_OBJECT_0 != WaitToReadFcn(ActualAccount->AccountAccessSO))
					continue;

				if ((ActualAccount->Flags & YAMN_ACC_ENA) && (ActualAccount->StatusFlags & YAMN_ACC_FORCE)) { //account cannot be forced to check
					if (ActualAccount->Plugin->Fcn->ForceCheckFcnPtr == nullptr) {
						ReadDoneFcn(ActualAccount->AccountAccessSO);
						continue;
					}
					struct CheckParam ParamToPlugin = { YAMN_CHECKVERSION, ThreadRunningEV, ActualAccount, YAMN_FORCECHECK, (void *)nullptr, nullptr };

					if (nullptr == CreateThread(nullptr, 0, (YAMN_STANDARDFCN)ActualAccount->Plugin->Fcn->ForceCheckFcnPtr, &ParamToPlugin, 0, &tid)) {
						ReadDoneFcn(ActualAccount->AccountAccessSO);
						continue;
					}
					else
						WaitForSingleObject(ThreadRunningEV, INFINITE);
				}
				ReadDoneFcn(ActualAccount->AccountAccessSO);
			}
			SWMRGDoneReading(ActualPlugin->Plugin->AccountBrowserSO);
		}
	}

	CloseHandle(ThreadRunningEV);

	if (hTTButton)
		CallService(MS_TTB_SETBUTTONSTATE, (WPARAM)hTTButton, 0);
	return 1;
}
