/***************************************************************************

    file                 : grboard.cpp
    created              : Thu Aug 17 23:52:20 CEST 2000
    copyright            : (C) 2000-2014 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: grboard.cpp,v 1.27.2.8 2014/03/04 00:37:17 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <plib/ssg.h>
#include <portability.h>

#include "grcam.h"
#include "grboard.h"
#include "grssgext.h"
#include "grshadow.h"
#include "grskidmarks.h"
#include "grsmoke.h"
#include "grcar.h"
#include "grmain.h"
#include "grutil.h"
#include <robottools.h>
#include <tgfclient.h>

#include "grboard.h"

static float grWhite[4] = {1.0, 1.0, 1.0, 1.0};
static float grRed[4] = {1.0, 0.0, 0.0, 1.0};
static float grBlue[4] = {0.0, 0.0, 1.0, 1.0};
static float grGreen[4] = {0.0, 1.0, 0.0, 1.0};
static float grBlack[4] = {0.0, 0.0, 0.0, 1.0};
static float grDefaultClr[4] = {0.9, 0.9, 0.15, 1.0};

#define NB_BOARDS	3
#define NB_LBOARDS	3
#define NB_CBOARDS	3

static int	Winx	= 0;
static int	Winw	= 800;
static int	Winy	= 0;
static int	Winh	= 600;

cGrBoard::cGrBoard (int myid) {
	id = myid;
	trackMap = NULL;
	Winw = grWinw*600/grWinh;
}


cGrBoard::~cGrBoard () {
	trackMap = NULL;
}


void
cGrBoard::loadDefaults(tCarElt *curCar)
{
	const int BUFSIZE=1024;
	char path[BUFSIZE];
	snprintf(path, BUFSIZE, "%s/%d", GR_SCT_DISPMODE, id);
	
	debugFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_DEBUG, NULL, 1);
	boardFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_BOARD, NULL, 2);
	leaderFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_LEADER, NULL, 1);
	leaderNb	= (int)GfParmGetNum(grHandle, path, GR_ATT_NBLEADER, NULL, 10);
	counterFlag = (int)GfParmGetNum(grHandle, path, GR_ATT_COUNTER, NULL, 1);
	GFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_GGRAPH, NULL, 1);
	arcadeFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_ARCADE, NULL, 0);
	
	trackMap->setViewMode((int) GfParmGetNum(grHandle, path, GR_ATT_MAP, NULL, trackMap->getDefaultViewMode()));
	
	if (curCar->_driverType == RM_DRV_HUMAN) {
		snprintf(path, BUFSIZE, "%s/%s", GR_SCT_DISPMODE, curCar->_name);
		debugFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_DEBUG, NULL, debugFlag);
		boardFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_BOARD, NULL, boardFlag);
		leaderFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_LEADER, NULL, leaderFlag);
		leaderNb	= (int)GfParmGetNum(grHandle, path, GR_ATT_NBLEADER, NULL, leaderNb);
		counterFlag 	= (int)GfParmGetNum(grHandle, path, GR_ATT_COUNTER, NULL, counterFlag);
		GFlag		= (int)GfParmGetNum(grHandle, path, GR_ATT_GGRAPH, NULL, GFlag);
		arcadeFlag	= (int)GfParmGetNum(grHandle, path, GR_ATT_ARCADE, NULL, arcadeFlag);
		trackMap->setViewMode((int) GfParmGetNum(grHandle, path, GR_ATT_MAP, NULL, trackMap->getViewMode()));
	}
}


void
cGrBoard::selectBoard(int val)
{
	const int BUFSIZE=1024;
	char path[BUFSIZE];
	snprintf(path, BUFSIZE, "%s/%d", GR_SCT_DISPMODE, id);
	
	switch (val) {
		case 0:
			boardFlag = (boardFlag + 1) % NB_BOARDS;
			GfParmSetNum(grHandle, path, GR_ATT_BOARD, (char*)NULL, (tdble)boardFlag);
			break;
		case 1:
			counterFlag = (counterFlag + 1) % NB_BOARDS;
			GfParmSetNum(grHandle, path, GR_ATT_COUNTER, (char*)NULL, (tdble)counterFlag);
			break;
		case 2:
			leaderFlag = (leaderFlag + 1) % NB_LBOARDS;
			GfParmSetNum(grHandle, path, GR_ATT_LEADER, (char*)NULL, (tdble)leaderFlag);
			break;
		case 3:
			debugFlag = 1 - debugFlag;
			GfParmSetNum(grHandle, path, GR_ATT_DEBUG, (char*)NULL, (tdble)debugFlag);
			break;	
		case 4:
			GFlag = 1 - GFlag;
			GfParmSetNum(grHandle, path, GR_ATT_GGRAPH, (char*)NULL, (tdble)GFlag);
			break;	
		case 5:
			arcadeFlag = 1 - arcadeFlag;
			GfParmSetNum(grHandle, path, GR_ATT_ARCADE, (char*)NULL, (tdble)arcadeFlag);
			break;	
	}
	GfParmWriteFile(NULL, grHandle, "graph");
}


void
cGrBoard::grDispCarBoard1(tCarElt *car, tSituation *s)
{
	int  x, x2, y;
	const int BUFSIZE=256;
	char buf[BUFSIZE];
	float *clr;
	int dy, dy2, dx;
	
	x = 10;
	x2 = 110;
	dy = GfuiFontHeight(GFUI_FONT_MEDIUM_C);
	dy2 = GfuiFontHeight(GFUI_FONT_SMALL_C);
	y = Winy + Winh - dy - 5;
	snprintf(buf, BUFSIZE, "%d/%d - %s", car->_pos, s->_ncars, car->_name);
	dx = GfuiFontWidth(GFUI_FONT_MEDIUM_C, buf);
	dx = MAX(dx, (x2-x));
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
	glBegin(GL_QUADS);
	glColor4f(0.1, 0.1, 0.1, 0.8);
	glVertex2f(x-5, y + dy);
	glVertex2f(x+dx+5, y + dy);
	glVertex2f(x+dx+5, y-5 - dy2 * 9 /* lines */);
	glVertex2f(x-5, y-5 - dy2 * 9 /* lines */);
	glEnd();
	glDisable(GL_BLEND);
	
	GfuiPrintString(buf, grCarInfo[car->index].iconColor, GFUI_FONT_MEDIUM_C, x, y, GFUI_ALIGN_HL_VB);
	y -= dy;
	
	dy = GfuiFontHeight(GFUI_FONT_SMALL_C);
	
	GfuiPrintString("Fuel:", grWhite, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	if (car->_fuel < 5.0) {
		clr = grRed;
	} else {
		clr = grWhite;
	}
	snprintf(buf, BUFSIZE, "%.1f l", car->_fuel);
	GfuiPrintString(buf, clr, GFUI_FONT_SMALL_C, x2, y, GFUI_ALIGN_HR_VB);
	y -= dy;
	
	if (car->_state & RM_CAR_STATE_BROKEN) {
		clr = grRed;
	} else {
		clr = grWhite;
	}
	
	GfuiPrintString("Damage:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	snprintf(buf, BUFSIZE, "%d", car->_dammage);
	GfuiPrintString(buf, clr, GFUI_FONT_SMALL_C, x2, y, GFUI_ALIGN_HR_VB);
	y -= dy;
	clr = grWhite;
	
	GfuiPrintString("Laps:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	snprintf(buf, BUFSIZE, "%d / %d", car->_laps, s->_totLaps);
	GfuiPrintString(buf, clr, GFUI_FONT_SMALL_C, x2, y, GFUI_ALIGN_HR_VB);
	y -= dy;
	
	GfuiPrintString("Total:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	grWriteTime(clr, GFUI_FONT_SMALL_C, x2, y, s->currentTime, 0);
	y -= dy;
	
	GfuiPrintString("Curr:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	if (!car->_commitBestLapTime) {
		clr = grRed;
	}
	grWriteTime(clr, GFUI_FONT_SMALL_C, x2, y, car->_curLapTime, 0);
	y -= dy;
	clr = grWhite;
	
	GfuiPrintString("Last:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	grWriteTime(clr, GFUI_FONT_SMALL_C, x2, y, car->_lastLapTime, 0);
	y -= dy;
	
	GfuiPrintString("Best:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	grWriteTime(clr, GFUI_FONT_SMALL_C, x2, y, car->_bestLapTime, 0);
	y -= dy;
	
	GfuiPrintString("Penalty:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	grWriteTime(clr, GFUI_FONT_SMALL_C, x2, y, car->_penaltyTime, 0);
	y -= dy;

	GfuiPrintString("Top Speed:", clr, GFUI_FONT_SMALL_C, x, y, GFUI_ALIGN_HL_VB);
	snprintf(buf, BUFSIZE, "%d", (int)(car->_topSpeed * 3.6));
	GfuiPrintString(buf, clr, GFUI_FONT_SMALL_C, x2, y, GFUI_ALIGN_HR_VB);
}

static const char *gearStr[MAX_GEARS] = {"R", "N", "1", "2", "3", "4", "5", "6", "7", "8"};

#define ALIGN_CENTER	0
#define ALIGN_LEFT	1
#define ALIGN_RIGHT	2
void
cGrBoard::initBoard(void)
{
	if (trackMap == NULL) {
		trackMap = new cGrTrackMap();
	}
}

void
cGrBoard::shutdown(void)
{
	if (trackMap != NULL) {
		delete trackMap;
		trackMap = NULL;
	}
}


void grInitBoardCar(tCarElt *car)
{
	const int BUFSIZE=1024;
	char		buf[BUFSIZE];
	int			index;
	void		*handle;
	const char		*param;
	myLoaderOptions	options ;
	tgrCarInfo		*carInfo;
	tgrCarInstrument	*curInst;
	tdble		xSz, ySz, xpos, ypos;
	tdble		needlexSz, needleySz;
	
	ssgSetCurrentOptions ( &options ) ;
	
	index = car->index;	/* current car's index */
	carInfo = &grCarInfo[index];
	handle = car->_carHandle;
	
	/* Tachometer */
	curInst = &(carInfo->instrument[0]);
	
	/* Load the Tachometer texture */
	param = GfParmGetStr(handle, SECT_GROBJECTS, PRM_TACHO_TEX, "rpm8000.rgb");
	snprintf(buf, BUFSIZE, "drivers/%s/%d;drivers/%s;cars/%s;data/textures", car->_modName, car->_driverIndex, car->_modName, car->_carName);
	grFilePath = strdup(buf);
	curInst->texture = (ssgSimpleState*)grSsgLoadTexState(param);
	curInst->texture->ref();
	free(grFilePath);
	
	/* Load the intrument placement */
	xSz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_XSZ, (char*)NULL, 128);
	ySz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_YSZ, (char*)NULL, 128);
	xpos = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_XPOS, (char*)NULL, Winw / 2.0 - xSz);
	ypos = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_YPOS, (char*)NULL, 0);
	needlexSz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_NDLXSZ, (char*)NULL, 50);
	needleySz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_NDLYSZ, (char*)NULL, 2);
	curInst->needleXCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_XCENTER, (char*)NULL, xSz / 2.0) + xpos;
	curInst->needleYCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_YCENTER, (char*)NULL, ySz / 2.0) + ypos;
	curInst->digitXCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_XDIGITCENTER, (char*)NULL, xSz / 2.0) + xpos;
	curInst->digitYCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_YDIGITCENTER, (char*)NULL, 16) + ypos;
	curInst->minValue = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_MINVAL, (char*)NULL, 0);
	curInst->maxValue = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_MAXVAL, (char*)NULL, RPM2RADS(10000)) - curInst->minValue;
	curInst->minAngle = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_MINANG, "deg", 225);
	curInst->maxAngle = GfParmGetNum(handle, SECT_GROBJECTS, PRM_TACHO_MAXANG, "deg", -45) - curInst->minAngle;
	curInst->monitored = &(car->_enginerpm);
	curInst->prevVal = curInst->minAngle;
	
	curInst->CounterList = glGenLists(1);
	glNewList(curInst->CounterList, GL_COMPILE);
	glBegin(GL_TRIANGLE_STRIP);
	{
		glColor4f(1.0, 1.0, 1.0, 0.0);
		glTexCoord2f(0.0, 0.0); glVertex2f(xpos, ypos);
		glTexCoord2f(0.0, 1.0); glVertex2f(xpos, ypos + ySz);
		glTexCoord2f(1.0, 0.0); glVertex2f(xpos + xSz, ypos);
		glTexCoord2f(1.0, 1.0); glVertex2f(xpos + xSz, ypos + ySz);
	}
	glEnd();
	glEndList();
	
	curInst->needleList = glGenLists(1);
	glNewList(curInst->needleList, GL_COMPILE);
	glBegin(GL_TRIANGLE_STRIP);
	{
		glColor4f(1.0, 0.0, 0.0, 1.0);
		glVertex2f(0, -needleySz);
		glVertex2f(0, needleySz);
		glVertex2f(needlexSz, -needleySz / 2.0);
		glVertex2f(needlexSz, needleySz / 2.0);
	}
	glEnd();
	glEndList();
	
	
	/* Speedometer */
	curInst = &(carInfo->instrument[1]);
	
	/* Load the Speedometer texture */
	param = GfParmGetStr(handle, SECT_GROBJECTS, PRM_SPEEDO_TEX, "speed360.rgb");
	snprintf(buf, BUFSIZE, "drivers/%s/%d;drivers/%s;cars/%s;data/textures", car->_modName, car->_driverIndex, car->_modName, car->_carName);
	grFilePath = strdup(buf);
	curInst->texture = (ssgSimpleState*)grSsgLoadTexState(param);
	curInst->texture->ref();
	free(grFilePath);
	
	/* Load the intrument placement */
	xSz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_XSZ, (char*)NULL, 128);
	ySz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_YSZ, (char*)NULL, 128);
	xpos = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_XPOS, (char*)NULL, Winw / 2.0);
	ypos = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_YPOS, (char*)NULL, 0);
	needlexSz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_NDLXSZ, (char*)NULL, 50);
	needleySz = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_NDLYSZ, (char*)NULL, 2);
	curInst->needleXCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_XCENTER, (char*)NULL, xSz / 2.0) + xpos;
	curInst->needleYCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_YCENTER, (char*)NULL, ySz / 2.0) + ypos;
	curInst->digitXCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_XDIGITCENTER, (char*)NULL, xSz / 2.0) + xpos;
	curInst->digitYCenter = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_YDIGITCENTER, (char*)NULL, 10) + ypos; 
	curInst->minValue = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_MINVAL, (char*)NULL, 0);
	curInst->maxValue = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_MAXVAL, (char*)NULL, 100) - curInst->minValue;
	curInst->minAngle = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_MINANG, "deg", 225);
	curInst->maxAngle = GfParmGetNum(handle, SECT_GROBJECTS, PRM_SPEEDO_MAXANG, "deg", -45) - curInst->minAngle;
	curInst->monitored = &(car->_speed_x);
	curInst->prevVal = curInst->minAngle;
	if (strcmp(GfParmGetStr(handle, SECT_GROBJECTS, PRM_SPEEDO_DIGITAL, "yes"), "yes") == 0) {
		curInst->digital = 1;
	}
	
	curInst->CounterList = glGenLists(1);
	glNewList(curInst->CounterList, GL_COMPILE);
	glBegin(GL_TRIANGLE_STRIP);
	{
		glColor4f(1.0, 1.0, 1.0, 0.0);
		glTexCoord2f(0.0, 0.0); glVertex2f(xpos, ypos);
		glTexCoord2f(0.0, 1.0); glVertex2f(xpos, ypos + ySz);
		glTexCoord2f(1.0, 0.0); glVertex2f(xpos + xSz, ypos);
		glTexCoord2f(1.0, 1.0); glVertex2f(xpos + xSz, ypos + ySz);
	}
	glEnd();
	glEndList();
	
	curInst->needleList = glGenLists(1);
	glNewList(curInst->needleList, GL_COMPILE);
	glBegin(GL_TRIANGLE_STRIP);
	{
		glColor4f(1.0, 0.0, 0.0, 1.0);
		glVertex2f(0, -needleySz);
		glVertex2f(0, needleySz);
		glVertex2f(needlexSz, -needleySz / 2.0);
		glVertex2f(needlexSz, needleySz / 2.0);
	}
	glEnd();
	glEndList();
	
}

void grShutdownBoardCar(void)
{
	int i;
	for (i = 0; i < grNbCars; i++) {
		ssgDeRefDelete(grCarInfo[i].instrument[0].texture);	
		ssgDeRefDelete(grCarInfo[i].instrument[1].texture);
		glDeleteLists(grCarInfo[i].instrument[0].needleList, 1);
		glDeleteLists(grCarInfo[i].instrument[1].needleList, 1);
		glDeleteLists(grCarInfo[i].instrument[0].CounterList, 1);
		glDeleteLists(grCarInfo[i].instrument[1].CounterList, 1);
	}
}
