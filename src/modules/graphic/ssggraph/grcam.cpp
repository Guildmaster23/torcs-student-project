/***************************************************************************

    file                 : grcam.cpp
    created              : Mon Aug 21 20:55:32 CEST 2000
    copyright            : (C) 2000 by Eric Espie
    email                : torcs@free.fr
    version              : $Id: grcam.cpp,v 1.50.2.4 2012/09/19 21:10:34 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <plib/ssg.h>

#include <robottools.h>
#include <portability.h>
#include <graphic.h>
#include "grcam.h"
#include "grscreen.h"
#include "grscene.h"
#include "grshadow.h"
#include "grskidmarks.h"
#include "grsmoke.h"
#include "grcar.h"
#include "grmain.h"
#include "grutil.h"
#include <tgfclient.h>

float
cGrCamera::getDist2 (tCarElt *car)
{
    float dx = car->_pos_X - eye[0];
    float dy = car->_pos_Y - eye[1];

    return dx * dx + dy * dy;
}


static void
grMakeLookAtMat4 ( sgMat4 dst, const sgVec3 eye, const sgVec3 center, const sgVec3 up )
{
  // Caveats:
  // 1) In order to compute the line of sight, the eye point must not be equal
  //    to the center point.
  // 2) The up vector must not be parallel to the line of sight from the eye
  //    to the center point.

  /* Compute the direction vectors */
  sgVec3 x,y,z;

  /* Y vector = center - eye */
  sgSubVec3 ( y, center, eye ) ;

  /* Z vector = up */
  sgCopyVec3 ( z, up ) ;

  /* X vector = Y cross Z */
  sgVectorProductVec3 ( x, y, z ) ;

  /* Recompute Z = X cross Y */
  sgVectorProductVec3 ( z, x, y ) ;

  /* Normalize everything */
  sgNormaliseVec3 ( x ) ;
  sgNormaliseVec3 ( y ) ;
  sgNormaliseVec3 ( z ) ;

  /* Build the matrix */
#define M(row,col)  dst[row][col]
  M(0,0) = x[0];    M(0,1) = x[1];    M(0,2) = x[2];    M(0,3) = 0.0;
  M(1,0) = y[0];    M(1,1) = y[1];    M(1,2) = y[2];    M(1,3) = 0.0;
  M(2,0) = z[0];    M(2,1) = z[1];    M(2,2) = z[2];    M(2,3) = 0.0;
  M(3,0) = eye[0];  M(3,1) = eye[1];  M(3,2) = eye[2];  M(3,3) = 1.0;
#undef M
}


cGrPerspCamera::cGrPerspCamera(class cGrScreen *myscreen, int id, int drawCurr, int drawDrv, int drawBG, int mirrorAllowed,
			       float myfovy, float myfovymin, float myfovymax,
			       float myfnear, float myffar, float myfogstart, float myfogend)
    : cGrCamera(myscreen, id, drawCurr, drawDrv, drawBG, mirrorAllowed)
{
    fovy     = myfovy;
    fovymin  = myfovymin;
    fovymax  = myfovymax;
    fnear    = myfnear;
    ffar     = myffar;
    fovydflt = myfovy;
    fogstart = myfogstart;
    fogend   = myfogend;
    
}

void cGrPerspCamera::setProjection(void)
{
    // PLib takes the field of view as angles in degrees. However, the
    // aspect ratio really aplies to lengths in the projection
    // plane. So we have to transform the fovy angle to a length in
    // the projection plane, apply the aspect ratio and transform the
    // result back to an angle. Care needs to be taken to because the
    // tan and atan functions operate on angles in radians. Also,
    // we're only interested in half the viewing angle.
    float fovx = atan(screen->getViewRatio() * tan(fovy * M_PI / 360.0)) * 360.0 / M_PI;
    grContext.setFOV(fovx, fovy);
    grContext.setNearFar(fnear, ffar);
}

void cGrPerspCamera::setModelView(void)
{
  sgMat4 mat;
  grMakeLookAtMat4(mat, eye, center, up);
  
  grContext.setCamera(mat);
}

void cGrPerspCamera::loadDefaults(char *attr)
{
	const int BUFSIZE=1024;
	char path[BUFSIZE];
	
	snprintf(path, BUFSIZE, "%s/%d", GR_SCT_DISPMODE, screen->getId());
	fovy = (float)GfParmGetNum(grHandle, path, attr, (char*)NULL, fovydflt);
	limitFov();
}


/* Give the height in pixels of 1 m high object on the screen at this point */
float cGrPerspCamera::getLODFactor(float x, float y, float z) {
    tdble	dx, dy, dz, dd;
    float	ang;
    int		scrh, dummy;
    float	res;

    dx = x - eye[0];
    dy = y - eye[1];
    dz = z - eye[2];

    dd = sqrt(dx*dx+dy*dy+dz*dz);

    ang = DEG2RAD(fovy / 2.0);
    GfScrGetSize(&dummy, &scrh, &dummy, &dummy);
    
    res = (float)scrh / 2.0 / dd / tan(ang);
    if (res < 0) {
	res = 0;
    }
    return res;
}

void cGrPerspCamera::setZoom(int cmd)
{
	const int BUFSIZE=256;
	char buf[BUFSIZE];
	const int PATHSIZE=1024;
	char path[PATHSIZE];


	switch(cmd) {
		case GR_ZOOM_IN:
			if (fovy > 2) {
				fovy--;
			} else {
				fovy /= 2.0;
			}
			if (fovy < fovymin) {
				fovy = fovymin;
			}
			break;

		case GR_ZOOM_OUT:
			fovy++;
			if (fovy > fovymax) {
				fovy = fovymax;
			}
			break;

		case GR_ZOOM_MAX:
			fovy = fovymax;
			break;

		case GR_ZOOM_MIN:
			fovy = fovymin;
			break;

		case GR_ZOOM_DFLT:
			fovy = fovydflt;
			break;
	}

	limitFov();

	snprintf(buf, BUFSIZE, "%s-%d-%d", GR_ATT_FOVY, screen->getCurCamHead(), getId());
	snprintf(path, PATHSIZE, "%s/%d", GR_SCT_DISPMODE, screen->getId());
	GfParmSetNum(grHandle, path, buf, (char*)NULL, (tdble)fovy);
	GfParmWriteFile(NULL, grHandle, "Graph");
}

void cGrOrthoCamera::setProjection(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(left, right, bottom, top);
}

void cGrOrthoCamera::setModelView(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void cGrBackgroundCam::update(cGrCamera *curCam)
{
    static float BACKGROUND_FOVY_CUTOFF = 60.0f;
    memcpy(&eye, curCam->getPosv(), sizeof(eye));
    memcpy(&center, curCam->getCenterv(), sizeof(center));
    sgSubVec3(center, center, eye);
    sgSetVec3(eye, 0, 0, 0);
    speed[0]=0.0;
    speed[1]=0.0;
    speed[2]=0.0;
    fovy = curCam->getFovY();
    if (fovy < BACKGROUND_FOVY_CUTOFF) {
        fovy = BACKGROUND_FOVY_CUTOFF;
    }
    memcpy(&up, curCam->getUpv(), sizeof(up));
}



class cGrCarCamInside : public cGrPerspCamera
{
 public:
    cGrCarCamInside(class cGrScreen *myscreen, int id, int drawCurr, int drawBG,
		    float myfovy, float myfovymin, float myfovymax,
		    float myfnear, float myffar = 1500.0,
		    float myfogstart = 800.0, float myfogend = 1500.0)
	: cGrPerspCamera(myscreen, id, drawCurr, 0, drawBG, 1,
			 myfovy, myfovymin, myfovymax,
			 myfnear, myffar, myfogstart, myfogend) {
	up[0] = 0;
	up[1] = 0;
	up[2] = 1;
    }

    void update(tCarElt *car, tSituation *s) {
	sgVec3 P, p;
	
	p[0] = car->_drvPos_x;
	p[1] = car->_drvPos_y;
	p[2] = car->_drvPos_z;
	sgXformPnt3(p, car->_posMat);
	
	eye[0] = p[0];
	eye[1] = p[1];
	eye[2] = p[2];

	P[0] = car->_drvPos_x + 30.0;
	P[1] = car->_drvPos_y;
	P[2] = car->_drvPos_z;
	sgXformPnt3(P, car->_posMat);

	center[0] = P[0];
	center[1] = P[1];
	center[2] = P[2];

	speed[0] =car->pub.DynGCg.vel.x;  
	speed[1] =car->pub.DynGCg.vel.y;  
	speed[2] =car->pub.DynGCg.vel.z;
    }
};


/* MIRROR */
cGrCarCamMirror::~cGrCarCamMirror ()
{
    glDeleteTextures (1, &tex);
    delete viewCam;
}


void cGrCarCamMirror::limitFov(void) {
    fovy = 90.0 / screen->getViewRatio();
}


void cGrCarCamMirror::update(tCarElt *car, tSituation * /* s */)
{
    sgVec3 P, p;

    P[0] = car->_bonnetPos_x;
    P[1] = car->_bonnetPos_y;
    P[2] = car->_bonnetPos_z;
    sgXformPnt3(P, car->_posMat);
	
    eye[0] = P[0];
    eye[1] = P[1];
    eye[2] = P[2];
	
    p[0] = car->_bonnetPos_x - 30.0;
    p[1] = car->_bonnetPos_y;
    p[2] = car->_bonnetPos_z;
    sgXformPnt3(p, car->_posMat);

    center[0] = p[0];
    center[1] = p[1];
    center[2] = p[2];

    up[0] = car->_posMat[2][0];
    up[1] = car->_posMat[2][1];
    up[2] = car->_posMat[2][2];
}


void cGrCarCamMirror::setViewport(int x, int y, int w, int h)
{
	vpx = x;
	vpy = y;
	vpw = w;
	vph = h;

	if (viewCam) {
		delete viewCam;
	}
	viewCam = new cGrOrthoCamera(screen, x,  x + w, y, y + h);
	limitFov();
}


void cGrCarCamMirror::setPos (int x, int y, int w, int h)
{
    mx = x;
    my = y;
    mw = w;
    mh = h;

    // round up texture size to next power of two
    tw = GfNearestPow2(w);
    th = GfNearestPow2(h);
    if (tw < w) {
		tw *= 2;
	}
    if (th < h) {
		th *= 2;
	}

    // Create texture object.
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glReadBuffer(GL_BACK);

    glCopyTexImage2D(GL_TEXTURE_2D,
		      0, // map level,
		      GL_RGB, // internal format,
		      0, 0, tw, th,
		      0 ); // border

    tsu = (float) mw / tw;
    teu = 0.0;
    tsv = 0.0;
    tev = (float) mh / th;
}


void cGrCarCamMirror::activateViewport (void)
{
    glViewport(vpx, vpy, vpw, vph);

    // Enable scissor test to conserve graphics memory bandwidth.
    glEnable(GL_SCISSOR_TEST);
    glScissor(vpx + (vpw - mw)/2, vpy + (vph - mh)/2, mw, mh);
}

void cGrCarCamMirror::store (void)
{
	glDisable(GL_SCISSOR_TEST);

	glBindTexture(GL_TEXTURE_2D, tex);
	glReadBuffer(GL_BACK);

	// NVidia recommends to NOT use glCopyTexImage2D for performance reasons.
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			    vpx + (vpw - mw)/2,
			    vpy + (vph - mh)/2, mw, mh);
}


void cGrCarCamMirror::display (void)
{
    viewCam->action ();

    glBindTexture (GL_TEXTURE_2D, tex);
    glBegin(GL_TRIANGLE_STRIP);
    {
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glTexCoord2f(tsu, tsv); glVertex2f(mx, my);
	glTexCoord2f(tsu, tev); glVertex2f(mx, my + mh);
	glTexCoord2f(teu, tsv); glVertex2f(mx + mw, my);
	glTexCoord2f(teu, tev); glVertex2f(mx + mw, my + mh);
    }
    glEnd();
}


class cGrCarCamInsideFixedCar : public cGrPerspCamera
{
 public:
    cGrCarCamInsideFixedCar(class cGrScreen *myscreen, int id, int drawCurr, int drawBG,
			    float myfovy, float myfovymin, float myfovymax,
			    float myfnear, float myffar = 1500.0,
			    float myfogstart = 1400.0, float myfogend = 1500.0)
	: cGrPerspCamera(myscreen, id, drawCurr, 1, drawBG, 1,
			 myfovy, myfovymin, myfovymax,
			 myfnear, myffar, myfogstart, myfogend) {
    }

    void update(tCarElt *car, tSituation *s) {
	sgVec3 P, p;
	
	p[0] = car->_bonnetPos_x;
	p[1] = car->_bonnetPos_y;
	p[2] = car->_bonnetPos_z;
	sgXformPnt3(p, car->_posMat);
	
	eye[0] = p[0];
	eye[1] = p[1];
	eye[2] = p[2];

	P[0] = car->_bonnetPos_x + 30.0;
	P[1] = car->_bonnetPos_y;
	P[2] = car->_bonnetPos_z;
	sgXformPnt3(P, car->_posMat);

	center[0] = P[0];
	center[1] = P[1];
	center[2] = P[2];

	up[0] = car->_posMat[2][0];
	up[1] = car->_posMat[2][1];
	up[2] = car->_posMat[2][2];

	speed[0] =car->pub.DynGCg.vel.x;
	speed[1] =car->pub.DynGCg.vel.y;
	speed[2] =car->pub.DynGCg.vel.z;
    }
};



void
grCamCreateSceneCameraList(class cGrScreen *myscreen, tGrCamHead *cams, tdble fovFactor)
{
    int			id;
    int			c;
    class cGrCamera	*cam;
    
    /* Scene Cameras */
    c = 0;

    /* F2 */
    GF_TAILQ_INIT(&cams[c]);
    id = 0;
    
    /* cam F2 = car inside with car (bonnet view) */
    cam = new cGrCarCamInside(myscreen,
			      id,
			      1,	/* drawCurr */
			      1,	/* drawBG  */
			      67.5,	/* fovy */
			      50.0,	/* fovymin */
			      95.0,	/* fovymax */
			      0.1,	/* near */
			      600.0 * fovFactor,	/* far */
			      300.0 * fovFactor,	/* fog */
			      600.0 * fovFactor	/* fog */
			      );
    cam->add(&cams[c]);
    id++;

    /* cam F2 = car inside car (no car - road view) */
    cam = new cGrCarCamInsideFixedCar(myscreen,
				      id,
				      0,	/* drawCurr */
				      1,	/* drawBG  */
				      67.5,	/* fovy */
				      50.0,	/* fovymin */
				      95.0,	/* fovymax */
				      0.3,	/* near */
				      600.0 * fovFactor,	/* far */
				      300.0 * fovFactor,	/* fog */
				      600.0 * fovFactor	/* fog */
				      );
    cam->add(&cams[c]);
}