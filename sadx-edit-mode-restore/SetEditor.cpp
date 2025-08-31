#include "SADXModLoader.h"

#pragma warning(disable : 4996) //VS hates old sprintf and strcpy

task* editor_tp;

//These are are initialized but never used.
Sint32 leftoverVal1 = 1;
Sint32 leftoverVal2;
Sint32 leftoverVal3;
Sint32 leftoverVal4;

Sint32 scrollTimer;
Sint32 ssCurrentAction;
Sint32 objectsLoaded = 1;
Uint16 objEntryID;
Sint16 ssCurrentSubAction;
Sint16 dispFlag;

Bool boolEditorEnabled = 1;

Float moveSpd = 1.0f;
Float moveSpd2 = 5.0f;
Float max_moveSpd = 20.0f;

taskwk* taskwk_setedit; // Stores a pointer to setedit's taskwk but it never uses it. May have been intended for outside functions.

// Selected task and ocp lists. The one on the cursor is always task_list[0], the rest can only be created in MULTI mode.
task* task_list[32]; 
_OBJ_EDITENTRY ObjEditList[32];
OBJ_CONDITION objList[32];

Sint32 timeInEditor; //Global editor timer used to time the flashing number.

Float incVal_f = 1.0f;
Sint32 incVal_i = 0x10;
Sint32 incID_F = 1;
Sint32 incID_I = 2;

enum ActEnum {
	ACT_PLACE,
	ACT_MULTI,
	ACT_SYSTEM
};

enum SubEnum {
	SUB_POS,
	SUB_ANG,
	SUB_SCL
};

const char* actModeName[] = {
	"PLACE",
	"MULTI",
	"SYSTEM"
};

const char* subModeName[] = {
	"POSITION",
	"ROTATION",
	"SCALE"
};

static void SetEditTask_dest(task* tp) {
	;
}

/// <summary>
/// Creates a new object for the selection.
/// </summary>
/// <param name="twp">Editor's taskwk</param>
/// <param name="id">ID for task_list</param>
static void setEditor_CreateObject(taskwk* twp, Sint16 id)
{
	_OBJ_ITEMENTRY* itemEntry = &pObjItemTable->pObjItemEntry[objEntryID];
	task* new_tp = CreateElementalTask(itemEntry->ucInitMode, itemEntry->ucLevel, itemEntry->fnExec);
	task_list[id] = new_tp;
	if (task_list[id]) {
		objList[id].unionStatus.fRangeOut = itemEntry->fRange + 100.0f;
		objList[id].ssCondition = 1;
		objList[id].ssCondition |= 0x8000;
		objList[id].ssCondition |= 8;
		objList[id].pObjEditEntry = &ObjEditList[id];
		if (boolCommonSet) {
			objList[id].pObjEditEntry->usID = objEntryID;
		}
		else {
			objList[id].pObjEditEntry->usID = 0x8000 | objEntryID;
		}

		Float fRangeOut = 160100.0f;
		if (itemEntry->ssAttribute & 1) {
			fRangeOut = itemEntry->fRange + 100.0f;
		}
		objList[id].unionStatus.fRangeOut = fRangeOut;
		task_list[id]->ocp = &objList[id];
		task_list[id]->dest = SetEditTask_dest;
		taskwk* taskwk_p = task_list[id]->twp;
		if (taskwk_p) {
			taskwk_p->pos = twp->pos;
			taskwk_p->ang = twp->ang;
			taskwk_p->scl = twp->scl;
			taskwk_p->counter.ptr = itemEntry->ptr;
		}
	}
}

/// <summary>
/// Start the editor (After pressing X+A)
/// </summary>
/// <param name="twp"></param>
static void setEditor_Begin(taskwk* twp)
{
	ssEditorStatus = 1;
	ssCurrentAction = 0;
	CameraSetCollisionCameraFunc(CameraModeEditor, 0, 0); //0, 0 added based on original code.
	pTaskWorkEditor = taskwk_setedit; //Added. CameraSetCollisionCameraFunc covered this originally.
	twp->pos = playertwp[0]->pos;
	twp->ang = playertwp[0]->ang;
	setEditor_CreateObject(twp, 0);
	objectsLoaded = 1;
}

/// <summary>
/// Place Object.
/// </summary>
/// <param name="xpos">X Pos</param>
/// <param name="ypos">Y Pos</param>
/// <param name="zpos">Z Pos</param>
/// <param name="rotx">X Ang</param>
/// <param name="roty">Y Ang</param>
/// <param name="rotz">Z Ang</param>
/// <param name="xscl">X Scl</param>
/// <param name="yscl">Y Scl</param>
/// <param name="zscl">Z Scl</param>
static void setEditor_setEditEntry(Float xpos, Float ypos, Float zpos, Sint16 rotx, Sint16 roty, Sint16 rotz, Float xscl, Float yscl, Float zscl)
{
	OBJ_CONDITION* objEntry = &objStatusEntry[numStatusEntry];
	if (objEntry) {
		objEntry->ssCondition = (Sint16)0x8000;
		objEntry->scCount = 0;
		objEntry->scUserFlag = 0;
		_OBJ_EDITENTRY* objEditEntry = &___objEditEntry[*pNumEditEntry]; //This might be overwriting stuff and causing crashes. Don't know a way around it though.
		objEntry->pObjEditEntry = objEditEntry;
		if (boolCommonSet == 0) {
			objEditEntry->usID = objEntryID | 0x8000;
		}
		else {
			objEditEntry->usID = objEntryID;
		}

		objEditEntry->rotx = rotx;
		objEditEntry->roty = roty;
		objEditEntry->rotz = rotz;
		objEditEntry->xpos = xpos;
		objEditEntry->ypos = ypos;
		objEditEntry->zpos = zpos;
		objEditEntry->xscl = xscl;
		objEditEntry->yscl = yscl;
		objEditEntry->zscl = zscl;
		++numStatusEntry;
		++*pNumEditEntry;
	}
}

/// <summary>
/// Borrowed this
/// </summary>
/// <param name="p1"></param>
/// <param name="p2"></param>
/// <returns></returns>
static Float njDistanceP2P_se(NJS_POINT3* p1, NJS_POINT3* p2)
{
	Float y = p1->y - p2->y;
	Float x = p1->x - p2->x;
	Float z = p1->z - p2->z;
	return njSqrt(x * x + y * y + z * z);
}

/// <summary>
/// Grab a nearby object and make it the selected one. (Start+A)
/// </summary>
/// <param name="twp">Editor's taskwk</param>
static void setEditor_setOCP(taskwk* twp)
{
	Float max_dist = 200.0f;
	OBJ_CONDITION* ocp = &objStatusEntry[0];

	for (Sint32 i = 0; i < numStatusEntry; ++i) {
		_OBJ_EDITENTRY* pObjEditEntry = objStatusEntry[i].pObjEditEntry;
		if (!(pObjItemTable->pObjItemEntry[pObjEditEntry->usID & 0xFFF].ssAttribute & 8)) {
			Float distY = njDistanceP2P_se((NJS_POINT3*)&pObjEditEntry->xpos, &twp->pos);
			if (distY < max_dist) {
				ocp = &objStatusEntry[i];
				max_dist = distY;
			}
		}
	}

	if (max_dist < 200.0f) {
		FreeTask(task_list[0]); //Get rid of current selected object

		ObjEditList[0].usID = ocp->pObjEditEntry->usID & 0xFFF;
		objEntryID = ObjEditList[0].usID;
		OBJ_CONDITION* objEntry = &objList[0];
		objEntry->scCount = ocp->scCount;
		objEntry->scUserFlag = ocp->scUserFlag;
		objEntry->ssCondition = ocp->ssCondition;
		objEntry->pTask = ocp->pTask;
		objEntry->pObjEditEntry = ocp->pObjEditEntry;
		objEntry->unionStatus = ocp->unionStatus;
		objEntry->pObjEditEntry = &ObjEditList[0];
		if ((ocp->ssCondition & 1) == 0) {
			setEditor_CreateObject(twp, 0);
		}
		else {
			task_list[0] = ocp->pTask;
		}

		task_list[0]->ocp = &objList[0];
		_OBJ_EDITENTRY* pObjEditEntry = ocp->pObjEditEntry;
		twp->pos.x = pObjEditEntry->xpos;
		twp->pos.y = pObjEditEntry->ypos;
		twp->pos.z = pObjEditEntry->zpos;
		twp->ang.x = (Angle)pObjEditEntry->rotx;
		twp->ang.y = (Angle)pObjEditEntry->roty;
		twp->ang.z = (Angle)pObjEditEntry->rotz;
		twp->scl.x = pObjEditEntry->xscl;
		twp->scl.y = pObjEditEntry->yscl;
		twp->scl.z = pObjEditEntry->zscl;

		OBJ_CONDITION* ocp2 = &objStatusEntry[--numStatusEntry];
		ocp->scCount = ocp2->scCount;
		ocp->scUserFlag = ocp2->scUserFlag;
		ocp->ssCondition = ocp2->ssCondition;
		ocp->pTask = ocp2->pTask;
		ocp->pObjEditEntry = ocp2->pObjEditEntry;
		ocp->unionStatus = ocp2->unionStatus;

		ocp->pObjEditEntry = &___objEditEntry[--*pNumEditEntry];
		if (ocp->pTask) {
			ocp->pTask->ocp = ocp;
		}
	}
}

/// <summary>
/// Release and camera and close the editor.
/// </summary>
static void setEditor_End()
{
	CameraReleaseCollisionCamera();
	cameraSystemWork.G_scCameraLevel = 0; //Added to prevent problems resetting the camera.

	for (Sint32 i = 0; i < 32; ++i) {
		task_list[i] = NULL;
	}
	ssEditorStatus = 0;
}

/// <summary>
/// Adjusts the string for njPrintC with the flashing digit (in Float)
/// </summary>
/// <param name="value">Float value</param>
/// <param name="x">Text X Pos</param>
/// <param name="y">Text Y Pos</param>
/// <param name="increment_pos_id">ID of flashing number</param>
static void njPrintC_FlashFloat(Float* value, Sint32 x, Sint32 y, Sint32 increment_pos_id)
{
	char str[24];

	++timeInEditor;
	sprintf(str, "%08.2f", *value);
	if (increment_pos_id && (timeInEditor & 0x10) != 0) {
		str[5 + -increment_pos_id] = 0x20;
	}
	njPrintC(NJM_LOCATION(x, y), str);
}

void njPrintH_fixed(Sint32 loc, Sint32 val, Sint32 digit);

/// <summary>
/// Prints each number in the ANG value list individually to account for flashing.
/// </summary>
/// <param name="value">Hex Value</param>
/// <param name="xpos">Text X Pos</param>
/// <param name="ypos">Text Y Pos</param>
/// <param name="inc_id">ID of flashing number</param>
static void njPrintH_FlashInt(Uint32* value, Sint32 x, Uint32 y, Sint32 inc_id)
{
	++timeInEditor;

	Uint32 _value = *value;
	for (Uint32 i = 1; i < 9; ++i) {
		/* If not the highlighted number, or if it is the highlight number
		   but not on a frame it disappears, print the number. */
		if (inc_id != i || timeInEditor & 0x10) {
			njPrintH(NJM_LOCATION((x - i) + 8, y), _value & 0xF, 1);
		}
		_value >>= 4;
	}
}

/// <summary>
/// Draws the text in the object editor.
/// </summary>
/// <param name="twp">Editor's taskwk</param>
static void setEditor_DrawHUD(taskwk* twp)
{
	njPrintC(NJM_LOCATION(0, 10), "ACT MODE:");
	njPrintC(NJM_LOCATION(9, 10), actModeName[ssCurrentAction]);
	njPrintC(NJM_LOCATION(0, 11), "SUB MODE:");
	njPrintC(NJM_LOCATION(9, 11), subModeName[ssCurrentSubAction]);
	njPrintC(NJM_LOCATION(0, 12), "ITEM NAME:");
	njPrintC(NJM_LOCATION(10, 12), pObjItemTable->pObjItemEntry[objEntryID].strObjName);

	//POS
	njPrintC(NJM_LOCATION(0, 13), "POS X :");
	njPrintC(NJM_LOCATION(0, 14), "    Y :");
	njPrintC(NJM_LOCATION(0, 15), "    Z :");

	Sint32 flashing;

	//POS X
	if (ssCurrentSubAction == SUB_POS && per[0]->on & Buttons_X)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&twp->pos.x, 8, 13, flashing);

	//POS Y
	if (ssCurrentSubAction == SUB_POS && per[0]->on & Buttons_Y)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&twp->pos.y, 8, 14, flashing);

	//POS Z
	if (ssCurrentSubAction == SUB_POS && per[0]->on & Buttons_B)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&twp->pos.z, 8, 15, flashing);

	//ANG
	njPrintC(NJM_LOCATION(0, 16), "ANG X :");
	njPrintC(NJM_LOCATION(0, 17), "    Y :");
	njPrintC(NJM_LOCATION(0, 18), "    Z :");

	//ANG X
	if (ssCurrentSubAction == SUB_ANG && per[0]->on & Buttons_X)
		flashing = incID_I;
	else
		flashing = 0;
	njPrintH_FlashInt((Uint32*)&twp->ang.x, 8, 16, flashing);

	//ANG Y
	if (ssCurrentSubAction == SUB_ANG && per[0]->on & Buttons_Y)
		flashing = incID_I;
	else
		flashing = 0;
	njPrintH_FlashInt((Uint32*)&twp->ang.y, 8, 17, flashing);

	//ANG Z
	if (ssCurrentSubAction == SUB_ANG && per[0]->on & Buttons_B)
		flashing = incID_I;
	else
		flashing = 0;
	njPrintH_FlashInt((Uint32*)&twp->ang.z, 8, 18, flashing);


	//SCL
	njPrintC(NJM_LOCATION(0, 19), "SCL X :");
	njPrintC(NJM_LOCATION(0, 20), "    Y :");
	njPrintC(NJM_LOCATION(0, 21), "    Z :");

	//SCL X
	if (ssCurrentSubAction == SUB_SCL && per[0]->on & Buttons_X)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&twp->scl.x, 8, 19, flashing);

	//SCL Y
	if (ssCurrentSubAction == SUB_SCL && per[0]->on & Buttons_Y)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&(twp->scl).y, 8, 20, flashing);

	//SCL Z
	if (ssCurrentSubAction == SUB_SCL && per[0]->on & Buttons_B)
		flashing = incID_F;
	else
		flashing = 0;
	njPrintC_FlashFloat(&twp->scl.z, 8, 21, flashing);

	//SET DATA TYPE
	if (boolCommonSet)
		njPrintC(NJM_LOCATION(0, 22), "SET DATA TYPE : COMMON SET");
	else
		njPrintC(NJM_LOCATION(0, 22), "SET DATA TYPE : PLAYER DEPENDENT SET");

}

static NJS_POINT3 axisLine_p[6] = {
	{ -30.0f, 0.0f, 0.0f },
	{  30.0f, 0.0f, 0.0f },
	{  0.0f, -30.0f, 0.0f },
	{  0.0f,  30.0f, 0.0f },
	{  0.0f, 0.0f, -30.0f },
	{  0.0f, 0.0f, 30.0f },
};

static NJS_COLOR axisLine_c[6] = {
	{ 0xFFFFFFFF },
	{ 0xFFFFFFFF },
	{ 0xFFFFFFFF },
	{ 0xFFFFFFFF },
	{ 0xFFFFFFFF },
	{ 0xFFFFFFFF }
};

static NJS_POINT3COL axisLine = { axisLine_p, axisLine_c, 0, 6 };

/// <summary>
/// Draw axis markers around the selected object.
/// </summary>
/// <param name="twp">Editor's taskwk</param>
static void setEditor_DrawLine(taskwk* twp)
{
	njPushMatrix(0);
	njTranslateV(0, &twp->pos);
	njDrawLine3D(&axisLine, 3, 0);
	njPopMatrix(1);
}


/// <summary>
/// Initialize vars
/// </summary>
static void setEditor_Init()
{
	//numStatusEntry = 0; //I assume this function is supposed to be called before the stage loads lol.
	ssEditorStatus = 0;
	taskwk_setedit = 0;

	//None of these do anything and are only referenced here.
	leftoverVal1 = 1;
	leftoverVal2 = 0;
	leftoverVal3 = 0;

	ssCurrentAction = 0;
	ssCurrentSubAction = 0;
	objEntryID = 0;

	for (Sint32 i = 0; i < 32; ++i) {
		task_list[i] = NULL;
	}

	//Another that does nothing
	leftoverVal4 = 0;

	//Value to add/sub when changing pos/ang/scl
	incVal_f = 1.0f;
	incVal_i = 0x10;

	//Default highlighted number for pos+scl/ang
	incID_I = 2;
	incID_F = 1;

	//Editor's global timer. Used to time the flashing numbers.
	timeInEditor = 0;

	//Default object move speed.
	moveSpd = 1.0f;
}

/// <summary>
/// D-Pad and Trigger movement
/// </summary>
/// <param name="twp">Editor's taskwk</param>
static void setEditor_Move(taskwk* twp)
{
	//D-Pad Movement
	if (per[0]->on & (Buttons_Up | Buttons_Down | Buttons_Left | Buttons_Right)) {
		//Priority: Up > Down > Left > Right
		Angle cam_angy = camera_twp->ang.y;

		if (per[0]->on & Buttons_Up) {
			if (per[0]->on & Buttons_Left)
				cam_angy += 0xA000;
			else if (per[0]->on & Buttons_Right)
				cam_angy += 0x6000;
			else
				cam_angy += 0x8000;
		}
		else if (per[0]->on & Buttons_Down) {
			if (per[0]->on & Buttons_Left)
				cam_angy += 0xE000;
			else if (per[0]->on & Buttons_Right)
				cam_angy += 0x2000;
		}
		else {
			if (per[0]->on & Buttons_Left)
				cam_angy += 0xC000;
			else if (per[0]->on & Buttons_Right)
				cam_angy += 0x4000;
		}
		twp->pos.z += moveSpd * njCos(cam_angy);
		twp->pos.x += moveSpd * njSin(cam_angy);
	}


	//Trigger Movement
	if (per[0]->on & (Buttons_L | Buttons_R)) {
		if (per[0]->on & Buttons_L) {
			twp->pos.y += moveSpd;
		}
		else if (per[0]->on & Buttons_R) {
			twp->pos.y -= moveSpd;
		}
	}

	//Update selected object
	if (task_list[0] && task_list[0]->twp) {
		task_list[0]->twp->pos = twp->pos;
		task_list[0]->twp->ang.x = twp->ang.x;
		task_list[0]->twp->ang.z = twp->ang.z;
	}
}

/// <summary>
/// Remove a task from the list
/// </summary>
/// <param name="taskwk">Editor's taskwk (Unused)</param>
/// <param name="id">ID for task_list</param>
static void setEditor_removeID(taskwk* twp, Sint16 id)
{
	if (objEntryID & 0x8000) {
		objEntryID += pObjItemTable->ssCount;
	}
	else if (pObjItemTable->ssCount <= objEntryID) {
		objEntryID -= pObjItemTable->ssCount;
	}
	FreeTask(task_list[id]);
	task_list[id] = NULL;
}

/// <summary>
/// Enable entering the editor. Originally disabled when the editor is active I assume to avoid conflict with the camera editor.
/// </summary>
void editorEnable()
{
	boolEditorEnabled = 1;
}

/// <summary>
/// Disable entering the editor.
/// </summary>
void editorDisable()
{
	boolEditorEnabled = 0;
}

/// <summary>
/// Process editing Pos
/// </summary>
/// <param name="pos">Editor's Position</param>
/// <param name="tp">Object's task</param>
static void subActionPos(NJS_POINT3* pos, task* tp)
{
	if (!tp->twp)
		return;

	taskwk* twp = tp->twp;
	if (per[0]->on & (Buttons_X | Buttons_Y | Buttons_B)) {

		//Move selected number right
		if (per[0]->press & Buttons_Right && incVal_f >= 0.1f) {
			if (--incID_F == 0) {
				incID_F = -1;
			}

			if (0.1f / incVal_f > 0.9f && 0.1f / incVal_f < 1.01f) {
				incVal_f = 0.01f;
			}
			else if (1.0f / incVal_f > 0.9f && 1.0f / incVal_f < 1.01f) {
				incVal_f = 0.1f;
			}
			else if (10.0f / incVal_f > 0.9f && 10.0f / incVal_f < 1.01f) {
				incVal_f = 1.0f;
			}
			else if (100.0f / incVal_f > 0.9f && 100.0f / incVal_f < 1.01f) {
				incVal_f = 10.0f;
			}
			else if (1000.0f / incVal_f > 0.9f && 1000.0f / incVal_f < 1.01f) {
				incVal_f = 100.0f;
			}
		}

		//Move selected number left
		if (per[0]->press & Buttons_Left && incVal_f <= 101.0f) {
			if (++incID_F == 0) {
				incID_F = 1;
			}

			if (0.01f / incVal_f > 0.9f && 0.01f / incVal_f < 1.01f) {
				incVal_f = 0.1f;
			}
			else if (0.1f / incVal_f > 0.9f && 0.1f / incVal_f < 1.01f) {
				incVal_f = 1.0f;
			}
			else if (1.0f / incVal_f > 0.9f && 1.0f / incVal_f < 1.01f) {
				incVal_f = 10.0f;
			}
			else if (10.0f / incVal_f > 0.9f && 10.0f / incVal_f < 1.01f) {
				incVal_f = 100.0f;
			}
			else if (100.0f / incVal_f > 0.0f && 100.0f / incVal_f < 1.1f) {
				incVal_f = 1000.0f;
			}
		}

		//Get addr of pos to change
		Float* pos_to_change = &pos->x;
		if (!(per[0]->on & Buttons_X)) {
			if (per[0]->on & Buttons_Y)
				pos_to_change = &pos->y;
			else if (per[0]->on & Buttons_B)
				pos_to_change = &pos->z;
		}

		//Ues SWDATAE[0] to handle wait between inputs as it only holds a value for 1 frame per second.
		if (SWDATAE[0] & Buttons_Up)
			*pos_to_change += incVal_f;
		else if (SWDATAE[0] & Buttons_Down)
			*pos_to_change -= incVal_f;

		twp->pos = *pos;
	}
}

/// <summary>
/// Process editing ang
/// </summary>
/// <param name="ang">Editor's Angle</param>
/// <param name="tp">Object's task</param>
static void subActionAng(Angle3* ang, task* tp)
{
	if (!tp->twp)
		return;

	taskwk* twp = tp->twp;
	if (per[0]->on & (Buttons_X | Buttons_Y | Buttons_B)) {

		//Move selected number right
		if (per[0]->press & Buttons_Right && incVal_i > 0x1) {
			incVal_i >>= 4;
			--incID_I;
		}

		//Move selected number left
		if (per[0]->press & Buttons_Left && incVal_i < 0x10000000) {
			incVal_i <<= 4;
			++incID_I;
		}

		//Get addr of ang to change
		Sint32* ang_to_change = &ang->x;
		if (!(per[0]->on & Buttons_X)) {
			if (per[0]->on & Buttons_Y)
				ang_to_change = &ang->y;
			else if (per[0]->on & Buttons_B)
				ang_to_change = &ang->z;
		}

		//Uses SWDATAE[0] to handle wait between inputs as it only holds a value for 1 frame per second.
		if (SWDATAE[0] & Buttons_Up)
			*ang_to_change += incVal_i;
		else if (SWDATAE[0] & Buttons_Down)
			*ang_to_change -= incVal_i;

		twp->ang = *ang;
	}
}

/// <summary>
/// Process editing Scl. Same as pos except it changes scl.
/// </summary>
/// <param name="scl">Editor's scl</param>
/// <param name="tp">Object's task</param>
static void subActionScl(NJS_POINT3* scl, task* tp)
{
	if (!tp->twp)
		return;

	taskwk* twp = tp->twp;
	if (per[0]->on & (Buttons_X | Buttons_Y | Buttons_B)) {

		//Move selected number right
		if (per[0]->press & Buttons_Right && incVal_f >= 0.1f) {
			if (--incID_F == 0) {
				incID_F = -1;
			}

			if (0.1f / incVal_f > 0.9f && 0.1f / incVal_f < 1.01f) {
				incVal_f = 0.01f;
			}
			else if (1.0f / incVal_f > 0.9f && 1.0f / incVal_f < 1.01f) {
				incVal_f = 0.1f;
			}
			else if (10.0f / incVal_f > 0.9f && 10.0f / incVal_f < 1.01f) {
				incVal_f = 1.0f;
			}
			else if (100.0f / incVal_f > 0.9f && 100.0f / incVal_f < 1.01f) {
				incVal_f = 10.0f;
			}
			else if (1000.0f / incVal_f > 0.9f && 1000.0f / incVal_f < 1.01f) {
				incVal_f = 100.0f;
			}
		}

		//Move selected number left
		if (per[0]->press & Buttons_Left && incVal_f <= 101.0f) {
			if (++incID_F == 0) {
				incID_F = 1;
			}

			if (0.01f / incVal_f > 0.9f && 0.01f / incVal_f < 1.01f) {
				incVal_f = 0.1f;
			}
			else if (0.1f / incVal_f > 0.9f && 0.1f / incVal_f < 1.01f) {
				incVal_f = 1.0f;
			}
			else if (1.0f / incVal_f > 0.9f && 1.0f / incVal_f < 1.01f) {
				incVal_f = 10.0f;
			}
			else if (10.0f / incVal_f > 0.9f && 10.0f / incVal_f < 1.01f) {
				incVal_f = 100.0f;
			}
			else if (100.0f / incVal_f > 0.0f && 100.0f / incVal_f < 1.1f) {
				incVal_f = 1000.0f;
			}
		}

		//Get addr of scl to change
		Float* scl_to_change = &scl->x;
		if (!(per[0]->on & Buttons_X)) {
			if (per[0]->on & Buttons_Y)
				scl_to_change = &scl->y;
			else if (per[0]->on & Buttons_B)
				scl_to_change = &scl->z;
		}

		//Uses SWDATAE[0] to handle wait between inputs as it only holds a value for 1 frame per second.
		if (SWDATAE[0] & Buttons_Up)
			*scl_to_change += incVal_f;
		else if (SWDATAE[0] & Buttons_Down)
			*scl_to_change -= incVal_f;

		twp->scl = *scl;
	}
}


/// <summary>
/// Toggle flag for displaying the editor
/// </summary>
/// <param name="mode">On/Off</param>
/// <returns></returns>
static Bool setEditor_SetDisplayFlag(Sint16 mode)
{
	if (mode == 0) {
		if (dispFlag == 0) {
			return 0;
		}
		dispFlag = 0;
	}
	else {
		if (dispFlag != 0) {
			return 0;
		}
		dispFlag = -1;
	}
	return 1;
}

/// <summary>
/// Destructor for setEditor
/// </summary>
/// <param name="tp">Editor's task</param>
static void setEditor_dest(task* tp)
{
	if (tp->twp->mode == 3 || tp->twp->mode == 4 || tp->twp->mode == 5) { //Only one of the modes the editor is open.
		setEditor_SetDisplayFlag(0);
	}

}

/// <summary>
/// Edit Mode painstakingly ported from the AutoDemo.
/// </summary>
/// <param name="tp">Editor's task</param>
void setEditor(task* tp)
{
	taskwk* taskwk_p = tp->twp;
	Sint8 mode_p = taskwk_p->mode;
	switch (taskwk_p->mode) {
	case 0: //Initialize
		setEditor_Init();
		taskwk_p->mode = 1;
		taskwk_setedit = taskwk_p;
		tp->dest = setEditor_dest;
		break;
	case 1: //Wait for input to enter editor
		if (per[0]->on & Buttons_X && per[0]->press & Buttons_A && setEditor_SetDisplayFlag(1)) {
			taskwk_p->mode = 3;
		}
		break;
	case 3: //Enter editor
		taskwk_p->mode = 4;
		setEditor_Begin(taskwk_p);
		PadReadOff();
		editorDisable();
		break;
	case 4: //Editor running
	{

		//Added the below lines because CameraModeEditor only reads P4's controller in the PC version.
		//Also the text is tiny
		njPrintSize((Uint16)(((Float)HorizontalResolution / 640.0f) * 8.0f));
		per[3] = per[0];


		taskwk_p = tp->twp;


		pTaskWorkEditor = taskwk_setedit; //Added to prevent a crash in Lost World.

		setEditor_DrawHUD(taskwk_p);

		zxsdwstr shadowstr{};
		switch (ssCurrentAction) {
		case ACT_PLACE:
		{
			if (task_list[0] == 0) { //If the task doesn't exist, create it. Also used when the list ID changes.
				setEditor_CreateObject(taskwk_p, 0);
			}
			Float onpos_min = max_moveSpd;

			//Read input
			if (per[0]->on & Buttons_Start) { // Start button held
				if (per[0]->press & Buttons_A) { // Start+A (Grab Object)
					setEditor_setOCP(taskwk_p);
				}
				else if (per[0]->press & Buttons_B) { // Start+B (Place Object)
					setEditor_setEditEntry(taskwk_p->pos.x, taskwk_p->pos.y, taskwk_p->pos.z, taskwk_p->ang.x,
						taskwk_p->ang.y, taskwk_p->ang.z, taskwk_p->scl.x, taskwk_p->scl.y, taskwk_p->scl.z);
				}
				else if (per[0]->press & Buttons_X) { // Start+X (Snap to floor, rough)
					shadowstr.pos = taskwk_p->pos;
					ListGroundForCollision(taskwk_p->pos.x, taskwk_p->pos.y, taskwk_p->pos.z, max_moveSpd);

					for (Sint32 i = 0; i < numLandCollList; ++i) { // Find ground
						if (GetZxShadowOnFDPolygon(&shadowstr, LandCollList[i].pObject)) {
							taskwk_p->pos.y = shadowstr.lower.onpos;
							break;
						}
					}
				}
				else if (per[0]->press & Buttons_Y) { // Start+Y (Snap to floor and set ang x/z)
					onpos_min = taskwk_p->pos.y;
					Float onpos_max = onpos_min + 30.0f;
					onpos_min = onpos_min + -10000.0f;
					shadowstr.pos = taskwk_p->pos;

					ListGroundForCollision(taskwk_p->pos.x, taskwk_p->pos.y, taskwk_p->pos.z, max_moveSpd);

					for (Sint32 i = 0; i < numLandCollList; ++i) { // Find ground
						if (((GetZxShadowOnFDPolygon(&shadowstr, LandCollList[i].pObject)
							&& (shadowstr.lower.findflag))
							&& (onpos_min < shadowstr.lower.onpos))
							&& (shadowstr.lower.onpos < onpos_max)) {
							taskwk_p->pos.y = shadowstr.lower.onpos;
							taskwk_p->ang.x = shadowstr.lower.angx;
							taskwk_p->ang.z = shadowstr.lower.angz;
							onpos_min = shadowstr.lower.onpos;
						}
					}
				}
			}
			else if (per[0]->on & Buttons_A) { //A Button Held
				if (per[0]->press & (Buttons_Up | Buttons_Down) || per[0]->on & (Buttons_Left | Buttons_Right)) {
					if (per[0]->press & Buttons_Up) { //A+Up (Previous item in object list)
						--objEntryID;
					}
					else if (per[0]->press & Buttons_Down) { //A+Down (Next item in object list)
						++objEntryID;
					}
					else if (per[0]->on & Buttons_Right) { // A+Right (Scroll up object list fast)
						if (++scrollTimer > 4) {
							scrollTimer = 0;
							++objEntryID;
						}
					}
					else if (per[0]->on & Buttons_Left) { // A+Left (Scroll down object list fast)
						if (++scrollTimer > 4) {
							scrollTimer = 0;
							--objEntryID;
						}
					}

					//Delete the old object so the new one can spawn.
					setEditor_removeID(taskwk_p, 0);
					break;
				}
				else if (per[0]->press & Buttons_X) { // A+X (Switch sub mode)
					if (++ssCurrentSubAction > 2) {
						ssCurrentSubAction = 0;
					}
				}
				else if (per[0]->press & Buttons_B) { //A+B (Switch act mode)
					if (++ssCurrentAction > 2) {
						ssCurrentAction = 0;
					}

					//Delete the selected object
					FreeTask(task_list[0]);
					objectsLoaded = 0;
					task_list[0] = 0;
				}
				else if (!(per[0]->press & Buttons_Y)) { //Y Not Pressed
					if (per[0]->press & Buttons_R) { //A+R (Speed up movement)
						moveSpd += 2.0f;
						if (max_moveSpd < moveSpd) {
							moveSpd = onpos_min;
						}
					}
					else if (((per[0]->press & Buttons_L) != 0) //A+L (Slow down movement)
						&& (onpos_min = moveSpd - 2.0f, moveSpd = onpos_min, onpos_min < 1.0f)) {
						moveSpd = 1.0f;
					}
				}
			}
			else { //A Button not held
				//Edit values if one of the relevant buttons are pressed, otherwise process movement.
				if (per[0]->on & (Buttons_X | Buttons_Y | Buttons_B)) {
					switch (ssCurrentSubAction) {
					case SUB_POS:
						subActionPos(&taskwk_p->pos, task_list[0]);
						break;
					case SUB_ANG:
						subActionAng(&taskwk_p->ang, task_list[0]);
						break;
					case SUB_SCL:
						subActionScl(&taskwk_p->scl, task_list[0]);
						break;
					}
				}
				else {
					setEditor_Move(taskwk_p);
				}
			}
			setEditor_DrawLine(taskwk_p);
			break;
		}
		case ACT_MULTI:
		{
			//Scan task_list to find an empty ID.
			for (Sint32 i = 0; i < objectsLoaded; ++i) {
				if (!task_list[i]) {
					setEditor_CreateObject(taskwk_p, i);
				}
			}

			Float onpos_min = moveSpd;

			//Process input
			if (per[0]->on & Buttons_Start) { // Start button held
				if (per[0]->press & Buttons_A) { // Start+A (Create new object)
					if (objectsLoaded < 0x10) {
						setEditor_CreateObject(taskwk_p, objectsLoaded++);
					}
				}
				else {
					if (per[0]->press & Buttons_B) { // Start+B (Delete object)
						if (objectsLoaded > 0) {
							--objectsLoaded;
							FreeTask(task_list[objectsLoaded]);
							task_list[objectsLoaded] = 0;
						}
					}
					else if (per[0]->press & Buttons_X) { // Start+X (Increase move speed)
						moveSpd += 0.5f;
					}
					else if (per[0]->press & Buttons_Y && (onpos_min = moveSpd - 0.5f, moveSpd = onpos_min, onpos_min < 0.0f)) { // Start+Y (Decrease move speed)
						moveSpd = 0;
					}
				}
			}
			else if (per[0]->on & Buttons_A) { //A Button held
				if (per[0]->press & (Buttons_Up | Buttons_Down) || per[0]->on & (Buttons_Left | Buttons_Right)) {
					if (per[0]->press & Buttons_Up) { // A+Up
						--objEntryID;
						for (Sint32 i = 0; i < objectsLoaded; ++i) {
							setEditor_removeID(taskwk_p, i);
						}
					}
					else if (per[0]->press & Buttons_Down) { // A+Down
						++objEntryID;
						for (Sint32 i = 0; i < objectsLoaded; ++i) {
							setEditor_removeID(taskwk_p, i);
						}
					}
					else if (per[0]->on & Buttons_Right) { // A+Right
						moveSpd2 += 0.5f;
						if (moveSpd < moveSpd2) {
							moveSpd2 = onpos_min;
						}
					}
					else if (per[0]->on & Buttons_Left && (onpos_min = moveSpd2 - 0.5f, moveSpd2 = onpos_min, onpos_min < 0.0f)) { // A+Left
						moveSpd2 = 0.0f;
					}
				}
				else {
					if (per[0]->press & Buttons_X) { // A+X
						if (++ssCurrentSubAction > 2) {
							ssCurrentSubAction = 0;
						}
					}
					else if (per[0]->press & Buttons_B) { // A+B
						if (2 < ++ssCurrentAction) {
							ssCurrentAction = 0;
						}

						//Delete all tasks in the editor's list
						for (Sint32 i = 0; i < objectsLoaded; ++i) {
							FreeTask(task_list[i]);
							task_list[i] = 0;
						}
						objectsLoaded = 0;
					}
					else if (per[0]->press & Buttons_Y) { // A+Y
						for (Sint32 i = 0; i < objectsLoaded; ++i) {
							taskwk* new_twp = task_list[i]->twp;
							setEditor_setEditEntry(new_twp->pos.x, new_twp->pos.y, new_twp->pos.z, new_twp->ang.x,
								new_twp->ang.y, new_twp->ang.z, new_twp->scl.x, new_twp->scl.y, new_twp->scl.z);
						}
					}
					else if (per[0]->press & Buttons_R) { // A+R
						moveSpd += 2.0f;
						if (20.0f < moveSpd) {
							moveSpd = 20.0f;
						}
					}
					else if (per[0]->press & Buttons_L && (moveSpd -= 2.0f, moveSpd < 1.0f)) { //A+L
						moveSpd = 1.0f;
					}
				}
			}
			else {
				//Edit values if one of the relevant buttons are pressed, otherwise process movement.
				if (task_list[0] && per[0]->on & (Buttons_X | Buttons_Y | Buttons_B)) {
					switch (ssCurrentSubAction) {
					case SUB_POS:
						subActionPos(&taskwk_p->pos, task_list[0]);
						break;
					case SUB_ANG:
						subActionAng(&taskwk_p->ang, task_list[0]);
						break;
					case SUB_SCL:
						subActionScl(&taskwk_p->scl, task_list[0]);
						break;
					}
				}
				else {
					setEditor_Move(taskwk_p);
				}
			}
			setEditor_DrawLine(taskwk_p);

			//Huge variable overlap in this area. I don't think it even works correctly in the AutoDemo itself.
			//Does in Preview though.

			NJS_POINT3* pos = &taskwk_p->pos;
			for (Sint32 i = 1; i < objectsLoaded; ++i) {
				if (task_list[i] && task_list[i]->twp) {
					pos = &task_list[i]->twp->pos;

					pos->x = pos->x - moveSpd2 * (Float)i * njSin(camera_twp->ang.y);
					pos->z = pos->z - moveSpd2 * (Float)i * njCos(camera_twp->ang.y);

					Float onpos_max = taskwk_p->pos.y;
					onpos_max += -100.0f;

					shadowstr.pos = taskwk_p->pos;
					ListGroundForCollision(pos->x, pos->y, pos->z, 100.0f);

					Float xmov1;
					for (Sint32 j = 0; j < numLandCollList; ++j) {
						/* NOTE: The AutoDemo uses a ShadowOnPolygon function that doesn't exist in the final game.
						   I substituted GetZxShadowOnPolygon for it since it's similar, but it's not exact so there may be bugs. */
						if (GetZxShadowOnPolygon(&shadowstr, LandCollList[j].pObject, LandCollList[j].slAttribute)
							&& (xmov1 = shadowstr.lower.onpos, onpos_max < xmov1)) {
							pos->y = xmov1;
							onpos_max = xmov1;
						}
					}

					task_list[i]->twp->pos.x = pos->x;
					task_list[i]->twp->pos.y = pos->y + moveSpd;
					task_list[i]->twp->pos.z = pos->z;
				}
			}
			break;
		}
		case ACT_SYSTEM:
		{
			if (per[0]->on & Buttons_A) {
				if (per[0]->press & Buttons_B) { // A+B (Change Act Mode~)
					if (++ssCurrentAction > 2) {
						ssCurrentAction = 0;
					}
					objectsLoaded = 1;
					setEditor_CreateObject(taskwk_p, 0);
				}
				else if (per[0]->press & Buttons_X) { // A+X (Exit)
					setEditor_SetDisplayFlag(0);
					taskwk_p->mode = 5;
				}
			}
			else if (per[0]->press & Buttons_B) { // B (?)
				//Go through objStatusEntry and set ssCondition.
				for (OBJ_CONDITION* ocp_2 = &objStatusEntry[numStatusEntry - 1]; ocp_2 >= &objStatusEntry[0]; --ocp_2) {
					ocp_2->ssCondition |= 0x8000;
				}
			}
			else if (per[0]->press & Buttons_X) { // X (Common Set switch)
				boolCommonSet = boolCommonSet != 1;
			}
			break;
		}
		default:
			ssCurrentAction = 0;
			break;
		}
		break;
	}
	case 5: //Exit Editor
		taskwk_p->mode = 1;
		setEditor_End();
		PadReadOn();
		editorEnable();
		break;
	default:
		taskwk_p->mode = 0;
		break;
	}
}

/// <summary>
/// Check if a task is selected by the object editor: Non-empty version.
/// </summary>
/// <param name="tp">Object's task</param>
/// <returns>True if this is the selected object in the editor</returns>
Bool OnEdit_Full(task* tp)
{
	for (Sint32 i = 0; i < objectsLoaded; ++i) {
		if (tp == task_list[i])
			return TRUE;
	}
	return FALSE;
}

/// <summary>
/// custom njPrintH as the original doesn't work on PC.
/// </summary>
/// <param name="position"></param>
/// <param name="value"></param>
/// <param name="numdigits"></param>
/// <returns></returns>
void njPrintH_fixed(Sint32 loc, Sint32 val, Sint32 digit)
{
	char dst[128];
	char fmt[8];
	char c;

	strcpy(fmt, "%00X");

	c = digit;
	if (digit > 9)
	{
		c = 9;
	}
	fmt[2] += c;

	sprintf(dst, fmt, val);

	njPrintC(loc, dst);
}

