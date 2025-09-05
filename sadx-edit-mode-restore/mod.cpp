#include "SADXModLoader.h"
#include "FunctionHook.h"
#include "SetEditor.h"

extern "C"
{
	static FunctionHook<Bool, task*> OnEdit_hook(OnEdit);
	static FunctionHook<void, Sint32, Sint32, Sint32> njPrintH_hook(njPrintH);
	static FunctionHook<void> ProcessStatusTable_hook(ProcessStatusTable);
	static FunctionHook<LPVOID, Sint32, Sint32> _HeapAlloc_hook(_HeapAlloc);

	HMODULE MEMEMAKERMOD;
	HMODULE ADWINDYVALLEY;

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

	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };
	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
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

	__declspec(dllexport) void __cdecl OnFrame()
	{
		//If not in-game don't do anything. TODO: Turn off editor
		if (ssGameMode != MD_GAME_MAIN) {
			editorEnable();
			DestroyTask(editor_tp);
			editor_tp = 0;
		} else if (!editor_tp) {
			editor_tp = CreateElementalTask(2, 2, setEditor);
		}
		else if (ssEditorStatus) {
			Sint32 col = 2;
			if (MEMEMAKERMOD) {
				njPrintC(NJM_LOCATION(0, col++), "WARNING: Meme Maker's old recreation of the editor conflicts with this one.");
				njPrintC(NJM_LOCATION(0, col++), "Objects will not recognize being selected in the editor. -Speeps");
			}
		}
			
	}
}