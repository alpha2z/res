


#include "MpqScript.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "StormLib.h"
#include "StormCommon.h"

#include "TLogHelper.cpp"

#ifdef PLATFORM_WINDOWS
#define PATH_SEPARATOR   '\\'           // Path separator for Windows platforms
#else
#define PATH_SEPARATOR   '/'            // Path separator for Windows platforms
#endif

static char PACKET_PATH[MAX_PATH] = {0};
//static char root[MAX_PATH] = {0};// "/Users/riddick/StormLib/test";

static int AddFile_my(const char* );
static int AddSubdir_my(TLogHelper *, HANDLE , const char* , const char* );

static bool
IsFullPath_my(const char* szFileName) {
#ifdef PLATFORM_WINDOWS
	if (('A' <= szFileName[0] && szFileName[0] <= 'Z') || ('a' <= szFileName[0] && szFileName[0] <= 'z')) {
		return (szFileName[1] == ":" && szFileName[2] == PATH_SEPARATOR);
	}
#endif
	// szFileName = szFileName;
	return false;
}

static size_t
FileStream_PrefixA_my(const char* szFileName, DWORD * pdwProvider) {
	TCHAR szFileNameT[MAX_PATH] = {0};
	size_t nPrefixLength = 0;
	if (szFileName != NULL) {
		CopyFileName(szFileNameT, szFileName, strlen(szFileName));
		nPrefixLength = FileStream_Prefix(szFileNameT, pdwProvider);
	}

	return nPrefixLength;
}



static void
CreateFullPathName_my(char* pBuffer, const char* pSubDir, const char* pNamePart1, const char* pNamePart2) {
	size_t nPrefixLength = 0;
	size_t nLength;
	DWORD dwProvider = 0;
	bool bIsFullPath = false;
	char chSeparator = PATH_SEPARATOR;

	if (pNamePart1 != NULL) {
		nPrefixLength = FileStream_PrefixA_my(pNamePart1, &dwProvider);
		if ((dwProvider & BASE_PROVIDER_MASK) == BASE_PROVIDER_HTTP) {
			bIsFullPath = true;
			chSeparator = PATH_SEPARATOR;
		} else {
			bIsFullPath = IsFullPath_my(pNamePart1 + nPrefixLength);
		}
	}

	if (nPrefixLength > 0) {
		memcpy(pBuffer, pNamePart1, nPrefixLength);
		pNamePart1 += nPrefixLength;
		pBuffer += nPrefixLength;
	}

	if (bIsFullPath == false) {
		memcpy(pBuffer, PACKET_PATH, strlen(PACKET_PATH));
		pBuffer += strlen(PACKET_PATH);

		if (pSubDir != NULL && (nLength = strlen(pSubDir)) != 0) {
			assert(pSubDir[0] != PATH_SEPARATOR);
			assert(pSubDir[nLength - 1] != PATH_SEPARATOR);

			*pBuffer++ = PATH_SEPARATOR;

			memcpy(pBuffer, pSubDir, nLength);
			pBuffer += nLength;
		}
	}

	if (pNamePart1 != NULL && (nLength = strlen(pNamePart1)) != 0) {
		assert(pNamePart1[0] != PATH_SEPARATOR);
		assert(pNamePart1[nLength - 1] != PATH_SEPARATOR);

		if (bIsFullPath == false) {
			*pBuffer++ = PATH_SEPARATOR;
		}

		memcpy(pBuffer, pNamePart1, nLength);
		pBuffer += nLength;
	}

	if (pNamePart2 && (nLength = strlen(pNamePart2)) != 0) {
		memcpy(pBuffer, pNamePart2, nLength);
		pBuffer += nLength;
	}

	*pBuffer = 0;
}
 

static int
CreateNewArchive_my(TLogHelper *pLogger, const char* pPlainName, DWORD dwCreateFlags, DWORD dwMaxFileCount, HANDLE *phMpq) {
	HANDLE hMpq = NULL;
	TCHAR chMpqName[MAX_PATH] = {0};
	char chFullPath[MAX_PATH] = {0};

	CreateFullPathName_my(chFullPath, NULL, pPlainName);
	remove(chFullPath);

    dwCreateFlags |= MPQ_CREATE_ARCHIVE_V2;
	CopyFileName(chMpqName, chFullPath, strlen(chFullPath));
    if (!SFileCreateArchive(chMpqName, dwCreateFlags, dwMaxFileCount, &hMpq)) {
		return pLogger->PrintError(_T("Failed to create archive %s"), chMpqName);
	}

	if (phMpq == NULL) {
		SFileCloseArchive(hMpq);
		return ERROR_INVALID_HANDLE;
	} else {
		*phMpq = hMpq;
	}
	return ERROR_SUCCESS;
}


static bool
CheckIfFileIsPresent_my(TLogHelper *pLogger, HANDLE hMpq, const char* pFileName, bool bShouldExist) {
	HANDLE hFile = NULL;
	if (SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
		if (bShouldExist == false) {
			pLogger->PrintMessage("The file : %s is present, but it should not be", pFileName);
		}
		SFileCloseFile(hFile);
		return true;
	} else {
		if (bShouldExist) {
			pLogger->PrintMessage("the file %s is not present, but it should be", pFileName);
		}
		return false;
	}
}

static bool
IsDir(const char* name) {
	DIR *pDir = NULL;
	pDir = opendir(name);
	return pDir != NULL;
}

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>


static int
AddSubFile_my(TLogHelper *pLogger, HANDLE hMpq, const char* pDir, const char* pFileName) {
	int nError = ERROR_SUCCESS;
	
	DWORD dwCompression = MPQ_COMPRESSION_ZLIB;
	DWORD dwFlags = MPQ_FILE_ENCRYPTED | MPQ_FILE_COMPRESS;
    
    char chFileName[MAX_PATH] = {0};
    sprintf(chFileName, "%s/%s", pDir, pFileName);

    FILE *pFile = fopen(chFileName, "rb");
    assert(pFile);
    if (!pFile) {
    	nError = ERROR_FILE_NOT_FOUND;
    	return nError;
    }
    
    fseek(pFile, 0L, SEEK_END);
    size_t sz = ftell(pFile);
    fseek(pFile, 0L, SEEK_SET);

	HANDLE hFile = NULL;
	pLogger->PrintProgress("Adding file %s ...", pFileName);
    
	if (!SFileCreateFile(hMpq, pFileName, 0, sz, 0, dwFlags, &hFile)) {
		return GetLastError();
	}
    
    char * pFileData = NULL;
    while ((pFileData = fgetln(pFile, &sz)) != NULL) {
    	//   FileStream_Write
        if (!SFileWriteFile(hFile, pFileData, sz, dwCompression)) {
            nError = pLogger->PrintError("Failed to write data to MQP");
        }
    }
    SFileCloseFile(hFile);
    fclose(pFile);
	return nError;
}



static int
IteratorDirection(TLogHelper *pLogger, HANDLE hMpq, const char* pRoot, const char* pSubDir) {
    int nError = ERROR_SUCCESS;
    
    char chFilePath[MAX_PATH] = {0};
    if (pSubDir) {
        sprintf(chFilePath, "%s/%s", pRoot, pSubDir);
    } else {
        sprintf(chFilePath, "%s", pRoot);
    }

    DIR *pstDir = opendir(chFilePath);
    struct dirent *pstEnt = NULL;
    while ((pstEnt = readdir(pstDir)) != NULL) {
        if (pstEnt->d_name[0] == '.')
            continue;
        
        if (pstEnt->d_type & DT_DIR) {
            if (strcmp(pstEnt->d_name, ".") == 0 || strcmp(pstEnt->d_name, "..") == 0)
                continue;
            memset(chFilePath, 0, MAX_PATH);
            if (pSubDir) {
                sprintf(chFilePath, "%s/%s", pSubDir, pstEnt->d_name);
            } else {
                sprintf(chFilePath, "%s", pstEnt->d_name);
            }
            
            nError = IteratorDirection(pLogger, hMpq, pRoot, chFilePath);
            assert(nError == ERROR_SUCCESS);
            if (nError != ERROR_SUCCESS)
                break;
        } else {
            memset(chFilePath, 0, MAX_PATH);
            if (pSubDir) {
                sprintf(chFilePath, "%s/%s", pSubDir, pstEnt->d_name);
            } else {
                sprintf(chFilePath, "%s", pstEnt->d_name);
            }
            
            nError = AddSubFile_my(pLogger, hMpq, pRoot, chFilePath);
            assert(nError == ERROR_SUCCESS);
            if (nError != ERROR_SUCCESS)
                break;
        }
    }
    
    return nError;
}

#include <assert.h>


static int 
CreateArchive_luascript(const char* pPlainName, const char* pScriptPath, int nMaxCount) {
    assert(IsDir(pScriptPath));

	TLogHelper Logger("Create Lua Script", pPlainName);
	HANDLE hMpq = NULL;
	DWORD dwMaxFileCount = nMaxCount;

	int nError;

	nError = CreateNewArchive_my(&Logger, pPlainName, 0, dwMaxFileCount, &hMpq);
	if (nError == ERROR_SUCCESS) {
        IteratorDirection(&Logger, hMpq, pScriptPath, NULL);
	}


	if (hMpq) {
		SFileCloseArchive(hMpq);
		hMpq = NULL;
	}

	return nError;
}



static TFileData *
LoadMpqFile_my(TLogHelper *pLogger, HANDLE hMpq, const char* pFileName) {
	TFileData *pFileData = NULL;
	HANDLE hFile;
	DWORD dwFileSizeHi = 0xCCCCCCCC;
	DWORD dwFileSizeLo = 0;
	DWORD dwBytesRead;
	int nError = ERROR_SUCCESS;

	if (!SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
		nError = pLogger->PrintError("Failed to open the file %s", pFileName);
	}

	if (nError == ERROR_SUCCESS) {
		dwFileSizeLo = SFileGetFileSize(hFile, &dwFileSizeHi);
		if ((dwFileSizeLo == SFILE_INVALID_SIZE) || dwFileSizeHi != 0) {
			nError = pLogger->PrintError("Failed to query the file size");
		}
	}

	if (nError == ERROR_SUCCESS) {
		pFileData = (TFileData *)STORM_ALLOC(BYTE, sizeof(TFileData) + dwFileSizeLo);
		if (!pFileData) {
			pLogger->PrintError("Failed to allocate buffer for file %s content", pFileName);
			nError = ERROR_NOT_ENOUGH_MEMORY;
		}
	}

	if (nError == ERROR_SUCCESS) {
		memset(pFileData, 0, sizeof(TFileData) + dwFileSizeLo);
		pFileData->dwFileSize = dwFileSizeLo;
		if (!SFileGetFileInfo(hFile, SFileInfoFileIndex, &pFileData->dwBlockIndex, sizeof(DWORD), NULL)) {
			nError = pLogger->PrintError("Failed retrieve the file index of %s", pFileName);
		}
		if (!SFileGetFileInfo(hFile, SFileInfoFlags, &pFileData->dwFlags, sizeof(DWORD), NULL)) {
			nError = pLogger->PrintError("Failed retrieve the file flags of %s", pFileName);
		}
	}

	if (nError == ERROR_SUCCESS) {
		SFileReadFile(hFile, pFileData->FileData, dwFileSizeLo, &dwBytesRead, NULL);
		if (dwBytesRead != dwFileSizeLo) {
			nError = pLogger->PrintError("Failed to read the content of the file %s", pFileName);
		}
        pLogger->PrintProgress("LoadMpqFile_my(%s) ...", pFileName);
        // if (strcmp(pFileName, LISTFILE_NAME) == 0 || strcmp(pFileName, ATTRIBUTES_NAME) == 0) {
//            pLogger->PrintProgress("content %s ...", pFileData->FileData);
        // }
	}

	if (nError != ERROR_SUCCESS) {
		STORM_FREE(pFileData);
		SetLastError(nError);
		pFileData = NULL;
	}

	if (hFile != NULL) {
		SFileCloseFile(hFile);
	}
	return pFileData;
}

static int
GetFilePatchCount_my(TLogHelper *pLogger, HANDLE hMpq, const char* pFileName) {
	TCHAR *pPatchName;
	HANDLE hFile;
	TCHAR chPatchChain[0x400] = {0};
	int nPatchCount = 0;
	int nError = ERROR_SUCCESS;

	if (SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
		pLogger->PrintProgress("Verifying patch chain for %s ....", pFileName);

		if (!SFileGetFileInfo(hFile, SFileInfoPatchChain, chPatchChain, sizeof(chPatchChain), NULL))
			nError = pLogger->PrintError("Failed to retrieve the patch chain on %s", pFileName);

		if (nError == ERROR_SUCCESS && chPatchChain[0] == 0) {
			pLogger->PrintError("the patch chain for %s is empty", pFileName);
			nError = ERROR_FILE_CORRUPT;
		}

		if (nError == ERROR_SUCCESS) {
			pPatchName = chPatchChain;
			for(;;) {
				pPatchName = pPatchName + _tcslen(pPatchName) + 1;
				if (pPatchName[0] == 0)
					break;

				nPatchCount++;
			}
		}
		SFileCloseFile(hFile);
	} else {
		pLogger->PrintError("Failed to open file %s", pFileName);
	}

	return nPatchCount;
}

static int
SearchArchive_my(
	TLogHelper *pLogger,
	HANDLE hMpq,
	DWORD dwTestFlags = 0,
	DWORD * pdwFileCount = NULL,
	LPBYTE pbFileHash = NULL) {
	SFILE_FIND_DATA sf;
	TFileData * pFileData = NULL;
	HANDLE hFind;
	DWORD dwFileCount = 0;
	hash_state md5state;

	char chMostPatched[MAX_PATH] = {0};
	char chListFile[MAX_PATH] = {0};
	bool bFound = true;
	int nMaxPatchCount = 0;
	int nPatchCount = 0;
	int nError = ERROR_SUCCESS;

	CreateFullPathName_my(chListFile, NULL, "xxx.txt");

	md5_init(&md5state);

	pLogger->PrintProgress("search the archive ...");
	hFind = SFileFindFirstFile(hMpq, "*", &sf, chListFile);
	if (hFind == NULL) {
		nError = GetLastError();
		nError = (nError == ERROR_NO_MORE_FILES) ? ERROR_SUCCESS : nError;
		return nError;
	}

	while(bFound) {
		dwFileCount++;

		if (dwTestFlags & TEST_FLAG_MOST_PATCHED) {
			nPatchCount = GetFilePatchCount_my(pLogger, hMpq, sf.cFileName);
			if (nPatchCount > nMaxPatchCount) {
				strcpy(chMostPatched, sf.cFileName);
				nMaxPatchCount = nPatchCount;
			}
		}

		if (dwTestFlags & TEST_FLAG_LOAD_FILES) {
			pFileData = LoadMpqFile_my(pLogger, hMpq, sf.cFileName);
			if (pFileData != NULL) {
				if ((dwTestFlags & TEST_FLAG_HASH_FILES) && !IsInternalMpqFileName(sf.cFileName))
					md5_process(&md5state, pFileData->FileData, pFileData->dwFileSize);

				if ((dwTestFlags & TEST_FLAG_PLAY_WAVES) && strstr(sf.cFileName, ".wav") != NULL) {

				}

				STORM_FREE(pFileData);
			}
		}
		bFound = SFileFindNextFile(hFind, &sf);
	}

	SFileFindClose(hFind);

	if (pdwFileCount != NULL) {
		pdwFileCount[0] = dwFileCount;
	}

	if (pbFileHash != NULL && (dwTestFlags & TEST_FLAG_HASH_FILES)) {
		md5_done(&md5state, pbFileHash);
	}

	return nError;


}



static int
OpenArchive_luascript(const char* pPlainName, const char* pListFile) {
	TLogHelper Logger("Open luascript", pPlainName);
//	TFileData *pFileData = NULL;
	HANDLE hMpq;
	DWORD dwFileCount = 0;
	DWORD dwTestFlags;
	char chListFileBuff[MAX_PATH] = {0};
	int nError = ERROR_SUCCESS;

	DWORD dwFlags = MPQ_OPEN_READ_ONLY;
    
    char chRealPath[MAX_PATH] = {0};
    CreateFullPathName_my(chRealPath, NULL, pPlainName);

 	if (!SFileOpenArchive(chRealPath, 0, dwFlags, &hMpq)) {
 		nError = GetLastError();
 		if (nError == ERROR_AVI_FILE || nError == ERROR_FILE_INCOMPLETE)
 			return nError;

 		return Logger.PrintError("Failed to open archive %s", pPlainName);
 	}
	if (nError == ERROR_SUCCESS) {
		if (pListFile != NULL) {
			Logger.PrintProgress("Adding listfile %s ...", pListFile);
			CreateFullPathName_my(chListFileBuff, NULL, pListFile);
			nError = SFileAddListFile(hMpq, chListFileBuff);
			if (nError != ERROR_SUCCESS) {
				Logger.PrintMessage("Failed to add the listfile to the MPQ");
			}
		}

//		if (SFileHasFile(hMpq, LISTFILE_NAME)) {
//			pFileData = LoadMpqFile_my(&Logger, hMpq, LISTFILE_NAME);
//			if (pFileData != NULL) {
//				STORM_FREE(pFileData);
//			}
//		}
//
//		if (SFileHasFile(hMpq, ATTRIBUTES_NAME)) {
//			pFileData = LoadMpqFile_my(&Logger, hMpq, ATTRIBUTES_NAME);
//			if (pFileData != NULL) {
//				STORM_FREE(pFileData);
//			}
//		}

		dwTestFlags = TEST_FLAG_LOAD_FILES;
		nError = SearchArchive_my(&Logger, hMpq, dwTestFlags, &dwFileCount);
		SFileCloseArchive(hMpq);
	}

	return nError;
}

static int
LoadCertainFile(const char* pPackage, const char* pFileName) {
    int nError = ERROR_SUCCESS;
    TLogHelper Logger("test file data", pFileName);
    HANDLE hMpq = NULL;
    DWORD dwFlags = MPQ_OPEN_READ_ONLY;
    if (!SFileOpenArchive(pPackage, 0, dwFlags, &hMpq)) {
        nError = ERROR_FILE_NOT_FOUND;
    }
    
    if (nError == ERROR_SUCCESS) {
        TFileData* pstFileData = NULL;
        if (SFileHasFile(hMpq, pFileName)) {
            pstFileData = LoadMpqFile_my(&Logger, hMpq, pFileName);
//            Logger.PrintProgress("LoadCertainFile : %s", pstFileData->FileData);
        } else
            nError = ERROR_FILE_NOT_FOUND;
        
        if (pstFileData) {
            STORM_FREE(pstFileData);
        }
        
        SFileCloseArchive(hMpq);
        hMpq = NULL;
    }
    
    return nError;
}


int main(int argc, char * argv[])
{
    int nError = ERROR_SUCCESS;
    char root[MAX_PATH] = {0};
    strcpy(root, "/Users/riddick/StormLib/test");
    memcpy(PACKET_PATH, root, strlen(root));

    // Initialize storage and mix the random number generator
    printf("==== Test Suite for StormLib version %s ====\n", STORMLIB_VERSION_STRING);
//    nError = InitializeMpqDirectory(argv, argc);
    
    const char * script = "game.script";
//    const char * path2 = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit_release/Resources/script_lc";
    const char *path2 = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit/Resources/script";
	printf("------------- create -------------\n");
    CreateArchive_luascript(script, path2, 100);
    printf("------------- open -------------\n");
	OpenArchive_luascript(script, NULL);
    
    nError = LoadCertainFile("/Users/riddick/StormLib/test/game.script", "Main.lua");
    if (nError != ERROR_SUCCESS) {
        printf("LOAD main.lc failed \n");
    }
    
    printf("%x\n", ID_MPQ);
    printf("%x\n", ID_MPQ_USERDATA);
    printf("%x\n", ID_MPK);
    
    printf("%x\n", MPQ_HEADER_SIZE_V1);
    printf("%x\n", MPQ_HEADER_SIZE_V2);
    printf("%x\n", MPQ_HEADER_SIZE_V3);
    printf("%x\n", MPQ_HEADER_SIZE_V4);
    printf("%d\n", MPQ_HEADER_DWORDS);
    

    

    printf("===== END =====\n");
    return nError;

}















