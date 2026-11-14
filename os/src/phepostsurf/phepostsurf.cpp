// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phepostsurf.cpp -- OURS: PostingSurfaceFactory.dll, the S60 Display Posting API (the 2010 display_posting_api headers) for games
// that post frames instead of drawing through a window of their own (the N-Gage runtime in posting mode). A posted frame is
// converted, rotated and scaled into the application's own window.
#include <e32base.h>
#include <e32debug.h>
#include <gdi.h>
#include <fbs.h>
#include <w32std.h>
#include <coemain.h>
#include <coecntrl.h>

const TUint KPostFormatLE = 0x01000000;
const TUint KPostRGB565 = 0x00000001;
const TUint KPostRGBx888 = 0x00000004;
const TUint KPostRGB888 = 0x00000002;

class CPostingSurface : public CBase
	{
public:
	enum TPostingFormat { EFormatInvalidValue = 0 };
	enum TRotationType { ENoRotation = 0, ERotate90ClockWise = 1, ERotate180 = 2, ERotate90AntiClockWise = 4 };
	enum TBufferType { EBufferTypeInvalid = 0, EStandardBuffer = 1, EChunkBuffer = 2, EExternalBuffer = 4 };
	class TPostingCapab
		{
	public:
		TUint iMaxSourceSize;
		TSize iMaxPixelSize;
		TUint iSupportedRotations;
		TBool iSupportsMirroring;
		TBool iSupportsScaling;
		TBool iSupportsBrightnessControl;
		TBool iSupportsContrastControl;
		TUint iSupportedPostingBuffering;
		TUint iSupportedBufferTypes;
		};
	class TPostingSourceParams
		{
	public:
		TUint iPostingBuffering;
		TBufferType iBufferType;
		TUint iPostingUsage;
		TSize iSourceImageSize;
		TUint iPostingFormat;
		TUint16 iPixelAspectRatioNum;
		TUint16 iPixelAspectRatioDenom;
		TBool iContentCopyRestricted;
		};
	class TPostingParams
		{
	public:
		TRect iDisplayedRect;
		TRect iScaleToRect;
		TRect iInputCrop;
		TBool iMirror;
		TRotationType iRotation;
		TInt16 iBrightness;
		TInt16 iContrast;
		TRgb iBackGround;
		};
	class TPostingBuff
		{
	public:
		virtual TBufferType GetType() { return iType; }
		virtual TAny* GetBuffer() { return iBuffer; }
		virtual TSize GetSize() { return iSize; }
		virtual TUint GetStride() { return iStride; }
		TPostingBuff() : iType(EStandardBuffer), iBuffer(NULL), iStride(0) {}
		TBufferType iType;
		TAny* iBuffer;
		TSize iSize;
		TUint iStride;
		};
public:
	virtual ~CPostingSurface() {}
	virtual void InitializeL(const TPostingSourceParams& aSource, const TPostingParams& aDest) = 0;
	virtual void GetCapabilities(TPostingCapab& aCaps) = 0;
	virtual TBool FormatSupported(TUint aFormat) = 0;
	virtual void Destroy(TRequestStatus& aComplete) = 0;
	virtual void Destroy() = 0;
	virtual TInt WaitForNextBuffer(TRequestStatus& aComplete) = 0;
	virtual TPostingBuff* NextBuffer() = 0;
	virtual TInt CancelBuffer() = 0;
	virtual TInt PostBuffer(TPostingBuff* aBuffer) = 0;
	virtual TInt SetPostingParameters(const TPostingParams& aParams) = 0;
	virtual TInt SetClipRegion(const TRegion& aClipRegion) = 0;
	};

class CPostingSurfaceFactory : public CBase
	{
public:
	virtual ~CPostingSurfaceFactory() {}
	virtual CPostingSurface* NewPostingSurfaceL(TInt aDisplay);
	};

struct TPhPeekHeap : public RHeap { TInt Handle() const { return iChunkHandle; } TUint Top() const { return (TUint)iTop; } };
static TInt gLog = 0;
class CPhPostCtl;
static CPhPostCtl* iCtl = NULL;   // one window per process, kept across surface re-creation
#define PLOG(...) do { if (gLog < 200) { ++gLog; RDebug::Printf(__VA_ARGS__); } } while (0)

class TPhDummyAbort : public MDirectScreenAccess
	{
public:
	void Restart(RDirectScreenAccess::TTerminationReasons) {}
	void AbortNow(RDirectScreenAccess::TTerminationReasons) {}
	};

class CPhPostCtl : public CCoeControl
	{
public:
	CFbsBitmap* iBmp;
	CCoeControl* iTarget;     // NGi's own window control (pointer events go there)
	TSize iGame;              // the game's window size (landscape)
	TBool iRot;               // display rotated 90 cw
	void ConstructL()
		{
		CreateWindowL();
		SetExtentToWholeScreen();
		ActivateL();
		}
	void Draw(const TRect& /*aRect*/) const
		{
		CWindowGc& gc = SystemGc();
		if (iBmp)
			gc.BitBlt(TPoint(0, 0), iBmp);
		}
	void HandlePointerEventL(const TPointerEvent& aEv)
		{
		if (!iTarget)
			return;
		TPointerEvent e = aEv;
		const TSize s = Size();
		if (iRot && s.iWidth > 0 && s.iHeight > 0)
			{
			e.iPosition.iX = aEv.iPosition.iY * iGame.iWidth / s.iHeight;
			e.iPosition.iY = (s.iWidth - 1 - aEv.iPosition.iX) * iGame.iHeight / s.iWidth;
			}
		e.iParentPosition = e.iPosition;
		static TInt n = 0;
		if (n < 12)
			{
			++n;
			RDebug::Printf("PH: posting pointer type %d (%d,%d) -> (%d,%d)", aEv.iType, aEv.iPosition.iX, aEv.iPosition.iY, e.iPosition.iX, e.iPosition.iY);
			}
		iTarget->HandlePointerEventL(e);
		}
	};

class CPhPostingSurface : public CPostingSurface
	{
public:
	~CPhPostingSurface()
		{
		User::Free(iColMap);
		User::Free(iRowMap);
		if (iTimerUp)
			{
			iTimer.Cancel();
			iTimer.Close();
			}
		if (iOwnUp)
			{
			iWin.Close();
			iGrp.Close();
			delete iGc;
			delete iDev;
			iWs.Close();
			}
		delete iBitmap;
		User::Free(iBuf[0].iBuffer);
		User::Free(iBuf[1].iBuffer);
		}
	void InitializeL(const TPostingSourceParams& aSource, const TPostingParams& aDest)
		{
		iSrc = aSource;
		iDst = aDest;
		PLOG("PH: posting InitializeL src %dx%d fmt %08x buffering %x type %d usage %x; dst disp %d,%d-%d,%d scale %d,%d-%d,%d crop %d,%d-%d,%d rot %d mirror %d",
			aSource.iSourceImageSize.iWidth, aSource.iSourceImageSize.iHeight, aSource.iPostingFormat, aSource.iPostingBuffering,
			aSource.iBufferType, aSource.iPostingUsage,
			aDest.iDisplayedRect.iTl.iX, aDest.iDisplayedRect.iTl.iY, aDest.iDisplayedRect.iBr.iX, aDest.iDisplayedRect.iBr.iY,
			aDest.iScaleToRect.iTl.iX, aDest.iScaleToRect.iTl.iY, aDest.iScaleToRect.iBr.iX, aDest.iScaleToRect.iBr.iY,
			aDest.iInputCrop.iTl.iX, aDest.iInputCrop.iTl.iY, aDest.iInputCrop.iBr.iX, aDest.iInputCrop.iBr.iY,
			aDest.iRotation, aDest.iMirror);
		iBpp = (aSource.iPostingFormat & KPostRGB565) ? 2 : 4;
		const TSize s = aSource.iSourceImageSize;
		for (TInt i = 0; i < 2; ++i)
			{
			User::Free(iBuf[i].iBuffer);
			iBuf[i].iBuffer = User::AllocZL(s.iWidth * s.iHeight * iBpp);
			iBuf[i].iSize = s;
			iBuf[i].iStride = s.iWidth * iBpp;
			}
		iNext = 0;
		}
	void GetCapabilities(TPostingCapab& aCaps)
		{
		aCaps.iMaxSourceSize = 640 * 640 * 4;
		aCaps.iMaxPixelSize = TSize(640, 640);
		aCaps.iSupportedRotations = ENoRotation | ERotate90ClockWise | ERotate180 | ERotate90AntiClockWise;
		aCaps.iSupportsMirroring = EFalse;
		aCaps.iSupportsScaling = ETrue;
		aCaps.iSupportsBrightnessControl = EFalse;
		aCaps.iSupportsContrastControl = EFalse;
		aCaps.iSupportedPostingBuffering = 0x1 | 0x4 | 0x10 | 0x200 | 0x1000;
		aCaps.iSupportedBufferTypes = EStandardBuffer;
		PLOG("PH: posting GetCapabilities");
		}
	TBool FormatSupported(TUint aFormat)
		{
		const TBool ok = aFormat == (KPostRGB565 | KPostFormatLE) || aFormat == (KPostRGBx888 | KPostFormatLE);
		PLOG("PH: posting FormatSupported %08x -> %d", aFormat, ok);
		return ok;
		}
	void Destroy(TRequestStatus& aComplete)
		{
		PLOG("PH: posting Destroy(async)");
		TRequestStatus* s = &aComplete;
		User::RequestComplete(s, KErrNone);
		}
	void Destroy()
		{
		PLOG("PH: posting Destroy");   // the caller deletes the object itself (it calls the destructor next)
		}
	TInt WaitForNextBuffer(TRequestStatus& aComplete)
		{
		if (!iTimerUp)
			iTimerUp = iTimer.CreateLocal() == KErrNone;
		if (iTimerUp)
			{
			iTimer.After(aComplete, 16000);
			return KErrNone;
			}
		TRequestStatus* s = &aComplete;
		aComplete = KRequestPending;
		User::RequestComplete(s, KErrNone);
		return KErrNone;
		}
	TPostingBuff* NextBuffer()
		{
		return &iBuf[iNext];
		}
	TInt CancelBuffer()
		{
		if (iTimerUp)
			iTimer.Cancel();
		return KErrNone;
		}
	TInt PostBuffer(TPostingBuff* aBuffer);
	TInt SetPostingParameters(const TPostingParams& aParams)
		{
		iDst = aParams;
		PLOG("PH: posting SetPostingParameters disp %d,%d-%d,%d rot %d", aParams.iDisplayedRect.iTl.iX, aParams.iDisplayedRect.iTl.iY,
			aParams.iDisplayedRect.iBr.iX, aParams.iDisplayedRect.iBr.iY, aParams.iRotation);
		return KErrNone;
		}
	TInt SetClipRegion(const TRegion& aClipRegion)
		{
		PLOG("PH: posting SetClipRegion n=%d", aClipRegion.Count());
		return KErrNone;
		}
private:
	TPostingSourceParams iSrc;
	TPostingParams iDst;
	TPostingBuff iBuf[2];
	TInt iNext;
	TInt iBpp;
	TInt* iColMap;   // v10 index tables (zeroed by CBase)
	TInt* iRowMap;
	TInt iMapW, iMapH, iMapCw, iMapCh, iMapRot;
	TBool iMapSwap, iMapCw0;
	CFbsBitmap* iBitmap;
	TInt iPosts;
	// own wserv connection when the poster thread has no CONE environment
	TBool iOwnUp;
	RWsSession iWs;
	CWsScreenDevice* iDev;
	CWindowGc* iGc;
	RWindowGroup iGrp;
	RWindow iWin;
	TBool OwnWindow();
	CDirectScreenAccess* iDsa;
	void FindTarget(CCoeEnv* aEnv);
	CDirectScreenAccess* FindDsa(CCoeEnv* aEnv, const TUint* aRoot);
	RTimer iTimer;
	TBool iTimerUp;
	};

static inline TUint32 Px(const TUint8* aRow, TInt aX, TInt aBpp)
	{
	if (aBpp == 2)
		{
		const TUint16 p = ((const TUint16*)aRow)[aX];
		const TUint32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
		return 0xff000000u | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
		}
	return ((const TUint32*)aRow)[aX] | 0xff000000u;
	}

TInt CPhPostingSurface::PostBuffer(TPostingBuff* aBuffer)
	{
	++iPosts;
	if (iPosts <= 3 || (iPosts % 3000) == 0)
		PLOG("PH: posting PostBuffer #%d buf %08x", iPosts, (TUint)aBuffer);
	CCoeEnv* env = CCoeEnv::Static();
	CCoeControl* ctl = env ? (CCoeControl*)env->RootWin().Child() : NULL;
	RWindow* win = ctl ? (RWindow*)ctl->DrawableWindow() : NULL;
	if (iPosts <= 2)
		{
		PLOG("PH: posting thread %u env %08x ctl %08x win %08x", (TUint)RThread().Id(), (TUint)env, (TUint)ctl, (TUint)win);
		if (ctl)
			{
			const TUint* w = (const TUint*)ctl;
			PLOG("PH: posting child words %08x %08x %08x %08x %08x %08x %08x %08x", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
			}
		}
	if (!aBuffer || !aBuffer->iBuffer || !env || iPosts < 30)
		return KErrNone;
	if (iPosts == 40 || iPosts == 400)   // v9a: give NGi's window control(s) (vtable in RAM code, 0x0 size) the screen extent
		{
		const TSize scr = env->ScreenDevice()->SizeInPixels();
		RHeap& heap = User::Heap();
		RChunk hc;
		hc.SetHandle(static_cast<TPhPeekHeap&>(heap).Handle());
		const TUint lo = (TUint)heap.Base(), ce = (TUint)hc.Base() + hc.Size();
		const TUint hend = lo + (TUint)heap.Size();
		const TUint top = static_cast<TPhPeekHeap&>(heap).Top();
		const TUint hi = (top > lo && top < hend) ? top : hend;   // v9c: RHeap::iTop = the committed top (the chunk handle gave a wrong end)
		PLOG("PH: v9a scan %08x-%08x (top %08x end %08x chunk end %08x)", lo, hi, top, hend, ce);
		TInt fixed = 0;
		for (TUint a = (lo + 3) & ~3u; a + 4 * 12 < hi && fixed < 4; a += 4)
			{
			const TUint* o = (const TUint*)a;
			if (o[1] != (TUint)env || o[0] < 0x70000000u || o[0] >= 0x80000000u || !o[7])
				continue;
			CCoeControl* c = (CCoeControl*)o;
			if ((TUint)c->DrawableWindow() != o[7])
				continue;
			const TSize sz = c->Size();
			PLOG("PH: v9a ctl %08x vt %08x pos %d,%d size %dx%d -> %dx%d", a, o[0], c->Position().iX, c->Position().iY, sz.iWidth, sz.iHeight, scr.iWidth, scr.iHeight);
			if (sz.iWidth == 0 || sz.iHeight == 0)
				{
				c->SetExtent(TPoint(0, 0), scr);
				++fixed;
				}
			}
		PLOG("PH: v9a post %d fixed %d", iPosts, fixed);
		}
	if (!iDsa)
		{
		if (iPosts > 60)
			return KErrNone;   // gave up
		iDsa = FindDsa(env, (const TUint*)ctl);
		if (!iDsa)
			return KErrNone;
		}
	if ((iPosts % 120) == 0)
		User::ResetInactivityTime();   // v8z: no screensaver/keylock while the game posts
	if (!iDsa->IsActive())             // v8z: the DSA is aborted (region change) -- never draw/Update then
		{
		if ((iPosts % 60) == 0) PLOG("PH: posting DSA not active, skip #%d", iPosts);
		return KErrNone;
		}
	CFbsBitGc* dgc = iDsa->Gc();
	CFbsScreenDevice* sdev = iDsa->ScreenDevice();
	if (!dgc || !sdev)
		return KErrNone;
	const TSize ws = sdev->SizeInPixels();
	if (!iBitmap || iBitmap->SizeInPixels() != ws)
		{
		delete iBitmap;
		iBitmap = new CFbsBitmap;
		if (!iBitmap || iBitmap->Create(ws, EColor16MU) != KErrNone)
			{
			delete iBitmap;
			iBitmap = NULL;
			return KErrNone;
			}
		PLOG("PH: posting window %dx%d", ws.iWidth, ws.iHeight);
		}
	// destination rectangle (window coordinates) and the source crop
	TRect dr = iDst.iDisplayedRect;
	if (dr.IsEmpty())
		dr = TRect(ws);
	dr.Intersection(TRect(ws));
	TRect cr = iDst.iInputCrop;
	if (cr.IsEmpty())
		cr = TRect(aBuffer->iSize);
	TBool swap = iDst.iRotation == ERotate90ClockWise || iDst.iRotation == ERotate90AntiClockWise;
	TBool forceCw = EFalse;
	if (!swap && ws.iWidth < ws.iHeight && cr.Width() > cr.Height())
		{
		swap = forceCw = ETrue;
		dr = TRect(ws);
		}

	iBitmap->LockHeap();
	TUint32* dst = iBitmap->DataAddress();
	const TInt dstride = CFbsBitmap::ScanLineLength(ws.iWidth, EColor16MU) / 4;
	const TUint8* src = (const TUint8*)aBuffer->iBuffer;
	const TInt sstride = aBuffer->iStride;
	const TInt dw = dr.Width(), dh = dr.Height();
	const TInt cw = cr.Width(), ch = cr.Height();
	// v10: the per-pixel divides + Px call cost ~60 ms per frame on 360x640; map through index tables built once
	// per post: for each destination column / row, the source column / row (or, swapped, the other way round)
	if (iMapW != dw || iMapH != dh || iMapCw != cw || iMapCh != ch || iMapSwap != swap || iMapRot != (TInt)iDst.iRotation || iMapCw0 != forceCw)
		{
		User::Free(iColMap); User::Free(iRowMap);
		iColMap = (TInt*)User::Alloc(dw * sizeof(TInt));
		iRowMap = (TInt*)User::Alloc(dh * sizeof(TInt));
		if (!iColMap || !iRowMap) { User::Free(iColMap); User::Free(iRowMap); iColMap = iRowMap = NULL; iBitmap->UnlockHeap(); return KErrNone; }
		const TBool cwRot = forceCw || iDst.iRotation == ERotate90ClockWise;
		for (TInt x = 0; x < dw; ++x)
			{
			if (!swap)
				{ TInt sx = x * cw / dw; if (iDst.iRotation == ERotate180) sx = cw - 1 - sx; iColMap[x] = sx; }
			else
				{ TInt v = x * ch / dw; iColMap[x] = cwRot ? ch - 1 - v : v; }   // = source row
			}
		for (TInt y = 0; y < dh; ++y)
			{
			if (!swap)
				{ TInt sy = y * ch / dh; if (iDst.iRotation == ERotate180) sy = ch - 1 - sy; iRowMap[y] = sy; }
			else
				{ TInt u = y * cw / dh; iRowMap[y] = cwRot ? u : cw - 1 - u; }   // = source column
			}
		iMapW = dw; iMapH = dh; iMapCw = cw; iMapCh = ch; iMapSwap = swap; iMapRot = iDst.iRotation; iMapCw0 = forceCw;
		}
	const TUint8* sbase = src + cr.iTl.iY * sstride;
	const TInt sx0 = cr.iTl.iX;
	for (TInt y = 0; y < dh; ++y)
		{
		TUint32* drow = dst + (dr.iTl.iY + y) * dstride + dr.iTl.iX;
		const TInt ry = iRowMap[y];
		if (!swap)
			{
			const TUint8* srow = sbase + ry * sstride;
			if (iBpp == 2)
				{ const TUint16* s16 = (const TUint16*)srow + sx0; for (TInt x = 0; x < dw; ++x) drow[x] = Px((const TUint8*)s16, iColMap[x], 2); }
			else
				{ const TUint32* s32 = (const TUint32*)srow + sx0; for (TInt x = 0; x < dw; ++x) drow[x] = s32[iColMap[x]] | 0xff000000u; }
			}
		else
			{
			const TInt col = sx0 + ry;
			if (iBpp == 2)
				{ for (TInt x = 0; x < dw; ++x) drow[x] = Px(sbase + iColMap[x] * sstride, col, 2); }
			else
				{ for (TInt x = 0; x < dw; ++x) drow[x] = ((const TUint32*)(sbase + iColMap[x] * sstride))[col] | 0xff000000u; }
			}
		}
	iBitmap->UnlockHeap();
	if (iPosts <= 34 || (iPosts % 3000) == 0)
		PLOG("PH: posting DSA blit #%d %dx%d", iPosts, ws.iWidth, ws.iHeight);
	dgc->BitBlt(TPoint(0, 0), iBitmap);
	sdev->Update();
	iNext ^= 1;
	return KErrNone;
	}

CDirectScreenAccess* CPhPostingSurface::FindDsa(CCoeEnv* aEnv, const TUint* aRoot)
	{
	// the vtable of CDirectScreenAccess, from a throw-away instance on an inactive window
	RWindow w(aEnv->WsSession());
	if (w.Construct(aEnv->RootWin(), 0x7ead0001) != KErrNone)
		return NULL;
	TPhDummyAbort obs;
	CDirectScreenAccess* d = NULL;
	TRAPD(r, d = CDirectScreenAccess::NewL(aEnv->WsSession(), *aEnv->ScreenDevice(), w, obs));
	TUint vt = d ? *(const TUint*)d : 0;
	delete d;
	w.Close();
	RHeap& heap = User::Heap();
	TUint lo = (TUint)heap.Base(), hi = lo + heap.Size();
	{
	RChunk ch;
	ch.SetHandle(static_cast<TPhPeekHeap&>(heap).Handle());
	const TUint cb = (TUint)ch.Base(), ce = cb + ch.Size();   // the committed part (bottom-up heap chunk)
	(void)cb; (void)ce;
	}
	PLOG("PH: posting dsa vtable %08x (r %d) heap %08x-%08x root %08x", vt, r, lo, hi, (TUint)aRoot);
	if (!vt)
		return NULL;
	// linear scan from the heap bottom for the first object with that vtable (the DSA is made early)
	TInt n = 0;
	for (TUint a = (lo + 3) & ~3u; a + 8 < hi; a += 4)
		{
		if (*(const TUint*)a == vt)
			{
			PLOG("PH: posting dsa found %08x", a);
			return (CDirectScreenAccess*)a;
			}
		++n;
		}
	PLOG("PH: posting dsa not found (%d objects)", n);
	return NULL;
	}

void CPhPostingSurface::FindTarget(CCoeEnv* aEnv)
	{
	// word index of iWin inside a CCoeControl, from our own control
	const TUint* me = (const TUint*)iCtl;
	const TUint myWin = (TUint)iCtl->DrawableWindow();
	TInt winIdx = -1, envIdx = -1;
	for (TInt i = 0; i < 24; ++i)
		{
		if (me[i] == myWin && winIdx < 0) winIdx = i;
		if (me[i] == (TUint)aEnv && envIdx < 0) envIdx = i;
		}
	RHeap& heap = User::Heap();
	TUint lo = (TUint)heap.Base(), hi = lo + heap.Size();
	{
	RChunk ch;
	ch.SetHandle(static_cast<TPhPeekHeap&>(heap).Handle());
	const TUint cb = (TUint)ch.Base(), ce = cb + ch.Size();   // the committed part (bottom-up heap chunk)
	(void)cb; (void)ce;
	}
	PLOG("PH: posting target scan winIdx %d envIdx %d heap %08x-%08x", winIdx, envIdx, lo, hi);
	if (winIdx < 0 || envIdx < 0)
		return;
	for (TUint a = (lo + 3) & ~3u; a + 4 * 24 < hi; a += 4)
		{
		const TUint* o = (const TUint*)a;
		if (o[envIdx] != (TUint)aEnv || o == me)
			continue;
		if (o[0] < 0x70000000u || o[0] >= 0x80000000u)   // vtable in RAM-loaded code (the NGi runtime)
			continue;
		if (!o[winIdx])
			continue;
		CCoeControl* c = (CCoeControl*)o;
		const TSize sz = c->Size();
		PLOG("PH: posting target candidate %08x vt %08x size %dx%d", a, o[0], sz.iWidth, sz.iHeight);
		iCtl->iTarget = c;
		iCtl->iGame = (sz.iWidth > 0 && sz.iHeight > 0) ? sz : iDst.iDisplayedRect.Size();
		return;
		}
	}

TBool CPhPostingSurface::OwnWindow()
	{
	if (iOwnUp)
		return ETrue;
	if (!RFbsSession::GetSession())
		PLOG("PH: posting fbs connect %d", RFbsSession::Connect());
	TInt r = iWs.Connect();
	if (r != KErrNone)
		{
		PLOG("PH: posting own ws connect %d", r);
		return EFalse;
		}
	iDev = new CWsScreenDevice(iWs);
	if (!iDev || iDev->Construct() != KErrNone || iDev->CreateContext(iGc) != KErrNone)
		{
		PLOG("PH: posting own screen device failed");
		return EFalse;
		}
	iGrp = RWindowGroup(iWs);
	iGrp.Construct((TUint32)this + 1, EFalse);
	iGrp.SetOrdinalPosition(0, 0);
	iWin = RWindow(iWs);
	iWin.Construct(iGrp, (TUint32)this + 2);
	iWin.SetExtent(TPoint(0, 0), iDev->SizeInPixels());
	iWin.Activate();
	iOwnUp = ETrue;
	PLOG("PH: posting own window %dx%d", iDev->SizeInPixels().iWidth, iDev->SizeInPixels().iHeight);
	return ETrue;
	}

CPostingSurface* CPostingSurfaceFactory::NewPostingSurfaceL(TInt aDisplay)
	{
	PLOG("PH: posting NewPostingSurfaceL display %d", aDisplay);
	return new (ELeave) CPhPostingSurface;
	}

extern "C" EXPORT_C CPostingSurfaceFactory* PhNewPostingSurfaceFactory()
	{
	PLOG("PH: posting factory created");
	return new CPostingSurfaceFactory;
	}
