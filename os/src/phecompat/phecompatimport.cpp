// Copyright The Phorepoc Authors
// SPDX-License-Identifier: AGPL-3.0-only

// phecompatimport.cpp -- OURS: reads the profiles that phecompat's package installs (\private\e7016e02\profiles\*.ini, one app
// per file) and writes them into the profile repository (CenRep 0xE7016E00, key = the app's secure id, value = the profile bit
// field of core's ph_compat.h). The window server watches that repository, so a profile applies to the next app start.
//
// A profile file is "key=value" lines (';' or '#' starts a comment):
//   sid=0x2002a096           the app's secure id (required)
//   rotation=270             the rotation offset in degrees: 0, 90, 180 or 270 (default 0)
//   touchhold_ms=600         the minimum time a press lasts, in ms (default 0 = off; units of 10 ms)
// Anything else (name=, needs=, note=) is for people. Importing again overwrites what an earlier import wrote.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <centralrepository.h>

namespace
    {
    const TUint32 KRepositoryUid = 0xE7016E00;
    const TUint32 KHoldFlag = 0x4;

    // a number in decimal or 0x hex
    TInt ParseNumber(const TDesC8& aText, TUint32& aValue)
        {
        TLex8 lex(aText);
        lex.SkipSpace();
        if (aText.Length() > 2 && aText[0] == '0' && (aText[1] == 'x' || aText[1] == 'X'))
            {
            lex.Inc(2);
            return lex.Val(aValue, EHex);
            }
        return lex.Val(aValue, EDecimal);
        }

    void ImportFileL(RFs& aFs, CRepository& aRepo, const TDesC& aPath)
        {
        RFile file;
        User::LeaveIfError(file.Open(aFs, aPath, EFileRead | EFileShareReadersOnly));
        CleanupClosePushL(file);
        TInt size = 0;
        User::LeaveIfError(file.Size(size));
        if (size <= 0 || size > 4096)
            User::Leave(KErrCorrupt);
        HBufC8* text = HBufC8::NewLC(size);
        TPtr8 ptr = text->Des();
        User::LeaveIfError(file.Read(ptr));

        TUint32 sid = 0;
        TUint32 rotation = 0;
        TUint32 holdMs = 0;
        TPtrC8 rest(*text);
        while (rest.Length())
            {
            const TInt eol = rest.Locate('\n');
            TPtrC8 line = (eol == KErrNotFound) ? rest : rest.Left(eol);
            rest.Set((eol == KErrNotFound) ? TPtrC8() : rest.Mid(eol + 1));
            TPtrC8 l(line);
            while (l.Length() && (l[l.Length() - 1] == '\r' || l[l.Length() - 1] == ' '))
                l.Set(l.Left(l.Length() - 1));
            if (!l.Length() || l[0] == ';' || l[0] == '#')
                continue;
            const TInt eq = l.Locate('=');
            if (eq <= 0)
                continue;
            const TPtrC8 key = l.Left(eq);
            const TPtrC8 val = l.Mid(eq + 1);
            if (key == _L8("sid"))
                User::LeaveIfError(ParseNumber(val, sid));
            else if (key == _L8("rotation"))
                User::LeaveIfError(ParseNumber(val, rotation));
            else if (key == _L8("touchhold_ms"))
                User::LeaveIfError(ParseNumber(val, holdMs));
            }
        CleanupStack::PopAndDestroy(2, &file);

        if (sid == 0 || rotation % 90 != 0 || rotation > 270 || holdMs > 2550)
            User::Leave(KErrCorrupt);
        TUint32 value = rotation / 90;
        if (holdMs)
            value |= KHoldFlag | (((holdMs + 9) / 10) << 8);
        TInt err = aRepo.Create(sid, static_cast<TInt>(value));
        if (err == KErrAlreadyExists)
            err = aRepo.Set(sid, static_cast<TInt>(value));
        User::LeaveIfError(err);
        RDebug::Printf("phecompat: imported sid=%08x value=%x", sid, value);
        }

    void ImportAllL()
        {
        RFs fs;
        User::LeaveIfError(fs.Connect());
        CleanupClosePushL(fs);
        // the profiles sit in this process's private folder on the drive the package went to
        TFileName dir;
        User::LeaveIfError(fs.PrivatePath(dir));
        TParse exe;
        exe.Set(RProcess().FileName(), NULL, NULL);
        dir.Insert(0, exe.Drive());
        dir.Append(_L("profiles\\"));

        CRepository* repo = CRepository::NewLC(TUid::Uid(KRepositoryUid));
        TFileName spec(dir);
        spec.Append(_L("*.ini"));
        CDir* entries = NULL;
        const TInt listErr = fs.GetDir(spec, KEntryAttNormal, ESortByName, entries);
        if (listErr == KErrNone)
            {
            CleanupStack::PushL(entries);
            for (TInt i = 0; i < entries->Count(); ++i)
                {
                TFileName path(dir);
                path.Append((*entries)[i].iName);
                TRAPD(err, ImportFileL(fs, *repo, path));
                if (err != KErrNone)
                    RDebug::Printf("phecompat: %S: error %d", &path, err);
                }
            CleanupStack::PopAndDestroy(entries);
            }
        CleanupStack::PopAndDestroy(2, &fs);
        }
    }

TInt E32Main()
    {
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
        return KErrNoMemory;
    TRAPD(err, ImportAllL());
    delete cleanup;
    return err;
    }
