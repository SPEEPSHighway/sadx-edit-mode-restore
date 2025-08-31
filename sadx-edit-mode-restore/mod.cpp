#include "SADXModLoader.h"
#include "FunctionHook.h"
#include "SetEditor.h"

extern "C"
{
	static FunctionHook<Bool, task*> OnEdit_hook(OnEdit);
	static FunctionHook<void, Sint32, Sint32, Sint32> njPrintH_hook(njPrintH);
	static FunctionHook<void> ProcessStatusTable_hook(ProcessStatusTable);

	HMODULE MEMEMAKERMOD;

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

	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };
	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
		//Resurrect OnEdit
		OnEdit_hook.Hook(OnEdit_Full);

		//Fix njPrintH so editor's ANG values can display.
		njPrintH_hook.Hook(njPrintH_fixed);

		//Fix distant objects not spawning while in the object editor.
		ProcessStatusTable_hook.Hook(ProcessStatusTable_fix);
	}

	__declspec(dllexport) void __cdecl OnInitEnd()
	{
		MEMEMAKERMOD = GetModuleHandle(L"MemeMaker");
	}

	__declspec(dllexport) void __cdecl OnFrame()
	{
		//If not in-game don't do anything. TODO: Turn off editor
		if (ssGameMode != MD_GAME_MAIN) {
			DestroyTask(editor_tp);
			editor_tp = 0;
		} else if (!editor_tp) {
			editor_tp = CreateElementalTask(2, 2, setEditor);
		}
		else if (ssEditorStatus && MEMEMAKERMOD) {
			njPrintC(NJM_LOCATION(0, 2), "WARNING: Meme Maker's old recreation of the editor conflicts with this one.");
			njPrintC(NJM_LOCATION(0, 3), "Objects will not recognize being selected in the editor. -Speeps");
		}
	}
}