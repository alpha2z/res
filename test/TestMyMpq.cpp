//
//  TestMyMpq.cpp
//  StormLib
//
//  Created by riddick on 14-4-12.
//
//

#include "TestMyMpq.h"
#include "StormLib.h"
#include "StormCommon.h"

static const char * root = "/Users/riddick/StormLib/test";

static unsigned char password[PASSWORD_LENGTH + 1] = "S48B6CDTN5XEQAKQDJNDLJBJ73FDFM3U1111111111111111111111111111111\0";


int main(int argc, char * argv[])
{
    int nError = ERROR_SUCCESS;
   
    // Initialize storage and mix the random number generator
    printf("==== Test Suite for StormLib version %s ====\n", STORMLIB_VERSION_STRING);

    InitMyMpqScriptData(root);
    
    std::string script = "game.script";
//    std::string path = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit_release/Resources/script_lc";
    std::string path = "/Users/riddick/workspace/cocos2dx-20140301/projects/actgamejit/Resources/script";
	CreateArchive_luascript(script.c_str(), path.c_str(), 100, password);
	OpenArchive_luascript(script.c_str(), password);
    
    TFileData * pData = LoadCertainFile("/Users/riddick/StormLib/test/game.script", "Main.lua", password);
    if (pData == NULL) {
        printf("LOAD Main.lua failed \n");
    }

    STORM_FREE(pData);
    
    printf("===== END =====\n");
    return nError;
    
}