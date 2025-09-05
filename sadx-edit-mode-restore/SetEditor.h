#pragma once

#include "SADXModLoader.h"

extern task* editor_tp;

//AutoDemo
void editorEnable();
void editorDisable();
void setEditor(task* tp);
Bool OnEdit_Full(task* tp);

//Custom
void njPrintH_fixed(Sint32 loc, Sint32 val, Sint32 digit);
void sethasADWV();
