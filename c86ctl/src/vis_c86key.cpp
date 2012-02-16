/***
	c86ctl
	
	Copyright (c) 2009-2012, honet. All rights reserved.
	This software is licensed under the BSD license.

	honet.kk(at)gmail.com
 */
#include "stdafx.h"
#include <stdio.h>
#include "vis_c86key.h"
#include "vis_c86sub.h"

#ifdef _DEBUG
#define new new(_NORMAL_BLOCK,__FILE__,__LINE__)
#endif

bool CVisC86OPNAKey::create( HWND parent )
{
	int left = INT_MIN;
	int top = INT_MIN;
	
	if( !CVisWnd::create(
		left, top, windowWidth, windowHeight,
		WS_EX_TOOLWINDOW, (WS_POPUP | WS_CLIPCHILDREN), parent ) )
		return false;

	::ShowWindow( hWnd, SW_SHOWNOACTIVATE );

	for( int i=0; i<14; i++ ){
		if( 3<=i && i<=5 ) continue;
		muteSw[i] = CVisMuteSwPtr( new CVisMuteSw( this, 5+28, 7+i*35 ) );
		muteSw[i]->getter = [this, i]()-> int { return this->pOPNA->getPartMask(i);};
		muteSw[i]->setter = [this, i](int mask) {  this->pOPNA->setPartMask(i, mask ? true : false); };
		widgets.push_back(muteSw[i]);

		soloSw[i] = CVisSoloSwPtr( new CVisSoloSw( this, 5+28+16, 7+i*35 ) );
		soloSw[i]->getter = [this, i]()-> int { return this->pOPNA->getPartSolo(i);};
		soloSw[i]->setter = [this, i](int mask) {  this->pOPNA->setPartSolo(i, mask ? true : false); };
		widgets.push_back(soloSw[i]);
	}
	
	
	return true;
}

void CVisC86OPNAKey::close()
{
	CVisWnd::close();
}



void CVisC86OPNAKey::onPaintClient(void)
{
	visFillRect( clientCanvas, 0, 0, clientCanvas->getWidth(), clientCanvas->getHeight(), ARGB(255,0,0,0) );
	
	if( pOPNA ){
		int sx=5, sy=5, cx=6, cy=8;
		int tr=0;
		for( int i=0; i<3; i++ ){
			drawFMTrackView( clientCanvas, sx, sy, tr, i );
			sy+=35; tr++;
		}
		for( int i=0; i<3; i++ ){
			drawFM3EXTrackView( clientCanvas, sx, sy, tr, 2, i );
			sy+=35; tr++;
		}
		for( int i=3; i<6; i++ ){
			drawFMTrackView( clientCanvas, sx, sy, tr, i );
			sy+=35; tr++;
		}
		for( int i=0; i<3; i++ ){
			drawSSGTrackView( clientCanvas, sx, sy, tr, i );
			sy+=35; tr++;
		}
		drawADPCMTrackView( clientCanvas, sx, sy, tr );
		sy+=35; tr++;
		drawRhythmTrackView( clientCanvas, sx, sy, tr );
	}
}



void CVisC86OPNAKey::drawFMTrackView( IVisBitmap *canvas, int ltx, int lty,
								  int trNo, int fmNo )
{
	int sy = 0;
	char str[64], tmp[64];
	int cx=6, cy=8;
	CVisC86Skin *skin = &gVisSkin;

	sprintf( str, "%02d", trNo+1 );
	skin->drawNumStr1( canvas, ltx+5, lty+sy+2, str );
	sprintf( str, "FM-CH%d", fmNo+1 );
	skin->drawStr( canvas, 0, ltx+5+cx*10, lty+sy+5, str );

	if( !pOPNA->getMixedMask(trNo) ){
		COPNAFmCh *pFMCh = pOPNA->fm->ch[fmNo];
		strcpy(str, "SLOT:");
		for( int i=0; i<4; i++ ){
			int sw = pFMCh->slot[i]->isOn();
			sprintf( tmp, "%d", i );
			if( sw ) strncat( str, tmp, 64 );
			else  strncat( str, "_", 64 );
		}
		skin->drawStr( canvas, 0, ltx+5+cx*24, lty+sy+5, str );

		int fblock = pFMCh->getFBlock();
		int fnum = pFMCh->getFNum();
		sprintf( str, "BLK:%d  FNUM:%04d", fblock, fnum );
		skin->drawStr( canvas, 0, ltx+5+cx*35, lty+sy+5, str );
		skin->drawKeyboard( canvas, ltx, lty+sy+15 );

		if( pFMCh->isKeyOn() && pFMCh->getMixLevel() ){
			int oct, note;
			pFMCh->getNote( oct, note );
			skin->drawHilightKey( canvas, ltx, lty+sy+15, oct, note );
		}
		skin->drawHBar( canvas, 290, lty+sy+15, pFMCh->getKeyOnLevel(), 0 );
	}else{
		skin->drawDarkKeyboard( canvas, ltx, lty+sy+15 );
		skin->drawHBar( canvas, 290, lty+sy+15, 0, 0 );
	}
}

void CVisC86OPNAKey::drawFM3EXTrackView( IVisBitmap *canvas, int ltx, int lty,
									 int trNo, int fmNo, int exNo )
{
	int sy = 0;
	int cx=6, cy=8;
	char str[64];
	CVisC86Skin *skin = &gVisSkin;
	
	sprintf( str, "%02d", trNo+1 );
	skin->drawNumStr1( canvas, ltx+5, lty+sy+2, str );
	sprintf( str, "FM-CH%dEX%d", fmNo+1, exNo );
	skin->drawStr( canvas, 0, ltx+5+60, lty+sy+5, str );
	skin->drawKeyboard( canvas, ltx, lty+sy+15 );

	COPNAFmCh *pFMCh = pOPNA->fm->ch[fmNo];
	if( pFMCh->getExMode() ){
		int fblock = pFMCh->getFBlockEx(exNo);
		int fnum = pFMCh->getFNumEx(exNo);
		sprintf( str, "BLK:%d  FNUM:%04d", fblock, fnum );
		skin->drawStr( canvas, 0, ltx+5+cx*35, lty+sy+5, str );

		skin->drawKeyboard( canvas, ltx, lty+sy+15 );
		if( pFMCh->slot[exNo]->isOn() && pFMCh->getMixLevel() ){
			int oct, note;
			pFMCh->getNoteEx( exNo, oct, note );
			skin->drawHilightKey( canvas, ltx, lty+sy+15, oct, note );
		}
		skin->drawHBar( canvas, 290, lty+sy+15, 0, 0 );
	}else{
		skin->drawDarkKeyboard( canvas, ltx, lty+sy+15 );
		skin->drawHBar( canvas, 290, lty+sy+15, 0, 0 );
	}
}

void CVisC86OPNAKey::drawSSGTrackView( IVisBitmap *canvas, int ltx, int lty, int trNo, int ssgNo )
{
	int sy = 0;
	int cx=6, cy=8;
	char str[64];
	CVisC86Skin *skin = &gVisSkin;
	
	sprintf( str, "%02d", trNo+1 );
	skin->drawNumStr1( canvas, ltx+5, lty+sy+2, str );
	sprintf( str, "SSG-CH%d", ssgNo+1 );
	skin->drawStr( canvas, 0, ltx+5+cx*10, lty+sy+5, str );

	COPNASsgCh *pSsgCh = pOPNA->ssg->ch[ssgNo];
	if( !pOPNA->getMixedMask(trNo) ){
		int tune = pSsgCh->getTune();
		sprintf( str, "TUNE:%04d", tune );
		skin->drawStr( canvas, 0, ltx+5+cx*42, lty+sy+5, str );

		skin->drawKeyboard( canvas, ltx, lty+sy+15 );

		if( pSsgCh->isOn() && pSsgCh->getLevel()!=0 ){
			int oct, note;
			pSsgCh->getNote( oct, note );
			skin->drawHilightKey( canvas, ltx, lty+sy+15, oct, note );
		}
		skin->drawHBar( canvas, 290, lty+sy+15, pSsgCh->getKeyOnLevel(), 0 );
	}else{
		skin->drawDarkKeyboard( canvas, ltx, lty+sy+15 );
		skin->drawHBar( canvas, 290, lty+sy+15, 0, 0 );
	}
}

void CVisC86OPNAKey::drawADPCMTrackView( IVisBitmap *canvas, int ltx, int lty, int trNo )
{
	int sy = 0;
	char str[64];
	CVisC86Skin *skin = &gVisSkin;
	
	sprintf( str, "%02d", trNo+1 );
	skin->drawNumStr1( canvas, ltx+5, lty+sy+2, str );
	sprintf( str, "ADPCM" );
	skin->drawStr( canvas, 0, ltx+5+60, lty+sy+5, str );

	if( !pOPNA->getMixedMask(trNo) ){
		skin->drawKeyboard( canvas, ltx, lty+sy+15 );
	}else{
		skin->drawDarkKeyboard( canvas, ltx, lty+sy+15 );
	}
	skin->drawHBar( canvas, 290, lty+sy+15, 0, 0 );
}

void CVisC86OPNAKey::drawRhythmTrackView( IVisBitmap *canvas, int ltx, int lty, int trNo )
{
	int sy = 0;
	int cx=6, cy=8;
	char str[64];
	CVisC86Skin *skin = &gVisSkin;

	UINT col_mid = skin->getPal(CVisC86Skin::IDCOL_MID);
	
	sprintf( str, "%02d", trNo+1 );
	skin->drawNumStr1( canvas, ltx+5, lty+sy+2, str );
	sprintf( str, "RHYTHM" );
	skin->drawStr( canvas, 0, ltx+5+60, lty+sy+5, str );

	visDrawLine( canvas, ltx    , lty+sy+5+10, ltx+280, lty+sy+5+10, col_mid );
	visDrawLine( canvas, ltx+280, lty+sy+5+10, ltx+280, lty+sy+5+30, col_mid );
	visDrawLine( canvas, ltx+280, lty+sy+5+30, ltx    , lty+sy+5+30, col_mid );
	visDrawLine( canvas, ltx    , lty+sy+5+30, ltx    , lty+sy+5+10, col_mid );

	if( !pOPNA->getMixedMask(trNo) ){
		if( pOPNA->rhythm->rim->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*9, lty+sy+5+18, "RIM" );
		if( pOPNA->rhythm->tom->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*14, lty+sy+5+18, "TOM" );
		if( pOPNA->rhythm->hh->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*19, lty+sy+5+18, "HH" );
		if( pOPNA->rhythm->top->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*24, lty+sy+5+18, "TOP" );
		if( pOPNA->rhythm->sd->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*29, lty+sy+5+18, "SD" );
		if( pOPNA->rhythm->bd->isOn() )
			skin->drawStr( canvas, 0, ltx+5+cx*34, lty+sy+5+18, "BD");
	}
	skin->drawHBar( canvas, 290, lty+sy+16, 0, 0 );
}

// --------------------------------------------------------
CVisC86KeyPtr visC86KeyViewFactory(Chip *pchip, int id)
{
	if( typeid(*pchip) == typeid(COPNA) ){
		return CVisC86KeyPtr( new CVisC86OPNAKey(dynamic_cast<COPNA*>(pchip), id ) );
//	}else if( typeid(*pchip) == typeid(COPN3L) ){
//		return CVisC86RegPtr( new CVisC86OPN3LKey(dynamic_cast<COPN3L*>(pchip), id) );
//	}else if( typeid(*pchip) == typeid(COPM) ){
//		return CVisC86RegPtr( new CVisC86OPMKey(dynamic_cast<COPM*>(pchip), id) );
	}
	return 0;
}
