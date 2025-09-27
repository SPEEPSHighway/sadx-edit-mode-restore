#include "SADXModLoader.h"
#include "FunctionHook.h"
#include "SetEditor.h"
#include <IniFile.hpp>

extern "C"
{
	static FunctionHook<Bool, task*> OnEdit_hook(OnEdit);
	static FunctionHook<void, Sint32, Sint32, Sint32> njPrintH_hook(njPrintH);
	static FunctionHook<void> ProcessStatusTable_hook(ProcessStatusTable);
	static FunctionHook<LPVOID, Sint32, Sint32> _HeapAlloc_hook(_HeapAlloc);

	HMODULE MEMEMAKERMOD;
	HMODULE ADWINDYVALLEY;

	const char* const characterinitial[] = {
		"S",
		"EG",
		"M",
		"K",
		"TI",
		"A",
		"E",
		"B"
	};


	const char* saveMessage[] = {
	"\aSET File saved!",
	NULL
	};

	const char* errorMessage[] = {
	"\aSave failed.\n\aMake sure the mod's system folder exists.",
	NULL
	};

	/*	ProcessStatusTable's conditions don't work the way they should, resulting in it
		using ProcessStatusTable1P in Edit Mode, instead of ProcessStatusTable2P.
		Which causes distant objects not to spawn. */
	void ProcessStatusTable_fix() {
		if (CheckEditMode()) {
			ProcessStatusTable2P();
			boolOneShot_0 = 1;

		}
		else {
			ProcessStatusTable_hook.Original();
		}
	}


	//SET memory size:
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------

	Bool isSETFile;

	/// <summary>
	/// Determine if file being loaded is a SET file.
	/// </summary>
	/// <param name="no1"></param>
	/// <param name="no2"></param>
	void getIsSETFile(char* no1, char* no2) {

		std::string str = std::string(no2).c_str();

		if (str.find("set") == 0)
			isSETFile = TRUE;
		else
			isSETFile = FALSE;

	}

	/// <summary>
	/// If a SET File, force it to generate space for 1024 objects so the editor doesn't corrupt anything.
	/// </summary>
	/// <param name="count"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	LPVOID forceMaxSETSize(Sint32 count, Sint32 size) {
		if (isSETFile) {
			size = 32800 * (size / 32800 + 1); //Extend, accounting for AD Windy Valley's larger object list.
			isSETFile = FALSE;
		}

		return _HeapAlloc_hook.Original(count, size);
	}

	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------

	std::string modPath;

	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };
	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
		//Get Spawning Objects flag
		modPath = std::string(path);

		const IniFile* config = new IniFile(std::string(path) + "\\config.ini");
		if (!config->getBool("General", "EnableObjects", true)) {
			WriteData<1>((void*)0x46BD95, 0x0);
		}

		//Resurrect OnEdit
		OnEdit_hook.Hook(OnEdit_Full);

		//Fix njPrintH so editor's ANG values can display.
		njPrintH_hook.Hook(njPrintH_fixed);

		//Fix distant objects not spawning while in the object editor.
		ProcessStatusTable_hook.Hook(ProcessStatusTable_fix);

		//Unlike the AutoDemo, the game only creates space for the size of the object list, so it needs to be extended.
		WriteCall((void*)0x42238E, forceMaxSETSize);
		WriteCall((void*)0x42234F, getIsSETFile);

	}

	__declspec(dllexport) void __cdecl OnInitEnd()
	{
		MEMEMAKERMOD = GetModuleHandle(L"MemeMaker");

		ADWINDYVALLEY = GetModuleHandle(L"AutoDemo_WindyValley");
		if (ADWINDYVALLEY)
			sethasADWV();
	}

	/// <summary>
	/// Saves to a SET File in the mod's system folder
	/// </summary>
	void saveSETFile() {
		char path[64];

		//Get player initial, accounting for Super Sonic
		const char* initial = characterinitial[0];
		if (playertwp[0])
			initial = characterinitial[usPlayer];
		if (flgPlayingSuperSonic)
			initial = "L";


		char stg[32];
		switch (ssStageNumber) {
		default: //All action stages
			snprintf(stg, 32, "%02d%02d", ssStageNumber, ssActNumber);
			break;
		case STAGE_EGGMOBILE1:
			snprintf(stg, 32, "EGM1");
			break;
		case STAGE_EGGMOBILE2:
			snprintf(stg, 32, "EGM2");
			break;
		case STAGE_EGGMOBILE3:
			snprintf(stg, 32, "EGM3");
			break;
		case STAGE_EGGMANROBO:
			snprintf(stg, 32, "ZERO");
			break;
		case STAGE_E101:
			snprintf(stg, 32, "R101");
			break;
		case STAGE_E101_R:
			snprintf(stg, 32, "R101R");
			break;
		case STAGE_SS_AFT:
		case STAGE_SS_EVE:
		case STAGE_SS_NIG:
			snprintf(stg, 32, "SS%02d", ssActNumber);
			break;
		case STAGE_EC_ST_AB:
			snprintf(stg, 32, "EC%02d", ssActNumber);
			break;
		case STAGE_EC_C:
			snprintf(stg, 32, "EC%02d", ssActNumber);
			break;
		case STAGE_MR:
			snprintf(stg, 32, "MR%02d", ssActNumber);
			break;
		case STAGE_PAST:
			snprintf(stg, 32, "PAST%02d", ssActNumber);
			break;
		case STAGE_MG_CART:
			snprintf(stg, 32, "MCART%02d", ssActNumber);
			break;
		case STAGE_SHOOTING:
			snprintf(stg, 32, "SHT%d", ssActNumber + 1);
			break;
		case STAGE_SANDBOARD:
			snprintf(stg, 32, "SBOARD%02d", ssActNumber);
			break;
		case STAGE_AL_GARDEN00_SS:
		case STAGE_AL_GARDEN01_EC:
		case STAGE_AL_GARDEN02_MR:
			snprintf(stg, 32, "GARDEN%02d", ssActNumber); //Unused
			break;
		case STAGE_AL_RACE:
			snprintf(stg, 32, "AL_RACE%02d", ssActNumber); //Unused by game
			break;
		}

		snprintf(path, 64, "%s%s%02s%s.BIN", modPath.c_str(), "\\system\\SET", stg, initial);
		FILE* newSet;
		struct stat buf;
		std::string folder = modPath + "\\system";
		if (stat(folder.c_str(), &buf) != -1) {
			if (!fopen_s(&newSet, path, "wb")) {
				if (gpvSetData[ssActNumber]) { //Vast majority of places we can just grab the existing file
					fwrite(gpvSetData[ssActNumber], sizeof(_OBJ_EDITENTRY) * (gpvSetData[ssActNumber]->ssCount + 1), 1, newSet);
				}
				else {
					//Saving for places the temp list was needed (Hedgehog Hammer).
					char editNum[28] = { 0 };
					fwrite(pNumEditEntry, 4, 1, newSet);
					fwrite(&editNum, 28, 1, newSet);
					fwrite(___objEditEntry, sizeof(_OBJ_EDITENTRY) * (*pNumEditEntry), 1, newSet);
				}
				fclose(newSet);
				PrintDebug("\nSET EDITOR: Saved to %s", path);

				dsPlay_oneshot(SE_ITEMGET, (Sint32)editor_tp, 0, 0);
				DisplayHintText(saveMessage, 120);
			}
		}
		else {
			//Directory doesn't exist.
			dsPlay_oneshot(SE_MOTINOR, (Sint32)editor_tp, 0, 0);
			DisplayHintText(errorMessage, 120);
		}
	}

	__declspec(dllexport) void __cdecl OnFrame()
	{
		//If not in-game don't do anything.
		if (ssGameMode != MD_GAME_MAIN) {
			editorEnable();
			DestroyTask(editor_tp);
			editor_tp = 0;
		}
		else if (!editor_tp) {
			editor_tp = CreateElementalTask(2, 2, setEditor);
		}
		else if (ssEditorStatus) {

			//Save
			if (per[0]->on & Buttons_Y && per[0]->press & Buttons_Z) {
				saveSETFile();
			}

			Sint32 col = 2;
			if (MEMEMAKERMOD) {
				njPrintC(NJM_LOCATION(0, col++), "WARNING: Meme Maker's old recreation of the editor conflicts with this one.");
				njPrintC(NJM_LOCATION(0, col++), "Objects will not recognize being selected in the editor. -Speeps");
			}
		}

	}
}