


//#include "MpqScript.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "StormLib.h"
#include "StormCommon.h"

//#include "TLogHelper.cpp"
#include <assert.h>

#include <string>

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#ifndef PRINT_MESSAGE
#define PRINT_MESSAGE PrintMessage
#endif

#ifdef PLATFORM_WINDOWS
#define PATH_SEPARATOR   '\\'           // Path separator for Windows platforms
#else
#define PATH_SEPARATOR   '/'            // Path separator for Windows platforms
#endif


static char PACKET_PATH[MAX_PATH] = {0};

static int AddSubdir_my(HANDLE hMpq, const char* root, const char* dir);

void PrintMessage(const char* fmt, ...) {
    
#ifdef _DEBUG
    va_list args;
    char msg[4096] = {0};
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    printf("%s\n", msg);
#endif
    
}

static bool
IsDir(const char* name) {
	DIR *pDir = NULL;
	pDir = opendir(name);
	return pDir != NULL;
}

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
CreateFullPathName_my(char* pBuffer, const char* pSubDir, const char* pNamePart1, const char* pNamePart2 = NULL) {
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

static bool
CheckIfFileIsPresent_my(HANDLE hMpq, const char* pFileName, bool bShouldExist) {
	HANDLE hFile = NULL;
	if (SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
		if (bShouldExist == false) {
			PRINT_MESSAGE("The file : %s is present, but it should not be", pFileName);
		}
		SFileCloseFile(hFile);
		return true;
	} else {
		if (bShouldExist) {
			PRINT_MESSAGE("the file %s is not present, but it should be", pFileName);
		}
		return false;
	}
}

void InitMyMpqScriptData(const char* path) {
	assert(strlen(path) <= MAX_PATH);
	memcpy(PACKET_PATH, path, strlen(path));
}


int
AddSubFile_my(HANDLE hMpq, const char* pDir, const char* pFileName) {
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
	PRINT_MESSAGE("Adding file %s ...", pFileName);
    
	if (!SFileCreateFile(hMpq, pFileName, 0, sz, 0, dwFlags, &hFile)) {
		return GetLastError();
	}
    
    void * pBuffer = ::malloc(sz);
    int nLength = fread(pBuffer, 1, sz, pFile);
    assert(nLength != -1);
    if (nLength < sz || !SFileWriteFile(hFile, pBuffer, sz, dwCompression)) {
        PRINT_MESSAGE("Failed to write data to MQP");
        nError = GetLastError();
    }
    
    SFileCloseFile(hFile);
    fclose(pFile);
    ::free(pBuffer);
	return nError;
}



static int
IteratorDirection(HANDLE hMpq, const char* pRoot, const char* pSubDir) {
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
            
            nError = IteratorDirection(hMpq, pRoot, chFilePath);
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
            
            nError = AddSubFile_my(hMpq, pRoot, chFilePath);
            assert(nError == ERROR_SUCCESS);
            if (nError != ERROR_SUCCESS)
                break;
        }
    }
    
    return nError;
}

int 
CreateArchive_luascript(const char* pPlainName, const char* pScriptPath, int nMaxCount, LPBYTE pPassword) {
    assert(IsDir(pScriptPath));
    
	HANDLE hMpq = NULL;
	DWORD dwMaxFileCount = nMaxCount;
  
    DWORD dwStreamFlags = STREAM_PROVIDER_FLAT;
    if (pPassword) {
        dwStreamFlags = STREAM_PROVIDER_MPQE;
    }

	int nError;
	char chFullPath[MAX_PATH] = {0};
    
	CreateFullPathName_my(chFullPath, NULL, pPlainName);
	remove(chFullPath);
    
    if (!SFileCreateArchive(chFullPath, dwStreamFlags, dwMaxFileCount, &hMpq, pPassword)) {
        PRINT_MESSAGE("Failed to create archive %s", chFullPath);
        return GetLastError();
	}

    IteratorDirection(hMpq, pScriptPath, NULL);

	if (hMpq) {
		SFileCloseArchive(hMpq);
		hMpq = NULL;
	}
    
	return nError;
}

static TFileData *
LoadMpqFile_my(HANDLE hMpq, const char* pFileName) {
	TFileData *pFileData = NULL;
	HANDLE hFile;
	DWORD dwFileSizeHi = 0xCCCCCCCC;
	DWORD dwFileSizeLo = 0;
	DWORD dwBytesRead;
	int nError = ERROR_SUCCESS;

	PRINT_MESSAGE("Loading file %s ...", pFileName);

	if (!SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
        PRINT_MESSAGE("Failed to open the file %s", pFileName);
        return NULL;
	}

	if (nError == ERROR_SUCCESS) {
		dwFileSizeLo = SFileGetFileSize(hFile, &dwFileSizeHi);
		if ((dwFileSizeLo == SFILE_INVALID_SIZE) || dwFileSizeHi != 0) {
            PRINT_MESSAGE("Failed to query the file size");
            return NULL;
		}
	}

	if (nError == ERROR_SUCCESS) {
		pFileData = (TFileData *)STORM_ALLOC(BYTE, sizeof(TFileData) + dwFileSizeLo);
		if (!pFileData) {
			PRINT_MESSAGE("Failed to allocate buffer for file %s content", pFileName);
			nError = ERROR_NOT_ENOUGH_MEMORY;
		}
	}

	if (nError == ERROR_SUCCESS) {
		memset(pFileData, 0, sizeof(TFileData) + dwFileSizeLo);
		pFileData->dwFileSize = dwFileSizeLo;
		if (!SFileGetFileInfo(hFile, SFileInfoFileIndex, &pFileData->dwBlockIndex, sizeof(DWORD), NULL)) {
			PRINT_MESSAGE("Failed retrieve the file index of %s", pFileName);
			nError = GetLastError();
		}
		if (!SFileGetFileInfo(hFile, SFileInfoFlags, &pFileData->dwFlags, sizeof(DWORD), NULL)) {
			PRINT_MESSAGE("Failed retrieve the file flags of %s", pFileName);
			nError = GetLastError();
		}
	}

	if (nError == ERROR_SUCCESS) {
		SFileReadFile(hFile, pFileData->FileData, dwFileSizeLo, &dwBytesRead, NULL);
		if (dwBytesRead != dwFileSizeLo) {
			PRINT_MESSAGE("Failed to read the content of the file %s", pFileName);
			nError = GetLastError();
		}
		// PRINT_MESSAGE("LoadMpqFile_my(%s) content: %s", pFileName, pFileData->FileData);
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
//
//int
//GetFilePatchCount_my(HANDLE hMpq, const char* pFileName) {
//	TCHAR *pPatchName;
//	HANDLE hFile;
//	TCHAR chPatchChain[0x400] = {0};
//	int nPatchCount = 0;
//	int nError = ERROR_SUCCESS;
//
//	if (SFileOpenFileEx(hMpq, pFileName, 0, &hFile)) {
//		pLogger->PrintProgress("Verifying patch chain for %s ....", pFileName);
//
//		if (!SFileGetFileInfo(hFile, SFileInfoPatchChain, chPatchChain, sizeof(chPatchChain), NULL))
//			nError = pLogger->PrintError("Failed to retrieve the patch chain on %s", pFileName);
//
//		if (nError == ERROR_SUCCESS && chPatchChain[0] == 0) {
//			pLogger->PrintError("the patch chain for %s is empty", pFileName);
//			nError = ERROR_FILE_CORRUPT;
//		}
//
//		if (nError == ERROR_SUCCESS) {
//			pPatchName = chPatchChain;
//			for(;;) {
//				pPatchName = pPatchName + _tcslen(pPatchName) + 1;
//				if (pPatchName[0] == 0)
//					break;
//
//				nPatchCount++;
//			}
//		}
//		SFileCloseFile(hFile);
//	} else {
//		pLogger->PrintError("Failed to open file %s", pFileName);
//	}
//
//	return nPatchCount;
//}

static int
SearchArchive_my(
	HANDLE hMpq,
	DWORD dwTestFlags,
	DWORD * pdwFileCount,
	LPBYTE pbFileHash = NULL) {
	SFILE_FIND_DATA sf;
	TFileData * pFileData = NULL;
	HANDLE hFind;
	DWORD dwFileCount = 0;
	hash_state md5state;

	bool bFound = true;
	int nError = ERROR_SUCCESS;
    
	md5_init(&md5state);

	PRINT_MESSAGE("search the archive ...");
	hFind = SFileFindFirstFile(hMpq, "*", &sf, NULL);
	if (hFind == NULL) {
		nError = GetLastError();
		return nError;
	}

	while(bFound) {
		dwFileCount++;

		if (dwTestFlags & TEST_FLAG_LOAD_FILES) {
			pFileData = LoadMpqFile_my(hMpq, sf.cFileName);
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

int
OpenArchive_luascript(const char* pPlainName, LPBYTE pPassword, const char* pListFile) {
	TFileData *pFileData = NULL;
	HANDLE hMpq;
	DWORD dwFileCount = 0;
	DWORD dwTestFlags;
	char chListFileBuff[MAX_PATH] = {0};
	int nError = ERROR_SUCCESS;

	DWORD dwFlags = MPQ_OPEN_READ_ONLY;// | STREAM_PROVIDER_MPQE;
    if (pPassword) {
        dwFlags |= STREAM_PROVIDER_MPQE;
    }
    
    char chRealPath[MAX_PATH] = {0};
    CreateFullPathName_my(chRealPath, NULL, pPlainName);

 	if (!SFileOpenArchive(chRealPath, 0, dwFlags, &hMpq, pPassword)) {
 		nError = GetLastError();
 		if (nError == ERROR_AVI_FILE || nError == ERROR_FILE_INCOMPLETE)
 			return nError;

 		PRINT_MESSAGE("Failed to open archive %s", pPlainName);
 		return nError;
 	}
	if (nError == ERROR_SUCCESS) {
		if (pListFile != NULL) {
			PRINT_MESSAGE("Adding listfile %s ...", pListFile);
			CreateFullPathName_my(chListFileBuff, NULL, pListFile);
			nError = SFileAddListFile(hMpq, chListFileBuff);
			if (nError != ERROR_SUCCESS) {
				PRINT_MESSAGE("Failed to add the listfile to the MPQ");
			}
		}

		if (SFileHasFile(hMpq, LISTFILE_NAME)) {
			pFileData = LoadMpqFile_my(hMpq, LISTFILE_NAME);
			if (pFileData != NULL) {
				STORM_FREE(pFileData);
			}
		}

		if (SFileHasFile(hMpq, ATTRIBUTES_NAME)) {
			pFileData = LoadMpqFile_my(hMpq, ATTRIBUTES_NAME);
			if (pFileData != NULL) {
				STORM_FREE(pFileData);
			}
		}

		dwTestFlags = TEST_FLAG_LOAD_FILES;
		nError = SearchArchive_my(hMpq, dwTestFlags, &dwFileCount);
		SFileCloseArchive(hMpq);
	}

	return nError;
}

TFileData *
LoadCertainFile(const char* pPackage, const char* pFileName, LPBYTE pPassword) {
    HANDLE hMpq = NULL;
    TFileData* pstFileData = NULL;
    
    DWORD dwFlags = MPQ_OPEN_READ_ONLY;
    if (pPassword) {
        dwFlags |= STREAM_PROVIDER_MPQE;
    }
    
    if (!SFileOpenArchive(pPackage, 0, dwFlags, &hMpq, pPassword)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return pstFileData;
    }
    
    if (SFileHasFile(hMpq, pFileName)) {
        pstFileData = LoadMpqFile_my(hMpq, pFileName);
        PRINT_MESSAGE("LoadCertainFile : \n%s", pstFileData->FileData);
    } else {
        SetLastError(ERROR_FILE_NOT_FOUND);
    }

    SFileCloseArchive(hMpq);
    hMpq = NULL;
    
    return pstFileData;
}


//
//int main(int argc, char * argv[])
//{
//    int nError = ERROR_SUCCESS;
//    
//    memcpy(PACKET_PATH, root, strlen(root));
//
//    // Initialize storage and mix the random number generator
//    printf("==== Test Suite for StormLib version %s ====\n", STORMLIB_VERSION_STRING);
//
//    std::string script = "game.script";
//    char * path = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit_release/Resources/script_lc";
//    std::string path2 = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit/Resources/script";
//	CreateArchive_luascript(script.c_str(), path2.c_str(), 100, password);
//	OpenArchive_luascript(script.c_str(), password);
//    
//    TFileData * pData = LoadCertainFile("/Users/riddick/StormLib/test/game.script", "Main.lua", password);
//    if (pData == NULL) {
//        printf("LOAD Main.lua failed \n");
//    }
//    
//    STORM_FREE(pData);
//
//    printf("===== END =====\n");
//    return nError;
//
//}















