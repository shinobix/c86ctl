/***
	c86ctl
	
	Copyright (c) 2009-2012, honet. All rights reserved.
	This software is licensed under the BSD license.

	honet.kk(at)gmail.com
 */
#include "stdafx.h"

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include <process.h>
#include <mmsystem.h>
#include <WinUser.h>
#include <Dbt.h>
#include <ShellAPI.h>

#include <string>
#include <list>
#include <vector>

extern "C" {
#include "hidsdi.h"
}

#include "resource.h"
#include "c86ctl.h"
#include "c86ctlmain.h"
#include "c86ctlmainwnd.h"

#include "config.h"
#include "vis/vis_c86main.h"
#include "ringbuff.h"
#include "interface/if.h"
#include "interface/if_gimic_hid.h"
#include "interface/if_gimic_midi.h"


#pragma comment(lib,"hidclass.lib")

#ifdef _DEBUG
#define new new(_NORMAL_BLOCK,__FILE__,__LINE__)
#endif

using namespace c86ctl;
using namespace c86ctl::vis;


// ------------------------------------------------------------------
#define WM_THREADEXIT       (WM_APP+10)
#define WM_TASKTRAY_EVENT   (WM_APP+11)

// ------------------------------------------------------------------
// グローバル変数
C86CtlMain theC86CtlMain;


HINSTANCE C86CtlMain::hInstance = 0;
ULONG_PTR C86CtlMain::gdiToken = 0;
Gdiplus::GdiplusStartupInput C86CtlMain::gdiInput;

// ------------------------------------------------------------------
// imprement

C86CtlMain* c86ctl::GetC86CtlMain(void)
{
	return &theC86CtlMain;
}

INT C86CtlMain::init(HINSTANCE h)
{
	hInstance = h;
	return 0;
}
INT C86CtlMain::deinit(void)
{
	hInstance = NULL;
	return 0;
}

HINSTANCE C86CtlMain::getInstanceHandle()
{
	return hInstance;
}

std::vector< std::shared_ptr<GimicIF> > &C86CtlMain::getGimics()
{
	return gGIMIC;
}

// ---------------------------------------------------------
// UIメッセージ処理スレッド
unsigned int WINAPI C86CtlMain::threadMain(LPVOID param)
{
	C86CtlMain *pThis = reinterpret_cast<C86CtlMain*>(param);
	MSG msg;
	BOOL b;

	ZeroMemory(&msg, sizeof(msg));
	::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

	try{
		if( Gdiplus::Ok != Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, NULL) )
			throw "failed to initialize GDI+";

		C86CtlMainWnd *pwnd = C86CtlMainWnd::getInstance();

		pwnd->createMainWnd(param);

		
		// メッセージループ
		while( (b = ::GetMessage(&msg, NULL, 0, 0)) ){
			if( b==-1 ) break;
			if( msg.message == WM_THREADEXIT )
				break;

			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		pwnd->destroyMainWnd(param);

		Gdiplus::GdiplusShutdown(gdiToken);
	}
	catch(...){
		::OutputDebugString(_T("ERROR\r\n"));
	}
	
	return (DWORD)msg.wParam;
}



// ---------------------------------------------------------
// 演奏処理スレッド
// mm-timerによる(だいたい)1ms単位処理
// note: timeSetEvent()だと転送処理がタイマ周期より遅いときに
//       再入されるのが怖かったので自前ループにした
unsigned int WINAPI C86CtlMain::threadSender(LPVOID param)
{
	try{
		const UINT period = 1;
		UINT now = ::timeGetTime();
		UINT next = now + period;
		UINT nextSec10 = now + 50;
		C86CtlMain *pThis = reinterpret_cast<C86CtlMain*>(param);

		while(1){
			if( pThis->terminateFlag )
				break;
		
			now = ::timeGetTime();
			if(now < next){
				if( pThis->terminateFlag ) break;
				Sleep(1);
				continue;
			}
			next += period;

			// update
			std::for_each( pThis->gGIMIC.begin(), pThis->gGIMIC.end(), [](std::shared_ptr<GimicIF> x){ x->tick(); } );
			
			if( nextSec10 < now ){
				nextSec10 += 50;
				std::for_each( pThis->gGIMIC.begin(), pThis->gGIMIC.end(), [](std::shared_ptr<GimicIF> x){ x->update(); } );
			}
		}
	}catch(...){
	}
	
	return 0;
}


// ---------------------------------------------------------
HRESULT C86CtlMain::QueryInterface( REFIID riid, LPVOID *ppvObj )
{
	if( ::IsEqualGUID( riid, IID_IRealChipBase ) ){
		*ppvObj = (LPVOID)this;
		return NOERROR;
	}
	*ppvObj = 0;
	return E_NOINTERFACE;
}

ULONG C86CtlMain::AddRef(VOID)
{
	return ++refCount;
}

ULONG C86CtlMain::Release(VOID)
{
	return --refCount;
}


int C86CtlMain::initialize(void)
{
	if( isInitialized )
		return C86CTL_ERR_UNKNOWN;
	
	// インスタンス生成
	int type = gConfig.getInt(INISC_MAIN, INIKEY_GIMICIFTYPE, 0);
	if( type==0 ){
		gGIMIC = GimicHID::CreateInstances();
	}else if( type==1 ){
		gGIMIC = GimicMIDI::CreateInstances();
	}
	
	// タイマ分解能設定
	TIMECAPS timeCaps;
	if( ::timeGetDevCaps(&timeCaps, sizeof(timeCaps)) == TIMERR_NOERROR ){
		::timeBeginPeriod(timeCaps.wPeriodMin);
		timerPeriod = timeCaps.wPeriodMin;
	}

	// 描画/UIスレッド開始
	hMainThread = (HANDLE)_beginthreadex( NULL, 0, &threadMain, this, 0, &mainThreadID );
	if( !hMainThread )
		return C86CTL_ERR_UNKNOWN;

	// 演奏スレッド開始
	hSenderThread = (HANDLE)_beginthreadex( NULL, 0, &threadSender, this, 0, &senderThreadID );
	if( !hSenderThread ){
		::PostThreadMessage( mainThreadID, WM_THREADEXIT, 0, 0 );
		::WaitForSingleObject( hMainThread, INFINITE );
		return C86CTL_ERR_UNKNOWN;
	}
	SetThreadPriority( hSenderThread, THREAD_PRIORITY_ABOVE_NORMAL );

	isInitialized = true;
	return C86CTL_ERR_NONE;
}


int C86CtlMain::deinitialize(void)
{
	if( !isInitialized )
		return C86CTL_ERR_UNKNOWN;

	reset();

	// 各種スレッド終了

//	HANDLE handles[3] = { gMainThread, gVisThread, gSenderThread };
//	::WaitForMultipleObjects(3, handles, TRUE, INFINITE );

	if( hMainThread ){
		::PostThreadMessage( mainThreadID, WM_THREADEXIT, 0, 0 );
		::WaitForSingleObject( hMainThread, INFINITE );
		hMainThread = NULL;
		mainThreadID = 0;
	}

	if( hSenderThread ){
		terminateFlag = true;
		::WaitForSingleObject( hSenderThread, INFINITE );
		hSenderThread = NULL;
		senderThreadID = 0;
	}
	terminateFlag = false;

	// インスタンス削除
	// note: このタイミングで終了処理が行われる。
	gGIMIC.clear();

	// タイマ分解能設定解除
	::timeEndPeriod(timerPeriod);
	isInitialized = false;
	
	return C86CTL_ERR_NONE;
}

int C86CtlMain::reset(void)
{
	std::for_each( gGIMIC.begin(), gGIMIC.end(), [](std::shared_ptr<GimicIF> x){ x->reset(); } );
	return 0;
}

int C86CtlMain::getNumberOfChip(void)
{
	return static_cast<int>(gGIMIC.size());
}

HRESULT C86CtlMain::getChipInterface( int id, REFIID riid, void** ppi )
{
	if( id < gGIMIC.size() ){
		return gGIMIC[id]->QueryInterface( riid, ppi );
	}
	return E_NOINTERFACE;
}

void C86CtlMain::out( UINT addr, UCHAR data )
{
	if( gGIMIC.size() ){
		gGIMIC.front()->out(addr,data);
	}
}

UCHAR C86CtlMain::in( UINT addr )
{
	if( gGIMIC.size() ){
		return gGIMIC.front()->in(addr);
	}else
		return 0;
}



