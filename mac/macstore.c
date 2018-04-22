/* $Id: macstore.c,v 1.19 2003/04/01 18:10:25 simon Exp $ */

/*
 * macstore.c: Macintosh-specific impementation of the interface
 * defined in storage.h
 */

#include <MacTypes.h>
#include <Folders.h>
#include <Memory.h>
#include <Resources.h>
#include <TextUtils.h>

#include <stdio.h>
#include <string.h>

#include "putty.h"
#include "storage.h"
#include "mac.h"
#include "macresid.h"

OSErr FSpGetDirID(FSSpec *f, long *idp, Boolean makeit);

/*
 * We store each session as a file in the "PuTTY" sub-directory of the
 * preferences folder.  Each (key,value) pair is stored as a resource.
 */

OSErr get_putty_dir(Boolean makeit, short *pVRefNum, long *pDirID)
{
  OSErr error = noErr;
  short prefVRefNum;
  FSSpec puttydir;
  long prefDirID, puttyDirID;

  error = FindFolder(
      kOnSystemDisk, kPreferencesFolderType, makeit, &prefVRefNum, &prefDirID);
  if (error != noErr)
    goto out;

  error = FSMakeFSSpec(prefVRefNum, prefDirID, "\pPuTTY", &puttydir);
  if (error != noErr && error != fnfErr)
    goto out;
  error = FSpGetDirID(&puttydir, &puttyDirID, makeit);
  if (error != noErr)
    goto out;

  *pVRefNum = prefVRefNum;
  *pDirID = puttyDirID;

out:
  return error;
}

OSErr get_session_dir(Boolean makeit, short *pVRefNum, long *pDirID)
{
  OSErr error = noErr;
  short puttyVRefNum;
  FSSpec sessdir;
  long puttyDirID, sessDirID;

  error = get_putty_dir(makeit, &puttyVRefNum, &puttyDirID);
  if (error != noErr)
    goto out;
  error = FSMakeFSSpec(puttyVRefNum, puttyDirID, "\pSaved Sessions", &sessdir);
  if (error != noErr && error != fnfErr)
    goto out;
  error = FSpGetDirID(&sessdir, &sessDirID, makeit);
  if (error != noErr)
    goto out;

  *pVRefNum = puttyVRefNum;
  *pDirID = sessDirID;

out:
  return error;
}

OSErr FSpGetDirID(FSSpec *f, long *idp, Boolean makeit)
{
  CInfoPBRec pb;
  OSErr error = noErr;

  pb.dirInfo.ioNamePtr = f->name;
  pb.dirInfo.ioVRefNum = f->vRefNum;
  pb.dirInfo.ioDrDirID = f->parID;
  pb.dirInfo.ioFDirIndex = 0;
  error = PBGetCatInfoSync(&pb);
  if (error == fnfErr && makeit)
    return FSpDirCreate(f, smSystemScript, idp);
  if (error != noErr)
    goto out;
  if ((pb.dirInfo.ioFlAttrib & ioDirMask) == 0) {
    error = dirNFErr;
    goto out;
  }
  *idp = pb.dirInfo.ioDrDirID;

out:
  return error;
}

/* Copy a resource into the current resource file */
static OSErr copy_resource(ResType restype, short resid)
{
  Handle h;
  Str255 resname;

  h = GetResource(restype, resid);
  if (h != NULL) {
    GetResInfo(h, &resid, &restype, resname);
    DetachResource(h);
    AddResource(h, restype, resid, resname);
    if (ResError() == noErr)
      WriteResource(h);
  }
  return ResError();
}

struct write_settings {
  int fd;
  FSSpec tmpfile;
  FSSpec dstfile;
};

void *open_settings_w(char const *sessionname, char **errmsg)
{
  short sessVRefNum;
  long sessDirID;
  OSErr error;
  Str255 psessionname;
  FSSpec dstfile;

  *errmsg = NULL;

  error = get_session_dir(kCreateFolder, &sessVRefNum, &sessDirID);
  if (error != noErr)
    return NULL;

  if (!sessionname || !*sessionname)
    sessionname = "Default Settings";
  c2pstrcpy(psessionname, sessionname);
  error = FSMakeFSSpec(sessVRefNum, sessDirID, psessionname, &dstfile);
  if (error == fnfErr) {
    FSpCreateResFile(&dstfile, PUTTY_CREATOR, SESS_TYPE, smSystemScript);
    if ((error = ResError()) != noErr)
      return NULL;
  } else if (error != noErr)
    return NULL;

  return open_settings_w_fsp(&dstfile);
}

/*
 * NB: Destination file must exist.
 */
void *open_settings_w_fsp(FSSpec *dstfile)
{
  short tmpVRefNum;
  long tmpDirID;
  struct write_settings *ws;
  OSErr error;
  Str255 tmpname;

  ws = snew(struct write_settings);
  ws->dstfile = *dstfile;

  /* Create a temporary file to save to first. */
  error = FindFolder(ws->dstfile.vRefNum,
                     kTemporaryFolderType,
                     kCreateFolder,
                     &tmpVRefNum,
                     &tmpDirID);
  if (error != noErr)
    goto out;
  c2pstrcpy(tmpname, tmpnam(NULL));
  error = FSMakeFSSpec(tmpVRefNum, tmpDirID, tmpname, &ws->tmpfile);
  if (error != noErr && error != fnfErr)
    goto out;
  if (error == noErr) {
    error = FSpDelete(&ws->tmpfile);
    if (error != noErr)
      goto out;
  }
  FSpCreateResFile(&ws->tmpfile, PUTTY_CREATOR, SESS_TYPE, smSystemScript);
  if ((error = ResError()) != noErr)
    goto out;

  ws->fd = FSpOpenResFile(&ws->tmpfile, fsWrPerm);
  if (ws->fd == -1) {
    error = ResError();
    goto out;
  }

  /* Set up standard resources.  Doesn't matter if these fail. */
  copy_resource('STR ', -16396);
  copy_resource('TMPL', TMPL_Int);

  return ws;

out:
  safefree(ws);
  fatalbox("Failed to open session for write (%d)", error);
}

void write_setting_s(void *handle, char const *key, char const *value)
{
  int fd = *(int *)handle;
  Handle h;
  int id;
  OSErr error;
  Str255 pkey;

  UseResFile(fd);
  if (ResError() != noErr)
    fatalbox("Failed to open saved session (%d)", ResError());

  error = PtrToHand(value, &h, strlen(value));
  if (error != noErr)
    fatalbox("Failed to allocate memory");
  /* Put the data in a resource. */
  id = Unique1ID(FOUR_CHAR_CODE('TEXT'));
  if (ResError() != noErr)
    fatalbox("Failed to get ID for resource %s (%d)", key, ResError());
  c2pstrcpy(pkey, key);
  AddResource(h, FOUR_CHAR_CODE('TEXT'), id, pkey);
  if (ResError() != noErr)
    fatalbox("Failed to add resource %s (%d)", key, ResError());
}

void write_setting_i(void *handle, char const *key, int value)
{
  int fd = *(int *)handle;
  Handle h;
  int id;
  OSErr error;
  Str255 pkey;

  UseResFile(fd);
  if (ResError() != noErr)
    fatalbox("Failed to open saved session (%d)", ResError());

  /* XXX assume all systems have the same "int" format */
  error = PtrToHand(&value, &h, sizeof(int));
  if (error != noErr)
    fatalbox("Failed to allocate memory (%d)", error);

  /* Put the data in a resource. */
  id = Unique1ID(FOUR_CHAR_CODE('Int '));
  if (ResError() != noErr)
    fatalbox("Failed to get ID for resource %s (%d)", key, ResError());
  c2pstrcpy(pkey, key);
  AddResource(h, FOUR_CHAR_CODE('Int '), id, pkey);
  if (ResError() != noErr)
    fatalbox("Failed to add resource %s (%d)", key, ResError());
}

void close_settings_w(void *handle)
{
  struct write_settings *ws = handle;
  OSErr error;

  CloseResFile(ws->fd);
  if ((error = ResError()) != noErr)
    goto out;
  error = FSpExchangeFiles(&ws->tmpfile, &ws->dstfile);
  if (error != noErr)
    goto out;
  error = FSpDelete(&ws->tmpfile);
  if (error != noErr)
    goto out;
  return;

out:
  fatalbox("Close of saved session failed (%d)", error);
  safefree(handle);
}

void *open_settings_r(char const *sessionname)
{
  short sessVRefNum;
  long sessDirID;
  FSSpec sessfile;
  OSErr error;
  Str255 psessionname;

  error = get_session_dir(kDontCreateFolder, &sessVRefNum, &sessDirID);

  if (!sessionname || !*sessionname)
    sessionname = "Default Settings";
  c2pstrcpy(psessionname, sessionname);
  error = FSMakeFSSpec(sessVRefNum, sessDirID, psessionname, &sessfile);
  if (error != noErr)
    goto out;
  return open_settings_r_fsp(&sessfile);

out:
  return NULL;
}

void *open_settings_r_fsp(FSSpec *sessfile)
{
  OSErr error;
  int fd;
  int *handle;

  fd = FSpOpenResFile(sessfile, fsRdPerm);
  if (fd == 0) {
    error = ResError();
    goto out;
  }

  handle = snew(int);
  *handle = fd;
  return handle;

out:
  return NULL;
}

char *read_setting_s(void *handle, char const *key, char *buffer, int buflen)
{
  int fd;
  Handle h;
  size_t len;
  Str255 pkey;

  if (handle == NULL)
    goto out;
  fd = *(int *)handle;
  UseResFile(fd);
  if (ResError() != noErr)
    goto out;
  c2pstrcpy(pkey, key);
  h = Get1NamedResource(FOUR_CHAR_CODE('TEXT'), pkey);
  if (h == NULL)
    goto out;

  len = GetHandleSize(h);
  if (len + 1 > buflen)
    goto out;
  memcpy(buffer, *h, len);
  buffer[len] = '\0';

  ReleaseResource(h);
  if (ResError() != noErr)
    goto out;
  return buffer;

out:
  return NULL;
}

int read_setting_i(void *handle, char const *key, int defvalue)
{
  int fd;
  Handle h;
  int value;
  Str255 pkey;

  if (handle == NULL)
    goto out;
  fd = *(int *)handle;
  UseResFile(fd);
  if (ResError() != noErr)
    goto out;
  c2pstrcpy(pkey, key);
  h = Get1NamedResource(FOUR_CHAR_CODE('Int '), pkey);
  if (h == NULL)
    goto out;
  value = *(int *)*h;
  ReleaseResource(h);
  if (ResError() != noErr)
    goto out;
  return value;

out:
  return defvalue;
}

int read_setting_fontspec(void *handle, const char *name, FontSpec *result)
{
  char *settingname;
  FontSpec ret;
  char tmp[256];

  if (!read_setting_s(handle, name, tmp, sizeof(tmp)))
    return 0;
  c2pstrcpy(ret.name, tmp);
  settingname = dupcat(name, "Face", NULL);
  ret.face = read_setting_i(handle, settingname, 0);
  sfree(settingname);
  settingname = dupcat(name, "Height", NULL);
  ret.size = read_setting_i(handle, settingname, 0);
  sfree(settingname);
  if (ret.size == 0)
    return 0;
  *result = ret;
  return 1;
}

void write_setting_fontspec(void *handle, const char *name, FontSpec font)
{
  char *settingname;
  char tmp[256];

  p2cstrcpy(tmp, font.name);
  write_setting_s(handle, name, tmp);
  settingname = dupcat(name, "Face", NULL);
  write_setting_i(handle, settingname, font.face);
  sfree(settingname);
  settingname = dupcat(name, "Size", NULL);
  write_setting_i(handle, settingname, font.size);
  sfree(settingname);
}

int read_setting_filename(void *handle, const char *key, Filename *result)
{
  int fd;
  AliasHandle h;
  Boolean changed;
  OSErr err;
  Str255 pkey;

  if (handle == NULL)
    goto out;
  fd = *(int *)handle;
  UseResFile(fd);
  if (ResError() != noErr)
    goto out;
  c2pstrcpy(pkey, key);
  h = (AliasHandle)Get1NamedResource(rAliasType, pkey);
  if (h == NULL)
    goto out;
  if ((*h)->userType == 'pTTY' && (*h)->aliasSize == sizeof(**h))
    memset(result, 0, sizeof(*result));
  else {
    err = ResolveAlias(NULL, h, &result->fss, &changed);
    if (err != noErr && err != fnfErr)
      goto out;
    if ((*h)->userType == 'pTTY') {
      long dirid;
      StrFileName fname;

      /* Tail of record is pascal string contaning leafname */
      if (FSpGetDirID(&result->fss, &dirid, FALSE) != noErr)
        goto out;
      memcpy(fname,
             (char *)*h + (*h)->aliasSize,
             GetHandleSize((Handle)h) - (*h)->aliasSize);
      err = FSMakeFSSpec(result->fss.vRefNum, dirid, fname, &result->fss);
      if (err != noErr && err != fnfErr)
        goto out;
    }
  }
  ReleaseResource((Handle)h);
  if (ResError() != noErr)
    goto out;
  return 1;

out:
  return 0;
}

void write_setting_filename(void *handle, const char *key, Filename fn)
{
  int fd = *(int *)handle;
  AliasHandle h;
  int id;
  OSErr error;
  Str255 pkey;

  UseResFile(fd);
  if (ResError() != noErr)
    fatalbox("Failed to open saved session (%d)", ResError());

  if (filename_is_null(fn)) {
    /* Generate a special "null" alias */
    h = (AliasHandle)NewHandle(sizeof(**h));
    if (h == NULL)
      fatalbox("Failed to create fake alias");
    (*h)->userType = 'pTTY';
    (*h)->aliasSize = sizeof(**h);
  } else {
    error = NewAlias(NULL, &fn.fss, &h);
    if (error == fnfErr) {
      /*
       * NewAlias can't create an alias for a nonexistent file.
       * Create an alias for the directory, and record the
       * filename as well.
       */
      FSSpec tmpfss;

      FSMakeFSSpec(fn.fss.vRefNum, fn.fss.parID, NULL, &tmpfss);
      error = NewAlias(NULL, &tmpfss, &h);
      if (error != noErr)
        fatalbox("Failed to create alias");
      (*h)->userType = 'pTTY';
      SetHandleSize((Handle)h, (*h)->aliasSize + fn.fss.name[0] + 1);
      if (MemError() != noErr)
        fatalbox("Failed to create alias");
      memcpy((char *)*h + (*h)->aliasSize, fn.fss.name, fn.fss.name[0] + 1);
    }
    if (error != noErr)
      fatalbox("Failed to create alias");
  }
  /* Put the data in a resource. */
  id = Unique1ID(rAliasType);
  if (ResError() != noErr)
    fatalbox("Failed to get ID for resource %s (%d)", key, ResError());
  c2pstrcpy(pkey, key);
  AddResource((Handle)h, rAliasType, id, pkey);
  if (ResError() != noErr)
    fatalbox("Failed to add resource %s (%d)", key, ResError());
}

void close_settings_r(void *handle)
{
  int fd;

  if (handle == NULL)
    return;
  fd = *(int *)handle;
  CloseResFile(fd);
  if (ResError() != noErr)
    fatalbox("Close of saved session failed (%d)", ResError());
  sfree(handle);
}

void del_settings(char const *sessionname)
{
  OSErr error;
  FSSpec sessfile;
  short sessVRefNum;
  long sessDirID;
  Str255 psessionname;

  error = get_session_dir(kDontCreateFolder, &sessVRefNum, &sessDirID);

  c2pstrcpy(psessionname, sessionname);
  error = FSMakeFSSpec(sessVRefNum, sessDirID, psessionname, &sessfile);
  if (error != noErr)
    goto out;

  error = FSpDelete(&sessfile);
  return;
out:
  fatalbox("Delete session failed (%d)", error);
}

struct enum_settings_state {
  short vRefNum;
  long dirID;
  int index;
};

void *enum_settings_start(void)
{
  OSErr error;
  struct enum_settings_state *state;

  state = safemalloc(sizeof(*state));
  error = get_session_dir(kDontCreateFolder, &state->vRefNum, &state->dirID);
  if (error != noErr) {
    safefree(state);
    return NULL;
  }
  state->index = 1;
  return state;
}

char *enum_settings_next(void *handle, char *buffer, int buflen)
{
  struct enum_settings_state *e = handle;
  CInfoPBRec pb;
  OSErr error = noErr;
  Str255 name;

  if (e == NULL)
    return NULL;
  do {
    pb.hFileInfo.ioNamePtr = name;
    pb.hFileInfo.ioVRefNum = e->vRefNum;
    pb.hFileInfo.ioDirID = e->dirID;
    pb.hFileInfo.ioFDirIndex = e->index++;
    error = PBGetCatInfoSync(&pb);
    if (error != noErr)
      return NULL;
  } while (!((pb.hFileInfo.ioFlAttrib & ioDirMask) == 0 &&
             pb.hFileInfo.ioFlFndrInfo.fdCreator == PUTTY_CREATOR &&
             pb.hFileInfo.ioFlFndrInfo.fdType == SESS_TYPE &&
             name[0] < buflen));

  p2cstrcpy(buffer, name);
  return buffer;
}

void enum_settings_finish(void *handle)
{

  safefree(handle);
}

#define SEED_SIZE 512

void read_random_seed(noise_consumer_t consumer)
{
  short puttyVRefNum;
  long puttyDirID;
  OSErr error;
  char buf[SEED_SIZE];
  short refnum;
  long count = SEED_SIZE;

  if (get_putty_dir(kDontCreateFolder, &puttyVRefNum, &puttyDirID) != noErr)
    return;
  if (HOpenDF(
          puttyVRefNum, puttyDirID, "\pPuTTY Random Seed", fsRdPerm, &refnum) !=
      noErr)
    return;
  error = FSRead(refnum, &count, buf);
  if (error != noErr && error != eofErr)
    return;
  (*consumer)(buf, count);
  FSClose(refnum);
}

/*
 * We don't bother with the usual FSpExchangeFiles dance here because
 * it doesn't really matter if the old random seed gets lost.
 */
void write_random_seed(void *data, int len)
{
  short puttyVRefNum;
  long puttyDirID;
  OSErr error;
  FSSpec dstfile;
  short refnum;
  long count = len;

  if (get_putty_dir(kCreateFolder, &puttyVRefNum, &puttyDirID) != noErr)
    return;

  error =
      FSMakeFSSpec(puttyVRefNum, puttyDirID, "\pPuTTY Random Seed", &dstfile);
  if (error == fnfErr) {
    /* Set up standard resources */
    FSpCreateResFile(&dstfile, INTERNAL_CREATOR, SEED_TYPE, smRoman);
    refnum = FSpOpenResFile(&dstfile, fsWrPerm);
    if (ResError() == noErr) {
      copy_resource('STR ', -16397);
      CloseResFile(refnum);
    }
  } else if (error != noErr)
    return;

  if (FSpOpenDF(&dstfile, fsWrPerm, &refnum) != noErr)
    return;
  FSWrite(refnum, &count, data);
  FSClose(refnum);

  return;
}

/*
 * Emacs magic:
 * Local Variables:
 * c-file-style: "simon"
 * End:
 */
