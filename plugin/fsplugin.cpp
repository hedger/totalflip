// fsplugin.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "fsplugin.h"
#include "cunicode.h"

#define pluginrootlen 1

HANDLE hinst;
char inifilename[MAX_PATH] = "fsplugin.ini";  // Unused in this plugin, may be used to save data

char* strlcpy(char* p, char* p2, int maxlen)
{
	if ((int)strlen(p2) >= maxlen) {
		strncpy(p, p2, maxlen);
		p[maxlen] = 0;
	}
	else
		strcpy(p, p2);
	return p;
}

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		hinst = hModule;
	return TRUE;
}

int PluginNumber;
tProgressProc ProgressProc = NULL;
tLogProc LogProc = NULL;
tRequestProc RequestProc = NULL;
tProgressProcW ProgressProcW = NULL;
tLogProcW LogProcW = NULL;
tRequestProcW RequestProcW = NULL;

int __stdcall FsInit(int PluginNr, tProgressProc pProgressProc, tLogProc pLogProc, tRequestProc pRequestProc)
{
	ProgressProc = pProgressProc;
	LogProc = pLogProc;
	RequestProc = pRequestProc;
	PluginNumber = PluginNr;
	return 0;
}

int __stdcall FsInitW(int PluginNr, tProgressProcW pProgressProcW, tLogProcW pLogProcW, tRequestProcW pRequestProcW)
{
	ProgressProcW = pProgressProcW;
	LogProcW = pLogProcW;
	RequestProcW = pRequestProcW;
	PluginNumber = PluginNr;
	return 0;
}

typedef struct {
	WCHAR PathW[wdirtypemax];
	WCHAR LastFoundNameW[wdirtypemax];
	HANDLE searchhandle;
} tLastFindStuct, * pLastFindStuct;

HANDLE __stdcall FsFindFirstW(WCHAR* Path, WIN32_FIND_DATAW* FindData)
{
	WCHAR buf[wdirtypemax];
	pLastFindStuct lf;

	memset(FindData, 0, sizeof(WIN32_FIND_DATAW));
	if (wcscmp(Path, L"\\") == 0) {
		FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		FindData->ftLastWriteTime.dwHighDateTime = 0xFFFFFFFF;
		FindData->ftLastWriteTime.dwLowDateTime = 0xFFFFFFFE;
		lf = (pLastFindStuct)malloc(sizeof(tLastFindStuct));
		wcslcpy(lf->PathW, Path, countof(lf->PathW) - 1);
		lf->searchhandle = INVALID_HANDLE_VALUE;
		char ch = 'A';
		wcscpy(buf, L"A:\\");
		while (GetDriveTypeW(buf) == DRIVE_NO_ROOT_DIR && ch < 'Z' + 1) {
			ch++;
			buf[0] = ch;
		}
		buf[2] = 0;
		if (ch <= 'Z') {
			wcslcpy(lf->LastFoundNameW, buf, countof(lf->LastFoundNameW) - 1);
			wcslcpy(FindData->cFileName, buf, countof(FindData->cFileName) - 1);
			return (HANDLE)lf;
		}
		else {
			free(lf);
			return INVALID_HANDLE_VALUE;
		}
	}
	else {
		wcslcpy(buf, Path + pluginrootlen, countof(buf) - 5);
		wcslcat(buf, L"\\*.*", countof(buf) - 1);
		HANDLE hdnl = FindFirstFileT(buf, FindData);
		if (hdnl == INVALID_HANDLE_VALUE)
			return INVALID_HANDLE_VALUE;
		else {
			lf = (pLastFindStuct)malloc(sizeof(tLastFindStuct));
			wcslcpy(lf->PathW, buf, countof(lf->PathW) - 1);
			lf->searchhandle = hdnl;
			return (HANDLE)lf;
		}
	}
	return INVALID_HANDLE_VALUE;
}

HANDLE __stdcall FsFindFirst(char* Path, WIN32_FIND_DATA* FindData)
{
	WIN32_FIND_DATAW FindDataW;
	WCHAR PathW[wdirtypemax];
	HANDLE retval = FsFindFirstW(awfilenamecopy(PathW, Path), &FindDataW);
	if (retval != INVALID_HANDLE_VALUE)
		copyfinddatawa(FindData, &FindDataW);
	return retval;
}

BOOL __stdcall FsFindNextW(HANDLE Hdl, WIN32_FIND_DATAW* FindData)
{
	WCHAR buf[wdirtypemax];
	pLastFindStuct lf;

	if ((int)Hdl == 1)
		return false;

	lf = (pLastFindStuct)Hdl;
	if (lf->searchhandle == INVALID_HANDLE_VALUE) {   // drive list!
		char ch = (char)lf->LastFoundNameW[0];
		wcscpy(buf, L"A:\\");
		buf[0] = ch + 1;
		while (GetDriveTypeW(buf) == DRIVE_NO_ROOT_DIR && ch < 'Z' + 1) {
			ch++;
			buf[0] = ch;
		}
		buf[2] = 0;
		if (ch <= 'Z') {
			wcslcpy(lf->LastFoundNameW, buf, countof(lf->LastFoundNameW) - 1);
			wcslcpy(FindData->cFileName, buf, countof(FindData->cFileName) - 1);
			return true;
		}
		else {
			return false;
		}

	}
	else {
		lf = (pLastFindStuct)Hdl;
		return FindNextFileT(lf->searchhandle, FindData);
	}
	return false;
}

BOOL __stdcall FsFindNext(HANDLE Hdl, WIN32_FIND_DATA* FindData)
{
	WIN32_FIND_DATAW FindDataW;
	copyfinddataaw(&FindDataW, FindData);
	BOOL retval = FsFindNextW(Hdl, &FindDataW);
	if (retval)
		copyfinddatawa(FindData, &FindDataW);
	return retval;
}

int __stdcall FsFindClose(HANDLE Hdl)
{
	if ((int)Hdl == 1)
		return 0;
	pLastFindStuct lf;
	lf = (pLastFindStuct)Hdl;
	if (lf->searchhandle != INVALID_HANDLE_VALUE) {
		FindClose(lf->searchhandle);
		lf->searchhandle = INVALID_HANDLE_VALUE;
	}
	free(lf);
	return 0;
}

BOOL __stdcall FsMkDir(char* Path)
{
	WCHAR wbuf[wdirtypemax];
	return FsMkDirW(awfilenamecopy(wbuf, Path));
}

BOOL __stdcall FsMkDirW(WCHAR* Path)
{
	if (wcslen(Path) < pluginrootlen + 2)
		return false;
	return CreateDirectoryT(Path + pluginrootlen, NULL);
}

int __stdcall FsRenMovFile(char* OldName, char* NewName, BOOL Move, BOOL OverWrite, RemoteInfoStruct* ri)
{
	WCHAR OldNameW[wdirtypemax], NewNameW[wdirtypemax];
	return FsRenMovFileW(awfilenamecopy(OldNameW, OldName), awfilenamecopy(NewNameW, NewName), Move, OverWrite, ri);
}

int __stdcall FsRenMovFileW(WCHAR* OldName, WCHAR* NewName, BOOL Move, BOOL OverWrite, RemoteInfoStruct* ri)
{
	if (wcslen(OldName) < pluginrootlen + 2 || wcslen(NewName) < pluginrootlen + 2)
		return FS_FILE_NOTFOUND;

	int err = ProgressProcT(PluginNumber, OldName, NewName, 0);
	if (err)
		return FS_FILE_USERABORT;

	if (Move) {
		if (OverWrite)
			DeleteFileT(NewName + pluginrootlen);
		if (MoveFileT(OldName + pluginrootlen, NewName + pluginrootlen))
			return FS_FILE_OK;
	}
	else {
		if (CopyFileT(OldName + pluginrootlen, NewName + pluginrootlen, !OverWrite))
			return FS_FILE_OK;
	}
	switch (GetLastError()) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_ACCESS_DENIED:
		return FS_FILE_NOTFOUND;
	case ERROR_FILE_EXISTS:
		return FS_FILE_EXISTS;
	default:
		return FS_FILE_WRITEERROR;
	}
	err = ProgressProcT(PluginNumber, OldName, NewName, 100);
	if (err)
		return FS_FILE_USERABORT;
}

int __stdcall FsGetFileW(WCHAR* RemoteName, WCHAR* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
	int err;
	BOOL ok, OverWrite, Resume, Move;

	OverWrite = CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume = CopyFlags & FS_COPYFLAGS_RESUME;
	Move = CopyFlags & FS_COPYFLAGS_MOVE;

	if (Resume)
		return FS_FILE_NOTSUPPORTED;

	if (wcslen(RemoteName) < pluginrootlen + 2)
		return FS_FILE_NOTFOUND;

	err = ProgressProcT(PluginNumber, RemoteName, LocalName, 0);
	if (err)
		return FS_FILE_USERABORT;
	if (Move) {
		if (OverWrite)
			DeleteFileT(LocalName);
		ok = MoveFileT(RemoteName + pluginrootlen, LocalName);
	}
	else
		ok = CopyFileT(RemoteName + pluginrootlen, LocalName, !OverWrite);

	if (ok) {
		ProgressProcT(PluginNumber, RemoteName, LocalName, 100);
		return FS_FILE_OK;
	}
	else {
		err = GetLastError();
		switch (err) {
		case 2:
		case 3:
		case 4:
		case 5:
			return FS_FILE_NOTFOUND;
		case ERROR_FILE_EXISTS:
			return FS_FILE_EXISTS;
		default:
			return FS_FILE_READERROR;
		}
	}
}

int __stdcall FsGetFile(char* RemoteName, char* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
	WCHAR RemoteNameW[wdirtypemax], LocalNameW[wdirtypemax];
	return FsGetFileW(awfilenamecopy(RemoteNameW, RemoteName), awfilenamecopy(LocalNameW, LocalName), CopyFlags, ri);
}

int __stdcall FsPutFileW(WCHAR* LocalName, WCHAR* RemoteName, int CopyFlags)
{
	int err;
	BOOL ok, OverWrite, Resume, Move;

	OverWrite = CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume = CopyFlags & FS_COPYFLAGS_RESUME;
	Move = CopyFlags & FS_COPYFLAGS_MOVE;
	if (Resume)
		return FS_FILE_NOTSUPPORTED;

	if (wcslen(RemoteName) < pluginrootlen + 2)
		return FS_FILE_NOTFOUND;

	err = ProgressProcT(PluginNumber, LocalName, RemoteName, 0);
	if (err)
		return FS_FILE_USERABORT;
	if (Move) {
		if (OverWrite)
			DeleteFileT(RemoteName + pluginrootlen);
		ok = MoveFileT(LocalName, RemoteName + pluginrootlen);
	}
	else
		ok = CopyFileT(LocalName, RemoteName + pluginrootlen, !OverWrite);

	if (ok) {
		ProgressProcT(PluginNumber, RemoteName, LocalName, 100);
		return FS_FILE_OK;
	}
	else {
		err = GetLastError();
		switch (err) {
		case 2:
		case 3:
		case 4:
		case 5:
			return FS_FILE_NOTFOUND;
		case ERROR_FILE_EXISTS:
			return FS_FILE_EXISTS;
		default:
			return FS_FILE_READERROR;
		}
	}
}

int __stdcall FsPutFile(char* LocalName, char* RemoteName, int CopyFlags)
{
	WCHAR LocalNameW[wdirtypemax], RemoteNameW[wdirtypemax];
	return FsPutFileW(awfilenamecopy(LocalNameW, LocalName), awfilenamecopy(RemoteNameW, RemoteName), CopyFlags);
}

BOOL __stdcall FsDeleteFileW(WCHAR* RemoteName)
{
	if (wcslen(RemoteName) < pluginrootlen + 2)
		return false;

	return DeleteFileT(RemoteName + pluginrootlen);
}

BOOL __stdcall FsDeleteFile(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsDeleteFileW(awfilenamecopy(RemoteNameW, RemoteName));
}

BOOL __stdcall FsRemoveDirW(WCHAR* RemoteName)
{
	if (wcslen(RemoteName) < pluginrootlen + 2)
		return false;

	return RemoveDirectoryT(RemoteName + pluginrootlen);
}

BOOL __stdcall FsRemoveDir(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsRemoveDirW(awfilenamecopy(RemoteNameW, RemoteName));
}

void __stdcall FsStatusInfo(char* RemoteDir, int InfoStartEnd, int InfoOperation)
{
	// This function may be used to initialize variables and to flush buffers

/*	char text[wdirtypemax];

	if (InfoStartEnd==FS_STATUS_START)
		strcpy(text,"Start: ");
	else
		strcpy(text,"End: ");

	switch (InfoOperation) {
	case FS_STATUS_OP_LIST:
		strcat(text,"Get directory list");
		break;
	case FS_STATUS_OP_GET_SINGLE:
		strcat(text,"Get single file");
		break;
	case FS_STATUS_OP_GET_MULTI:
		strcat(text,"Get multiple files");
		break;
	case FS_STATUS_OP_PUT_SINGLE:
		strcat(text,"Put single file");
		break;
	case FS_STATUS_OP_PUT_MULTI:
		strcat(text,"Put multiple files");
		break;
	case FS_STATUS_OP_RENMOV_SINGLE:
		strcat(text,"Rename/Move/Remote copy single file");
		break;
	case FS_STATUS_OP_RENMOV_MULTI:
		strcat(text,"Rename/Move/Remote copy multiple files");
		break;
	case FS_STATUS_OP_DELETE:
		strcat(text,"Delete multiple files");
		break;
	case FS_STATUS_OP_ATTRIB:
		strcat(text,"Change attributes of multiple files");
		break;
	case FS_STATUS_OP_MKDIR:
		strcat(text,"Create directory");
		break;
	case FS_STATUS_OP_EXEC:
		strcat(text,"Execute file or command line");
		break;
	case FS_STATUS_OP_CALCSIZE:
		strcat(text,"Calculate space occupied by a directory");
		break;
	case FS_STATUS_OP_SEARCH:
		strcat(text,"Search for file names");
		break;
	case FS_STATUS_OP_SEARCH_TEXT:
		strcat(text,"Search for text in files");
		break;
	case FS_STATUS_OP_SYNC_SEARCH:
		strcat(text,"Search files for sync comparison");
		break;
	case FS_STATUS_OP_SYNC_GET:
		strcat(text,"download files during sync");
		break;
	case FS_STATUS_OP_SYNC_PUT:
		strcat(text,"Upload files during sync");
		break;
	case FS_STATUS_OP_SYNC_DELETE:
		strcat(text,"Delete files during sync");
		break;
	default:
		strcat(text,"Unknown operation");
	}
	if (InfoOperation != FS_STATUS_OP_LIST)   // avoid recursion due to re-reading!
		MessageBox(0,text,RemoteDir,0);
*/
}

void __stdcall FsGetDefRootName(char* DefRootName, int maxlen)
{
	strlcpy(DefRootName, "Flipper", maxlen);
}

void __stdcall FsSetDefaultParams(FsDefaultParamStruct* dps)
{
	strlcpy(inifilename, dps->DefaultIniName, MAX_PATH - 1);
}

void __stdcall FsContentPluginUnloading(void)
{
	// If you do something in a background thread, you may
	// wait in this function until the thread has finished
	// its work to prevent Total Commander from closing!
	MessageBox(0, "totalflip unloading!", "Test", 0);
}
