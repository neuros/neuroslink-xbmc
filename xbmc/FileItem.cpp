/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "FileItem.h"
#include "Util.h"
#include "Picture.h"
#include "PlayListFactory.h"
#include "Shortcut.h"
#include "Crc32.h"
#include "FileSystem/DirectoryCache.h"
#include "FileSystem/StackDirectory.h"
#include "FileSystem/FileCurl.h"
#include "FileSystem/MultiPathDirectory.h"
#include "FileSystem/MusicDatabaseDirectory.h"
#include "FileSystem/VideoDatabaseDirectory.h"
#include "FileSystem/IDirectory.h"
#include "FileSystem/FactoryDirectory.h"
#include "MusicInfoTagLoaderFactory.h"
#include "CueDocument.h"
#include "utils/fstrcmp.h"
#include "VideoDatabase.h"
#include "MusicDatabase.h"
#include "SortFileItem.h"
#include "utils/TuxBoxUtil.h"
#include "VideoInfoTag.h"
#include "utils/SingleLock.h"
#include "MusicInfoTag.h"
#include "PictureInfoTag.h"
#include "Artist.h"
#include "Album.h"
#include "Song.h"
#include "URL.h"
#include "Settings.h"

using namespace std;
using namespace XFILE;
using namespace DIRECTORY;
using namespace PLAYLIST;
using namespace MUSIC_INFO;

CFileItem::CFileItem(const CSong& song)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(song.strTitle);
  m_strPath = song.strFileName;
  GetMusicInfoTag()->SetSong(song);
  m_lStartOffset = song.iStartOffset;
  m_lEndOffset = song.iEndOffset;
  m_strThumbnailImage = song.strThumb;
}

CFileItem::CFileItem(const CStdString &path, const CAlbum& album)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(album.strAlbum);
  m_strPath = path;
  m_bIsFolder = true;
  m_strLabel2 = album.strArtist;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetAlbum(album);
  if (album.thumbURL.m_url.size() > 0)
    m_strThumbnailImage = album.thumbURL.m_url[0].m_url;
  else
    m_strThumbnailImage.clear();

  SetProperty("description", album.strReview);
  SetProperty("theme", album.strThemes);
  SetProperty("mood", album.strMoods);
  SetProperty("style", album.strStyles);
  SetProperty("type", album.strType);
  SetProperty("label", album.strLabel);
  if (album.iRating > 0)
    SetProperty("rating", album.iRating);
}

CFileItem::CFileItem(const CVideoInfoTag& movie)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(movie.m_strTitle);
  if (movie.m_strFileNameAndPath.IsEmpty())
  {
    m_strPath = movie.m_strPath;
    CUtil::AddSlashAtEnd(m_strPath);
    m_bIsFolder = true;
  }
  else
  {
    m_strPath = movie.m_strFileNameAndPath;
    m_bIsFolder = false;
  }
  *GetVideoInfoTag() = movie;
  FillInDefaultIcon();
  SetCachedVideoThumb();
  SetInvalid();
}

CFileItem::CFileItem(const CArtist& artist)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(artist.strArtist);
  m_strPath = artist.strArtist;
  m_bIsFolder = true;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetArtist(artist.strArtist);
}

CFileItem::CFileItem(const CGenre& genre)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(genre.strGenre);
  m_strPath = genre.strGenre;
  m_bIsFolder = true;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetGenre(genre.strGenre);
}

CFileItem::CFileItem(const CFileItem& item): CGUIListItem()
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  *this = item;
}

CFileItem::CFileItem(const CGUIListItem& item)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  // not particularly pretty, but it gets around the issue of Reset() defaulting
  // parameters in the CGUIListItem base class.
  *((CGUIListItem *)this) = item;
}

CFileItem::CFileItem(void)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
}

CFileItem::CFileItem(const CStdString& strLabel)
    : CGUIListItem()
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(strLabel);
}

CFileItem::CFileItem(const CStdString& strPath, bool bIsFolder)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  m_strPath = strPath;
  m_bIsFolder = bIsFolder;
  // tuxbox urls cannot have a / at end
  if (m_bIsFolder && !m_strPath.IsEmpty() && !IsFileFolder() && !CUtil::IsTuxBox(m_strPath))
  {
#ifdef DEBUG
    ASSERT(CUtil::HasSlashAtEnd(m_strPath));
#endif
    CUtil::AddSlashAtEnd(m_strPath);
  }
}

CFileItem::CFileItem(const CMediaSource& share)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  m_bIsFolder = true;
  m_bIsShareOrDrive = true;
  m_strPath = share.strPath;
  CUtil::AddSlashAtEnd(m_strPath);
  CStdString label = share.strName;
  if (share.strStatus.size())
    label.Format("%s (%s)", share.strName.c_str(), share.strStatus.c_str());
  SetLabel(label);
  m_iLockMode = share.m_iLockMode;
  m_strLockCode = share.m_strLockCode;
  m_iHasLock = share.m_iHasLock;
  m_iBadPwdCount = share.m_iBadPwdCount;
  m_iDriveType = share.m_iDriveType;
  m_strThumbnailImage = share.m_strThumbnailImage;
  SetLabelPreformated(true);
}

CFileItem::~CFileItem(void)
{
  delete m_musicInfoTag;
  delete m_videoInfoTag;
  delete m_pictureInfoTag;

  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
}

const CFileItem& CFileItem::operator=(const CFileItem& item)
{
  if (this == &item) return * this;
  CGUIListItem::operator=(item);
  m_bLabelPreformated=item.m_bLabelPreformated;
  FreeMemory();
  m_strPath = item.m_strPath;
  m_bIsParentFolder = item.m_bIsParentFolder;
  m_iDriveType = item.m_iDriveType;
  m_bIsShareOrDrive = item.m_bIsShareOrDrive;
  m_dateTime = item.m_dateTime;
  m_dwSize = item.m_dwSize;
  if (item.HasMusicInfoTag())
  {
    m_musicInfoTag = GetMusicInfoTag();
    if (m_musicInfoTag)
      *m_musicInfoTag = *item.m_musicInfoTag;
  }
  else
  {
    delete m_musicInfoTag;
    m_musicInfoTag = NULL;
  }

  if (item.HasVideoInfoTag())
  {
    m_videoInfoTag = GetVideoInfoTag();
    if (m_videoInfoTag)
      *m_videoInfoTag = *item.m_videoInfoTag;
  }
  else
  {
    delete m_videoInfoTag;
    m_videoInfoTag = NULL;
  }

  if (item.HasPictureInfoTag())
  {
    m_pictureInfoTag = GetPictureInfoTag();
    if (m_pictureInfoTag)
      *m_pictureInfoTag = *item.m_pictureInfoTag;
  }
  else
  {
    delete m_pictureInfoTag;
    m_pictureInfoTag = NULL;
  }

  m_lStartOffset = item.m_lStartOffset;
  m_lEndOffset = item.m_lEndOffset;
  m_strDVDLabel = item.m_strDVDLabel;
  m_strTitle = item.m_strTitle;
  m_iprogramCount = item.m_iprogramCount;
  m_idepth = item.m_idepth;
  m_iLockMode = item.m_iLockMode;
  m_strLockCode = item.m_strLockCode;
  m_iHasLock = item.m_iHasLock;
  m_iBadPwdCount = item.m_iBadPwdCount;
  m_bCanQueue=item.m_bCanQueue;
  m_contenttype = item.m_contenttype;
  m_extrainfo = item.m_extrainfo;
  return *this;
}

void CFileItem::Reset()
{
  m_strLabel2.Empty();
  SetLabel("");
  m_bLabelPreformated=false;
  FreeIcons();
  m_overlayIcon = ICON_OVERLAY_NONE;
  m_bSelected = false;
  m_strDVDLabel.Empty();
  m_strTitle.Empty();
  m_strPath.Empty();
  m_dwSize = 0;
  m_bIsFolder = false;
  m_bIsParentFolder=false;
  m_bIsShareOrDrive = false;
  m_dateTime.Reset();
  m_iDriveType = CMediaSource::SOURCE_TYPE_UNKNOWN;
  m_lStartOffset = 0;
  m_lEndOffset = 0;
  m_iprogramCount = 0;
  m_idepth = 1;
  m_iLockMode = LOCK_MODE_EVERYONE;
  m_strLockCode = "";
  m_iBadPwdCount = 0;
  m_iHasLock = 0;
  m_bCanQueue=true;
  m_contenttype = "";
  delete m_musicInfoTag;
  m_musicInfoTag=NULL;
  delete m_videoInfoTag;
  m_videoInfoTag=NULL;
  delete m_pictureInfoTag;
  m_pictureInfoTag=NULL;
  m_extrainfo.Empty();
  SetInvalid();
}

void CFileItem::Serialize(CArchive& ar)
{
  CGUIListItem::Serialize(ar);

  if (ar.IsStoring())
  {
    ar << m_bIsParentFolder;
    ar << m_bLabelPreformated;
    ar << m_strPath;
    ar << m_bIsShareOrDrive;
    ar << m_iDriveType;
    ar << m_dateTime;
    ar << m_dwSize;
    ar << m_strDVDLabel;
    ar << m_strTitle;
    ar << m_iprogramCount;
    ar << m_idepth;
    ar << m_lStartOffset;
    ar << m_lEndOffset;
    ar << m_iLockMode;
    ar << m_strLockCode;
    ar << m_iBadPwdCount;

    ar << m_bCanQueue;
    ar << m_contenttype;
    ar << m_extrainfo;

    if (m_musicInfoTag)
    {
      ar << 1;
      ar << *m_musicInfoTag;
    }
    else
      ar << 0;
    if (m_videoInfoTag)
    {
      ar << 1;
      ar << *m_videoInfoTag;
    }
    else
      ar << 0;
    if (m_pictureInfoTag)
    {
      ar << 1;
      ar << *m_pictureInfoTag;
    }
    else
      ar << 0;
  }
  else
  {
    ar >> m_bIsParentFolder;
    ar >> m_bLabelPreformated;
    ar >> m_strPath;
    ar >> m_bIsShareOrDrive;
    ar >> m_iDriveType;
    ar >> m_dateTime;
    ar >> m_dwSize;
    ar >> m_strDVDLabel;
    ar >> m_strTitle;
    ar >> m_iprogramCount;
    ar >> m_idepth;
    ar >> m_lStartOffset;
    ar >> m_lEndOffset;
    ar >> (int&)m_iLockMode;
    ar >> m_strLockCode;
    ar >> m_iBadPwdCount;

    ar >> m_bCanQueue;
    ar >> m_contenttype;
    ar >> m_extrainfo;

    int iType;
    ar >> iType;
    if (iType == 1)
      ar >> *GetMusicInfoTag();
    ar >> iType;
    if (iType == 1)
      ar >> *GetVideoInfoTag();
    ar >> iType;
    if (iType == 1)
      ar >> *GetPictureInfoTag();

    SetInvalid();
  }
}
bool CFileItem::Exists() const
{
  if (m_strPath.IsEmpty() || m_strPath.Equals("add") || IsInternetStream() || IsParentFolder() || IsVirtualDirectoryRoot() || IsPlugin())
    return true;

  if (IsVideoDb() && HasVideoInfoTag())
  {
    CFileItem dbItem(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath, m_bIsFolder);
    return dbItem.Exists();
  }

  CStdString strPath = m_strPath;
 
  if (CUtil::IsMultiPath(strPath))
    strPath = CMultiPathDirectory::GetFirstPath(strPath);
 
  if (CUtil::IsStack(strPath))
    strPath = CStackDirectory::GetFirstStackedFile(strPath);

  if (m_bIsFolder)
    return CDirectory::Exists(strPath);
  else
    return CFile::Exists(strPath);

  return false;
}

bool CFileItem::IsVideo() const
{
  /* check preset content type */
  if( m_contenttype.Left(6).Equals("video/") )
    return true;

  if (m_strPath.Left(7).Equals("tuxbox:"))
    return true;

  if (m_strPath.Left(10).Equals("hdhomerun:"))
    return true;

  CStdString extension;
  if( m_contenttype.Left(12).Equals("application/") )
  { /* check for some standard types */
    extension = m_contenttype.Mid(12);
    if( extension.Equals("ogg")
     || extension.Equals("mp4")
     || extension.Equals("mxf") )
     return true;
  }

  CUtil::GetExtension(m_strPath, extension);

  if (extension.IsEmpty())
    return false;

  extension.ToLower();

  if (g_stSettings.m_videoExtensions.Find(extension) != -1)
    return true;

  return false;
}

bool CFileItem::IsAudio() const
{
  if (IsCDDA()) return true;
  if (IsShoutCast() && !m_bIsFolder) return true;
  if (IsLastFM() && !m_bIsFolder) return true;

  /* check preset content type */
  if( m_contenttype.Left(6).Equals("audio/") )
    return true;

  CStdString extension;
  if( m_contenttype.Left(12).Equals("application/") )
  { /* check for some standard types */
    extension = m_contenttype.Mid(12);
    if( extension.Equals("ogg")
     || extension.Equals("mp4")
     || extension.Equals("mxf") )
     return true;
  }

  CUtil::GetExtension(m_strPath, extension);

  if (extension.IsEmpty())
    return false;

  extension.ToLower();
  if (g_stSettings.m_musicExtensions.Find(extension) != -1)
    return true;

  return false;
}

bool CFileItem::IsPicture() const
{
  if( m_contenttype.Left(6).Equals("image/") )
    return true;

  CStdString extension;
  CUtil::GetExtension(m_strPath, extension);

  if (extension.IsEmpty())
    return false;

  extension.ToLower();
  if (g_stSettings.m_pictureExtensions.Find(extension) != -1)
    return true;

  if (extension == ".tbn")
    return true;

  return false;
}

bool CFileItem::IsLyrics() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  return (strExtension.CompareNoCase(".cdg") == 0 || strExtension.CompareNoCase(".lrc") == 0);
}

bool CFileItem::IsCUESheet() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  return (strExtension.CompareNoCase(".cue") == 0);
}

bool CFileItem::IsShoutCast() const
{
  if (strstr(m_strPath.c_str(), "shout:") ) return true;
  return false;
}
bool CFileItem::IsLastFM() const
{
  if (strstr(m_strPath.c_str(), "lastfm:") ) return true;
  return false;
}

bool CFileItem::IsInternetStream() const
{
  CURL url(m_strPath);
  CStdString strProtocol = url.GetProtocol();
  strProtocol.ToLower();

  if (strProtocol.size() == 0 || HasProperty("IsHTTPDirectory"))
    return false;

  // there's nothing to stop internet streams from being stacked
  if (strProtocol == "stack")
  {
    CFileItem fileItem(CStackDirectory::GetFirstStackedFile(m_strPath), false);
    return fileItem.IsInternetStream();
  }

  if (strProtocol == "shout" || strProtocol == "mms" ||
      strProtocol == "http" || /*strProtocol == "ftp" ||*/
      strProtocol == "rtsp" || strProtocol == "rtp" ||
      strProtocol == "udp"  || strProtocol == "lastfm" ||
      strProtocol == "https" || strProtocol == "rtmp")
    return true;

  return false;
}

bool CFileItem::IsFileFolder() const
{
  return (
    m_bIsFolder && (
    IsPlugin() ||
    IsSmartPlayList() ||
    IsPlayList() ||
    IsZIP() ||
    IsRAR() ||
    IsType(".ogg") ||
    IsType(".nsf") ||
    IsType(".sid") ||
    IsType(".sap") ||
    IsShoutCast()
    )
    );
}


bool CFileItem::IsSmartPlayList() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  strExtension.ToLower();
  return (strExtension == ".xsp");
}

bool CFileItem::IsPlayList() const
{
  return CPlayListFactory::IsPlaylist(m_strPath);
}

bool CFileItem::IsPythonScript() const
{
  return CUtil::GetExtension(m_strPath).Equals(".py", false);
}

bool CFileItem::IsXBE() const
{
  return CUtil::GetExtension(m_strPath).Equals(".xbe", false);
}

bool CFileItem::IsType(const char *ext) const
{
  return CUtil::GetExtension(m_strPath).Equals(ext, false);
}

bool CFileItem::IsDefaultXBE() const
{
  CStdString filename = CUtil::GetFileName(m_strPath);
  if (filename.Equals("default.xbe")) return true;
  return false;
}

bool CFileItem::IsShortCut() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cut", false);
}

bool CFileItem::IsNFO() const
{
  return CUtil::GetExtension(m_strPath).Equals(".nfo", false);
}

bool CFileItem::IsDVDImage() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  if (strExtension.Equals(".img") || strExtension.Equals(".iso") || strExtension.Equals(".nrg")) return true;
  return false;
}

bool CFileItem::IsDVDFile(bool bVobs /*= true*/, bool bIfos /*= true*/) const
{
  CStdString strFileName = CUtil::GetFileName(m_strPath);
  if (bIfos)
  {
    if (strFileName.Equals("video_ts.ifo")) return true;
    if (strFileName.Left(4).Equals("vts_") && strFileName.Right(6).Equals("_0.ifo") && strFileName.length() == 12) return true;
  }
  if (bVobs)
  {
    if (strFileName.Equals("video_ts.vob")) return true;
    if (strFileName.Left(4).Equals("vts_") && strFileName.Right(4).Equals(".vob")) return true;
  }

  return false;
}

bool CFileItem::IsRAR() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  if ( (strExtension.CompareNoCase(".rar") == 0) || ((strExtension.Equals(".001") && m_strPath.Mid(m_strPath.length()-7,7).CompareNoCase(".ts.001"))) ) return true; // sometimes the first rar is named .001
  return false;
}

bool CFileItem::IsZIP() const
{
  return CUtil::GetExtension(m_strPath).Equals(".zip", false);
}

bool CFileItem::IsCBZ() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cbz", false);
}

bool CFileItem::IsCBR() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cbr", false);
}

bool CFileItem::IsStack() const
{
  return CUtil::IsStack(m_strPath);
}

bool CFileItem::IsPlugin() const
{
  CURL url(m_strPath);
  return url.GetProtocol().Equals("plugin") && !url.GetFileName().IsEmpty();
}

bool CFileItem::IsPluginRoot() const
{
  CURL url(m_strPath);
  return url.GetProtocol().Equals("plugin") && url.GetFileName().IsEmpty();
}

bool CFileItem::IsMultiPath() const
{
  return CUtil::IsMultiPath(m_strPath);
}

bool CFileItem::IsCDDA() const
{
  return CUtil::IsCDDA(m_strPath);
}

bool CFileItem::IsDVD() const
{
  return CUtil::IsDVD(m_strPath) || m_iDriveType == CMediaSource::SOURCE_TYPE_DVD;
}

bool CFileItem::IsOnDVD() const
{
  return CUtil::IsOnDVD(m_strPath) || m_iDriveType == CMediaSource::SOURCE_TYPE_DVD;
}

bool CFileItem::IsOnLAN() const
{
  return CUtil::IsOnLAN(m_strPath);
}

bool CFileItem::IsISO9660() const
{
  return CUtil::IsISO9660(m_strPath);
}

bool CFileItem::IsRemote() const
{
  return CUtil::IsRemote(m_strPath);
}

bool CFileItem::IsSmb() const
{
  return CUtil::IsSmb(m_strPath);
}

bool CFileItem::IsDAAP() const
{
  return CUtil::IsDAAP(m_strPath);
}

bool CFileItem::IsTuxBox() const
{
  return CUtil::IsTuxBox(m_strPath);
}

bool CFileItem::IsMythTV() const
{
  return CUtil::IsMythTV(m_strPath);
}

bool CFileItem::IsVTP() const
{
  return CUtil::IsVTP(m_strPath);
}

bool CFileItem::IsTV() const
{
  return CUtil::IsTV(m_strPath);
}

bool CFileItem::IsHD() const
{
  return CUtil::IsHD(m_strPath);
}

bool CFileItem::IsMusicDb() const
{
  if (strstr(m_strPath.c_str(), "musicdb:") ) return true;
  return false;
}

bool CFileItem::IsVideoDb() const
{
  if (strstr(m_strPath.c_str(), "videodb:") ) return true;
  return false;
}

bool CFileItem::IsVirtualDirectoryRoot() const
{
  return (m_bIsFolder && m_strPath.IsEmpty());
}

bool CFileItem::IsMemoryUnit() const
{
  CURL url(m_strPath);
  return url.GetProtocol().Left(3).Equals("mem");
}

bool CFileItem::IsRemovable() const
{
  return IsOnDVD() || IsCDDA() || IsMemoryUnit() || m_iDriveType == CMediaSource::SOURCE_TYPE_REMOVABLE;
}

bool CFileItem::IsReadOnly() const
{
  if (IsParentFolder()) return true;
  if (m_bIsShareOrDrive) return true;
  return !CUtil::SupportsFileOperations(m_strPath);
}

void CFileItem::FillInDefaultIcon()
{
  //CLog::Log(LOGINFO, "FillInDefaultIcon(%s)", pItem->GetLabel().c_str());
  // find the default icon for a file or folder item
  // for files this can be the (depending on the file type)
  //   default picture for photo's
  //   default picture for songs
  //   default picture for videos
  //   default picture for shortcuts
  //   default picture for playlists
  //   or the icon embedded in an .xbe
  //
  // for folders
  //   for .. folders the default picture for parent folder
  //   for other folders the defaultFolder.png

  CStdString strThumb;
  CStdString strExtension;
  if (GetIconImage() == "")
  {
    if (!m_bIsFolder)
    {
      if ( IsPlayList() )
      {
        SetIconImage("defaultPlaylist.png");
      }
      else if ( IsPicture() )
      {
        // picture
        SetIconImage("defaultPicture.png");
      }
      else if ( IsXBE() )
      {
        // xbe
        SetIconImage("defaultProgram.png");
      }
      else if ( IsAudio() )
      {
        // audio
        SetIconImage("defaultAudio.png");
      }
      else if ( IsVideo() )
      {
        // video
        SetIconImage("defaultVideo.png");
      }
      else if ( IsShortCut() && !IsLabelPreformated() )
      {
        // shortcut
        CStdString strDescription;
        CStdString strFName;
        strFName = CUtil::GetFileName(m_strPath);

        int iPos = strFName.ReverseFind(".");
        strDescription = strFName.Left(iPos);
        SetLabel(strDescription);
        SetIconImage("defaultShortcut.png");
      }
      else if ( IsPythonScript() )
      {
        SetIconImage("DefaultScript.png");
      }
      else
      {
        // default icon for unknown file type
        SetIconImage("defaultFile.png");
      }
    }
    else
    {
      if ( IsPlayList() )
      {
        SetIconImage("defaultPlaylist.png");
      }
      else if (IsParentFolder())
      {
        SetIconImage("defaultFolderBack.png");
      }
      else
      {
        SetIconImage("defaultFolder.png");
      }
    }
  }
  // Set the icon overlays (if applicable)
  if (!HasOverlay())
  {
    if (CUtil::IsInRAR(m_strPath))
      SetOverlayImage(CGUIListItem::ICON_OVERLAY_RAR);
    else if (CUtil::IsInZIP(m_strPath))
      SetOverlayImage(CGUIListItem::ICON_OVERLAY_ZIP);
  }
}

CStdString CFileItem::GetCachedArtistThumb() const
{
  return GetCachedThumb("artist"+GetLabel(),g_settings.GetMusicArtistThumbFolder());
}

CStdString CFileItem::GetCachedProfileThumb() const
{
  return GetCachedThumb("profile"+m_strPath,CUtil::AddFileToFolder(g_settings.GetUserDataFolder(),"Thumbnails\\Profiles"));
}

CStdString CFileItem::GetCachedSeasonThumb() const
{
  CStdString seasonPath;
  if (HasVideoInfoTag())
    seasonPath = GetVideoInfoTag()->m_strPath;

  return GetCachedThumb("season"+seasonPath+GetLabel(),g_settings.GetVideoThumbFolder(),true);
}

CStdString CFileItem::GetCachedActorThumb() const
{
  return GetCachedThumb("actor"+GetLabel(),g_settings.GetVideoThumbFolder(),true);
}

void CFileItem::SetCachedArtistThumb()
{
  CStdString thumb(GetCachedArtistThumb());
  if (CFile::Exists(thumb))
  {
    // found it, we are finished.
    SetThumbnailImage(thumb);
//    SetIconImage(strThumb);
  }
}

// set the album thumb for a file or folder
void CFileItem::SetMusicThumb(bool alwaysCheckRemote /* = true */)
{
  if (HasThumbnail()) return;
  SetCachedMusicThumb();
  if (!HasThumbnail())
    SetUserMusicThumb(alwaysCheckRemote);
}

void CFileItem::SetCachedSeasonThumb()
{
  CStdString thumb(GetCachedSeasonThumb());
  if (CFile::Exists(thumb))
  {
    // found it, we are finished.
    SetThumbnailImage(thumb);
  }
}

void CFileItem::RemoveExtension()
{
  if (m_bIsFolder)
    return ;
  CStdString strLabel = GetLabel();
  CUtil::RemoveExtension(strLabel);
  SetLabel(strLabel);
}

void CFileItem::CleanString()
{
  if (IsTV())
    return;
  CStdString strLabel = GetLabel();
  CUtil::CleanString(strLabel, m_bIsFolder);
  SetLabel(strLabel);
}

void CFileItem::SetLabel(const CStdString &strLabel)
{
  if (strLabel=="..")
  {
    m_bIsParentFolder=true;
    m_bIsFolder=true;
    SetLabelPreformated(true);
  }
  CGUIListItem::SetLabel(strLabel);
}

void CFileItem::SetFileSizeLabel()
{
  if( m_bIsFolder && m_dwSize == 0 )
    SetLabel2("");
  else
    SetLabel2(StringUtils::SizeToString(m_dwSize));
}

CURL CFileItem::GetAsUrl() const
{
  return CURL(m_strPath);
}

bool CFileItem::CanQueue() const
{
  return m_bCanQueue;
}

void CFileItem::SetCanQueue(bool bYesNo)
{
  m_bCanQueue=bYesNo;
}

bool CFileItem::IsParentFolder() const
{
  return m_bIsParentFolder;
}

const CStdString& CFileItem::GetContentType() const
{
  if( m_contenttype.IsEmpty() )
  {
    // discard const qualifyier
    CStdString& m_ref = (CStdString&)m_contenttype;

    if( m_bIsFolder )
      m_ref = "x-directory/normal";
    else if( m_strPath.Left(8).Equals("shout://")
          || m_strPath.Left(7).Equals("http://")
          || m_strPath.Left(8).Equals("https://")
          || m_strPath.Left(7).Equals("upnp://"))
    {
      CFileCurl::GetContent(GetAsUrl(), m_ref);

      // try to get content type again but with an NSPlayer User-Agent
      // in order for server to provide correct content-type.  Allows us
      // to properly detect an MMS stream
      if (m_ref.Left(11).Equals("video/x-ms-"))
        CFileCurl::GetContent(GetAsUrl(), m_ref, "NSPlayer/11.00.6001.7000");

      // make sure there are no options set in content type
      // content type can look like "video/x-ms-asf ; charset=utf8"
      int i = m_ref.Find(';');
      if(i>=0)
        m_ref.Delete(i,m_ref.length()-i);
      m_ref.Trim();
    }

    // if it's still empty set to an unknown type
    if( m_ref.IsEmpty() )
      m_ref = "application/octet-stream";
  }

  // change protocol to mms for the following content-type.  Allows us to create proper FileMMS.
  if( m_contenttype.Left(32).Equals("application/vnd.ms.wms-hdr.asfv1") || m_contenttype.Left(24).Equals("application/x-mms-framed") )
  {
    CStdString& m_path = (CStdString&)m_strPath;
    m_path.Replace("http:", "mms:");
  }

  return m_contenttype;
}

bool CFileItem::IsSamePath(const CFileItem *item) const
{
  if (!item)
    return false;

  if (item->m_strPath == m_strPath && item->m_lStartOffset == m_lStartOffset) return true;
  if (IsMusicDb() && HasMusicInfoTag())
  {
    CFileItem dbItem(m_musicInfoTag->GetURL(), false);
    dbItem.m_lStartOffset = m_lStartOffset;
    return dbItem.IsSamePath(item);
  }
  if (IsVideoDb() && HasVideoInfoTag())
  {
    CFileItem dbItem(m_videoInfoTag->m_strFileNameAndPath, false);
    dbItem.m_lStartOffset = m_lStartOffset;
    return dbItem.IsSamePath(item);
  }
  if (item->IsMusicDb() && item->HasMusicInfoTag())
  {
    CFileItem dbItem(item->m_musicInfoTag->GetURL(), false);
    dbItem.m_lStartOffset = item->m_lStartOffset;
    return IsSamePath(&dbItem);
  }
  if (item->IsVideoDb() && item->HasVideoInfoTag())
  {
    CFileItem dbItem(item->m_videoInfoTag->m_strFileNameAndPath, false);
    dbItem.m_lStartOffset = item->m_lStartOffset;
    return IsSamePath(&dbItem);
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////////
/////
///// CFileItemList
/////
//////////////////////////////////////////////////////////////////////////////////

CFileItemList::CFileItemList()
{
  m_fastLookup = false;
  m_bIsFolder=true;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_replaceListing = false;
}

CFileItemList::CFileItemList(const CStdString& strPath)
{
  m_strPath=strPath;
  m_fastLookup = false;
  m_bIsFolder=true;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_replaceListing = false;
}

CFileItemList::~CFileItemList()
{
  Clear();
}

CFileItemPtr CFileItemList::operator[] (int iItem)
{
  return Get(iItem);
}

const CFileItemPtr CFileItemList::operator[] (int iItem) const
{
  return Get(iItem);
}

CFileItemPtr CFileItemList::operator[] (const CStdString& strPath)
{
  return Get(strPath);
}

const CFileItemPtr CFileItemList::operator[] (const CStdString& strPath) const
{
  return Get(strPath);
}

void CFileItemList::SetFastLookup(bool fastLookup)
{
  CSingleLock lock(m_lock);

  if (fastLookup && !m_fastLookup)
  { // generate the map
    m_map.clear();
    for (unsigned int i=0; i < m_items.size(); i++)
    {
      CFileItemPtr pItem = m_items[i];
      CStdString path(pItem->m_strPath); path.ToLower();
      m_map.insert(MAPFILEITEMSPAIR(path, pItem));
    }
  }
  if (!fastLookup && m_fastLookup)
    m_map.clear();
  m_fastLookup = fastLookup;
}

bool CFileItemList::Contains(const CStdString& fileName) const
{
  CSingleLock lock(m_lock);

  // checks case insensitive
  CStdString checkPath(fileName); checkPath.ToLower();
  if (m_fastLookup)
    return m_map.find(checkPath) != m_map.end();
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    const CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(checkPath))
      return true;
  }
  return false;
}

void CFileItemList::Clear()
{
  CSingleLock lock(m_lock);

  ClearItems();
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortDetails.clear();
  m_replaceListing = false;
  m_content.Empty();
}

void CFileItemList::ClearItems()
{
  CSingleLock lock(m_lock);
  // make sure we free the memory of the items (these are GUIControls which may have allocated resources)
  FreeMemory();
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr item = m_items[i];
    item->FreeMemory();
  }
  m_items.clear();
  m_map.clear();
}

void CFileItemList::Add(const CFileItemPtr &pItem)
{
  CSingleLock lock(m_lock);

  m_items.push_back(pItem);
  if (m_fastLookup)
  {
    CStdString path(pItem->m_strPath); path.ToLower();
    m_map.insert(MAPFILEITEMSPAIR(path, pItem));
  }
}

void CFileItemList::AddFront(const CFileItemPtr &pItem, int itemPosition)
{
  CSingleLock lock(m_lock);

  if (itemPosition >= 0)
  {
    m_items.insert(m_items.begin()+itemPosition, pItem);
  }
  else
  {
    m_items.insert(m_items.begin()+(m_items.size()+itemPosition), pItem);
  }
  if (m_fastLookup)
  {
    CStdString path(pItem->m_strPath); path.ToLower();
    m_map.insert(MAPFILEITEMSPAIR(path, pItem));
  }
}

void CFileItemList::Remove(CFileItem* pItem)
{
  CSingleLock lock(m_lock);

  for (IVECFILEITEMS it = m_items.begin(); it != m_items.end(); ++it)
  {
    if (pItem == it->get())
    {
      m_items.erase(it);
      if (m_fastLookup)
      {
        CStdString path(pItem->m_strPath); path.ToLower();
        m_map.erase(path);
      }
      break;
    }
  }
}

void CFileItemList::Remove(int iItem)
{
  CSingleLock lock(m_lock);

  if (iItem >= 0 && iItem < (int)Size())
  {
    CFileItemPtr pItem = *(m_items.begin() + iItem);
    if (m_fastLookup)
    {
      CStdString path(pItem->m_strPath); path.ToLower();
      m_map.erase(path);
    }
    m_items.erase(m_items.begin() + iItem);
  }
}

void CFileItemList::Append(const CFileItemList& itemlist)
{
  CSingleLock lock(m_lock);

  for (int i = 0; i < itemlist.Size(); ++i)
    Add(itemlist[i]);
}

void CFileItemList::Assign(const CFileItemList& itemlist, bool append)
{
  CSingleLock lock(m_lock);
  if (!append)
    Clear();
  Append(itemlist);
  m_strPath = itemlist.m_strPath;
  m_sortDetails = itemlist.m_sortDetails;
  m_replaceListing = itemlist.m_replaceListing;
  m_content = itemlist.m_content;
  m_mapProperties = itemlist.m_mapProperties;
  m_cacheToDisc = itemlist.m_cacheToDisc;
}

CFileItemPtr CFileItemList::Get(int iItem)
{
  CSingleLock lock(m_lock);

  if (iItem > -1)
    return m_items[iItem];

  return CFileItemPtr();
}

const CFileItemPtr CFileItemList::Get(int iItem) const
{
  CSingleLock lock(m_lock);

  if (iItem > -1)
    return m_items[iItem];

  return CFileItemPtr();
}

CFileItemPtr CFileItemList::Get(const CStdString& strPath)
{
  CSingleLock lock(m_lock);

  CStdString pathToCheck(strPath); pathToCheck.ToLower();
  if (m_fastLookup)
  {
    IMAPFILEITEMS it=m_map.find(pathToCheck);
    if (it != m_map.end())
      return it->second;

    return CFileItemPtr();
  }
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(pathToCheck))
      return pItem;
  }

  return CFileItemPtr();
}

const CFileItemPtr CFileItemList::Get(const CStdString& strPath) const
{
  CSingleLock lock(m_lock);

  CStdString pathToCheck(strPath); pathToCheck.ToLower();
  if (m_fastLookup)
  {
    map<CStdString, CFileItemPtr>::const_iterator it=m_map.find(pathToCheck);
    if (it != m_map.end())
      return it->second;

    return CFileItemPtr();
  }
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(pathToCheck))
      return pItem;
  }

  return CFileItemPtr();
}

int CFileItemList::Size() const
{
  CSingleLock lock(m_lock);
  return (int)m_items.size();
}

bool CFileItemList::IsEmpty() const
{
  CSingleLock lock(m_lock);
  return (m_items.size() <= 0);
}

void CFileItemList::Reserve(int iCount)
{
  CSingleLock lock(m_lock);
  m_items.reserve(iCount);
}

void CFileItemList::Sort(FILEITEMLISTCOMPARISONFUNC func)
{
  CSingleLock lock(m_lock);
  DWORD dwStart = GetTickCount();
  std::sort(m_items.begin(), m_items.end(), func);
  DWORD dwElapsed = GetTickCount() - dwStart;
  CLog::Log(LOGDEBUG,"%s, sorting took %u millis", __FUNCTION__, dwElapsed);
}

void CFileItemList::FillSortFields(FILEITEMFILLFUNC func)
{
  CSingleLock lock(m_lock);
  std::for_each(m_items.begin(), m_items.end(), func);
}

void CFileItemList::Sort(SORT_METHOD sortMethod, SORT_ORDER sortOrder)
{
  //  Already sorted?
  if (sortMethod==m_sortMethod && m_sortOrder==sortOrder)
    return;

  switch (sortMethod)
  {
  case SORT_METHOD_LABEL:
    FillSortFields(SSortFileItem::ByLabel);
    break;
  case SORT_METHOD_LABEL_IGNORE_THE:
    FillSortFields(SSortFileItem::ByLabelNoThe);
    break;
  case SORT_METHOD_DATE:
    FillSortFields(SSortFileItem::ByDate);
    break;
  case SORT_METHOD_SIZE:
    FillSortFields(SSortFileItem::BySize);
    break;
  case SORT_METHOD_DRIVE_TYPE:
    FillSortFields(SSortFileItem::ByDriveType);
    break;
  case SORT_METHOD_TRACKNUM:
    FillSortFields(SSortFileItem::BySongTrackNum);
    break;
  case SORT_METHOD_EPISODE:
    FillSortFields(SSortFileItem::ByEpisodeNum);
    break;
  case SORT_METHOD_DURATION:
    FillSortFields(SSortFileItem::BySongDuration);
    break;
  case SORT_METHOD_TITLE_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongTitleNoThe);
    break;
  case SORT_METHOD_TITLE:
    FillSortFields(SSortFileItem::BySongTitle);
    break;
  case SORT_METHOD_ARTIST:
    FillSortFields(SSortFileItem::BySongArtist);
    break;
  case SORT_METHOD_ARTIST_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongArtistNoThe);
    break;
  case SORT_METHOD_ALBUM:
    FillSortFields(SSortFileItem::BySongAlbum);
    break;
  case SORT_METHOD_ALBUM_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongAlbumNoThe);
    break;
  case SORT_METHOD_GENRE:
    FillSortFields(SSortFileItem::ByGenre);
    break;
  case SORT_METHOD_FILE:
    FillSortFields(SSortFileItem::ByFile);
    break;
  case SORT_METHOD_VIDEO_RATING:
    FillSortFields(SSortFileItem::ByMovieRating);
    break;
  case SORT_METHOD_VIDEO_TITLE:
    FillSortFields(SSortFileItem::ByMovieTitle);
    break;
  case SORT_METHOD_YEAR:
    FillSortFields(SSortFileItem::ByYear);
    break;
  case SORT_METHOD_PRODUCTIONCODE:
    FillSortFields(SSortFileItem::ByProductionCode);
    break;
  case SORT_METHOD_PROGRAM_COUNT:
  case SORT_METHOD_PLAYLIST_ORDER:
    // TODO: Playlist order is hacked into program count variable (not nice, but ok until 2.0)
    FillSortFields(SSortFileItem::ByProgramCount);
    break;
  case SORT_METHOD_SONG_RATING:
    FillSortFields(SSortFileItem::BySongRating);
    break;
  case SORT_METHOD_MPAA_RATING:
    FillSortFields(SSortFileItem::ByMPAARating);
    break;
  case SORT_METHOD_VIDEO_RUNTIME:
    FillSortFields(SSortFileItem::ByMovieRuntime);
    break;
  case SORT_METHOD_STUDIO:
    FillSortFields(SSortFileItem::ByStudio);
    break;
  case SORT_METHOD_STUDIO_IGNORE_THE:
    FillSortFields(SSortFileItem::ByStudioNoThe);
    break;
  case SORT_METHOD_FULLPATH:
    FillSortFields(SSortFileItem::ByFullPath);
    break;
  default:
    break;
  }
  if (sortMethod == SORT_METHOD_FILE)
    Sort(sortOrder==SORT_ORDER_ASC ? SSortFileItem::IgnoreFoldersAscending : SSortFileItem::IgnoreFoldersDescending);
  else if (sortMethod != SORT_METHOD_NONE)
    Sort(sortOrder==SORT_ORDER_ASC ? SSortFileItem::Ascending : SSortFileItem::Descending);

  m_sortMethod=sortMethod;
  m_sortOrder=sortOrder;
}

void CFileItemList::Randomize()
{
  CSingleLock lock(m_lock);
  random_shuffle(m_items.begin(), m_items.end());
}

void CFileItemList::Serialize(CArchive& ar)
{
  CSingleLock lock(m_lock);
  if (ar.IsStoring())
  {
    CFileItem::Serialize(ar);

    int i = 0;
    if (m_items.size() > 0 && m_items[0]->IsParentFolder())
      i = 1;

    ar << (int)(m_items.size() - i);

    ar << m_fastLookup;

    ar << (int)m_sortMethod;
    ar << (int)m_sortOrder;
    ar << (int)m_cacheToDisc;

    ar << (int)m_sortDetails.size();
    for (unsigned int j = 0; j < m_sortDetails.size(); ++j)
    {
      const SORT_METHOD_DETAILS &details = m_sortDetails[j];
      ar << (int)details.m_sortMethod;
      ar << details.m_buttonLabel;
      ar << details.m_labelMasks.m_strLabelFile;
      ar << details.m_labelMasks.m_strLabelFolder;
      ar << details.m_labelMasks.m_strLabel2File;
      ar << details.m_labelMasks.m_strLabel2Folder;
    }

    ar << m_content;

    for (; i < (int)m_items.size(); ++i)
    {
      CFileItemPtr pItem = m_items[i];
      ar << *pItem;
    }
  }
  else
  {
    CFileItemPtr pParent;
    if (!IsEmpty())
    {
      CFileItemPtr pItem=m_items[0];
      if (pItem->IsParentFolder())
        pParent.reset(new CFileItem(*pItem));
    }

    SetFastLookup(false);
    Clear();


    CFileItem::Serialize(ar);

    int iSize = 0;
    ar >> iSize;
    if (iSize <= 0)
      return ;

    if (pParent)
    {
      m_items.reserve(iSize + 1);
      m_items.push_back(pParent);
    }
    else
      m_items.reserve(iSize);

    bool fastLookup=false;
    ar >> fastLookup;

    ar >> (int&)m_sortMethod;
    ar >> (int&)m_sortOrder;
    ar >> (int&)m_cacheToDisc;

    unsigned int detailSize = 0;
    ar >> detailSize;
    for (unsigned int j = 0; j < detailSize; ++j)
    {
      SORT_METHOD_DETAILS details;
      ar >> (int&)details.m_sortMethod;
      ar >> details.m_buttonLabel;
      ar >> details.m_labelMasks.m_strLabelFile;
      ar >> details.m_labelMasks.m_strLabelFolder;
      ar >> details.m_labelMasks.m_strLabel2File;
      ar >> details.m_labelMasks.m_strLabel2Folder;
      m_sortDetails.push_back(details);
    }

    ar >> m_content;

    for (int i = 0; i < iSize; ++i)
    {
      CFileItemPtr pItem(new CFileItem);
      ar >> *pItem;
      Add(pItem);
    }

    SetFastLookup(fastLookup);
  }
}

void CFileItemList::FillInDefaultIcons()
{
  CSingleLock lock(m_lock);
  for (int i = 0; i < (int)m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->FillInDefaultIcon();
  }
}

void CFileItemList::SetMusicThumbs()
{
  CSingleLock lock(m_lock);
  //cache thumbnails directory
  g_directoryCache.InitMusicThumbCache();

  for (int i = 0; i < (int)m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetMusicThumb();
  }

  g_directoryCache.ClearMusicThumbCache();
}

int CFileItemList::GetFolderCount() const
{
  CSingleLock lock(m_lock);
  int nFolderCount = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_bIsFolder)
      nFolderCount++;
  }

  return nFolderCount;
}

int CFileItemList::GetObjectCount() const
{
  CSingleLock lock(m_lock);

  int numObjects = (int)m_items.size();
  if (numObjects && m_items[0]->IsParentFolder())
    numObjects--;

  return numObjects;
}

int CFileItemList::GetFileCount() const
{
  CSingleLock lock(m_lock);
  int nFileCount = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (!pItem->m_bIsFolder)
      nFileCount++;
  }

  return nFileCount;
}

int CFileItemList::GetSelectedCount() const
{
  CSingleLock lock(m_lock);
  int count = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->IsSelected())
      count++;
  }

  return count;
}

void CFileItemList::FilterCueItems()
{
  CSingleLock lock(m_lock);
  // Handle .CUE sheet files...
  VECSONGS itemstoadd;
  CStdStringArray itemstodelete;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (!pItem->m_bIsFolder)
    { // see if it's a .CUE sheet
      if (pItem->IsCUESheet())
      {
        CCueDocument cuesheet;
        if (cuesheet.Parse(pItem->m_strPath))
        {
          VECSONGS newitems;
          cuesheet.GetSongs(newitems);

          std::vector<CStdString> MediaFileVec;
          cuesheet.GetMediaFiles(MediaFileVec);

          // queue the cue sheet and the underlying media file for deletion
          for(std::vector<CStdString>::iterator itMedia = MediaFileVec.begin(); itMedia != MediaFileVec.end(); itMedia++)
          {
            CStdString strMediaFile = *itMedia;
            CStdString fileFromCue = strMediaFile; // save the file from the cue we're matching against,
                                                   // as we're going to search for others here...
            bool bFoundMediaFile = CFile::Exists(strMediaFile);
            // queue the cue sheet and the underlying media file for deletion
            if (!bFoundMediaFile)
            {
              // try file in same dir, not matching case...
              if (Contains(strMediaFile))
              {
                bFoundMediaFile = true;
              }
              else
              {
                // try removing the .cue extension...
                strMediaFile = pItem->m_strPath;
                CUtil::RemoveExtension(strMediaFile);
                CFileItem item(strMediaFile, false);
                if (item.IsAudio() && Contains(strMediaFile))
                {
                  bFoundMediaFile = true;
                }
                else
                { // try replacing the extension with one of our allowed ones.
                  CStdStringArray extensions;
                  StringUtils::SplitString(g_stSettings.m_musicExtensions, "|", extensions);
                  for (unsigned int i = 0; i < extensions.size(); i++)
                  {
                    CUtil::ReplaceExtension(pItem->m_strPath, extensions[i], strMediaFile);
                    CFileItem item(strMediaFile, false);
                    if (!item.IsCUESheet() && !item.IsPlayList() && Contains(strMediaFile))
                    {
                      bFoundMediaFile = true;
                      break;
                    }
                  }
                }
              }
            }
            if (bFoundMediaFile)
            {
              itemstodelete.push_back(pItem->m_strPath);
              itemstodelete.push_back(strMediaFile);
              // get the additional stuff (year, genre etc.) from the underlying media files tag.
              CMusicInfoTag tag;
              auto_ptr<IMusicInfoTagLoader> pLoader (CMusicInfoTagLoaderFactory::CreateLoader(strMediaFile));
              if (NULL != pLoader.get())
              {
                // get id3tag
                pLoader->Load(strMediaFile, tag);
              }
              // fill in any missing entries from underlying media file
              for (int j = 0; j < (int)newitems.size(); j++)
              {
                CSong song = newitems[j];
                // only for songs that actually match the current media file
                if (song.strFileName == fileFromCue)
                {
                  // we might have a new media file from the above matching code
                  song.strFileName = strMediaFile;
                  if (tag.Loaded())
                  {
                    if (song.strAlbum.empty() && !tag.GetAlbum().empty()) song.strAlbum = tag.GetAlbum();
                    if (song.strGenre.empty() && !tag.GetGenre().empty()) song.strGenre = tag.GetGenre();
                    if (song.strArtist.empty() && !tag.GetArtist().empty()) song.strArtist = tag.GetArtist();
                    SYSTEMTIME dateTime;
                    tag.GetReleaseDate(dateTime);
                    if (dateTime.wYear) song.iYear = dateTime.wYear;
                  }
                  if (!song.iDuration && tag.GetDuration() > 0)
                  { // must be the last song
                    song.iDuration = (tag.GetDuration() * 75 - song.iStartOffset + 37) / 75;
                  }
                  // add this item to the list
                  itemstoadd.push_back(song);
                }
              }
            }
            else
            { // remove the .cue sheet from the directory
              itemstodelete.push_back(pItem->m_strPath);
            }
          }
        }
        else
        { // remove the .cue sheet from the directory (can't parse it - no point listing it)
          itemstodelete.push_back(pItem->m_strPath);
        }
      }
    }
  }
  // now delete the .CUE files and underlying media files.
  for (int i = 0; i < (int)itemstodelete.size(); i++)
  {
    for (int j = 0; j < (int)m_items.size(); j++)
    {
      CFileItemPtr pItem = m_items[j];
      if (stricmp(pItem->m_strPath.c_str(), itemstodelete[i].c_str()) == 0)
      { // delete this item
        m_items.erase(m_items.begin() + j);
        break;
      }
    }
  }
  // and add the files from the .CUE sheet
  for (int i = 0; i < (int)itemstoadd.size(); i++)
  {
    // now create the file item, and add to the item list.
    CFileItemPtr pItem(new CFileItem(itemstoadd[i]));
    m_items.push_back(pItem);
  }
}

// Remove the extensions from the filenames
void CFileItemList::RemoveExtensions()
{
  CSingleLock lock(m_lock);
  for (int i = 0; i < Size(); ++i)
    m_items[i]->RemoveExtension();
}

void CFileItemList::Stack()
{
  CSingleLock lock(m_lock);

  // not allowed here
  if (IsVirtualDirectoryRoot() || IsTV())
    return;

  // items needs to be sorted for stuff below to work properly
  Sort(SORT_METHOD_LABEL, SORT_ORDER_ASC);

  // stack folders
  bool isDVDFolder(false);
  int i = 0;
  for (i = 0; i < Size(); ++i)
  {
    CFileItemPtr item = Get(i);
    if (item->GetLabel().Equals("VIDEO_TS.IFO"))
    {
      isDVDFolder = true;
      break;
    }
    // combined the folder checks
    if (item->m_bIsFolder)
    {
      // only check known fast sources?
      // xbms included because it supports file existance
      // NOTES:
      // 1. xbms would not have worked previously: item->m_strPath.Left(5).Equals("xbms", false)
      // 2. rars and zips may be on slow sources? is this supposed to be allowed?
      if( !item->IsRemote()
        || item->IsSmb()
        || item->m_strPath.Left(7).Equals("xbms://")
        || CUtil::IsInRAR(item->m_strPath)
        || CUtil::IsInZIP(item->m_strPath)
        )
      {
        // stack cd# folders if contains only a single video file
        // NOTE: if we're doing this anyway, why not collapse *all* folders with just a single video file?
        CStdString folderName = item->GetLabel();
        if (folderName.Left(2).Equals("CD") && StringUtils::IsNaturalNumber(folderName.Mid(2)))
        {
          CFileItemList items;
          CDirectory::GetDirectory(item->m_strPath,items,g_stSettings.m_videoExtensions,true);
          // optimized to only traverse listing once by checking for filecount
          // and recording last file item for later use
          int nFiles = 0;
          int index = -1;
          for (int j = 0; j < items.Size(); j++)
          {
            if (!items[j]->m_bIsFolder)
            {
              nFiles++;
              index = j;
            }
            if (nFiles > 1)
              break;
          }
          if (nFiles == 1)
          {
            *item = *items[index];
          }
        }

        // check for dvd folders
        else
        {
          CStdString path;
          CStdString dvdPath;
          CUtil::AddFileToFolder(item->m_strPath, "VIDEO_TS.IFO", path);
          if (CFile::Exists(path))
            dvdPath = path;
          else
          {
            CUtil::AddFileToFolder(item->m_strPath, "VIDEO_TS", dvdPath);
            CUtil::AddFileToFolder(dvdPath, "VIDEO_TS.IFO", path);
            dvdPath.Empty();
            if (CFile::Exists(path))
              dvdPath = path;
          }
          if (!dvdPath.IsEmpty())
          {
            // NOTE: should this be done for the CD# folders too?
            /* set the thumbnail based on folder */
            item->SetCachedVideoThumb();
            if (!item->HasThumbnail())
              item->SetUserVideoThumb();

            item->m_bIsFolder = false;
            item->m_strPath = dvdPath;
            item->SetLabel2("");
            item->SetLabelPreformated(true);
            m_sortMethod = SORT_METHOD_NONE; /* sorting is now broken */

            /* override the previously set thumb if video_ts.ifo has any */
            /* otherwise user can't set icon on the stacked file as that */
            /* will allways be set on the video_ts.ifo file */
            CStdString thumb(item->GetCachedVideoThumb());
            if(CFile::Exists(thumb))
              item->SetThumbnailImage(thumb);
            else
              item->SetUserVideoThumb();
          }
        }
      }
    }
  }


  // now stack the files, some of which may be from the previous stack iteration
  i = 0;
  while (i < Size())
  {
    CFileItemPtr item = Get(i);

    // skip folders, nfo files, playlists, dvd images
    if (item->m_bIsFolder
      || item->IsParentFolder()
      || item->IsNFO()
      || item->IsPlayList()
      || item->IsDVDImage()
      )
    {
      // increment index
      i++;
      continue;
    }

    if (isDVDFolder)
    {
      // remove any other ifo files in this folder
      if (item->IsDVDFile(false, true) && !item->GetLabel().Equals("VIDEO_TS.IFO"))
      {
        Remove(i);
        continue;
      }
    }

    CStdString fileName, filePath;
    CUtil::Split(item->m_strPath, filePath, fileName);
    CStdString fileTitle, volumeNumber;
    // hmmm... should this use GetLabel() or fileName?
    if (CUtil::GetVolumeFromFileName(item->GetLabel(), fileTitle, volumeNumber))
    {
      vector<int> stack;
      stack.push_back(i);
      __int64 size = item->m_dwSize;

      int j = i + 1;
      while (j < Size())
      {
        CFileItemPtr item2 = Get(j);
        CStdString fileName2, filePath2;
        CUtil::Split(item2->m_strPath, filePath2, fileName2);
        // only do a stacking comparison if the first letter of the filename is the same
        if (fileName2.size() && fileName2.at(0) != fileName.at(0))
          break;

        CStdString fileTitle2, volumeNumber2;
        // hmmm... should this use GetLabel() or fileName2?
        if (CUtil::GetVolumeFromFileName(item2->GetLabel(), fileTitle2, volumeNumber2))
        {
          if (fileTitle2.Equals(fileTitle))
          {
            stack.push_back(j);
            size += item2->m_dwSize;
          }
        }

        // increment index
        j++;
      }

      if (stack.size() > 1)
      {
        // have a stack, remove the items and add the stacked item
        CStackDirectory dir;
        // dont actually stack a multipart rar set, just remove all items but the first
        CStdString stackPath;
        if (Get(stack[0])->IsRAR())
          stackPath = Get(stack[0])->m_strPath;
        else
          stackPath = dir.ConstructStackPath(*this, stack);
        item->m_strPath = stackPath;
        // clean up list
        for (unsigned int k = stack.size() - 1; k > 0; --k)
        {
          Remove(stack[k]);
        }
        // item->m_bIsFolder = true;  // don't treat stacked files as folders
        // the label may be in a different char set from the filename (eg over smb
        // the label is converted from utf8, but the filename is not)
        CUtil::GetVolumeFromFileName(item->GetLabel(), fileTitle, volumeNumber);
        if (g_guiSettings.GetBool("filelists.hideextensions"))
          CUtil::RemoveExtension(fileTitle);
        item->SetLabel(fileTitle);
        item->m_dwSize = size;
      }
    }

    // increment index
    i++;
  }
}

bool CFileItemList::Load()
{
  CFile file;
  if (file.Open(GetDiscCacheFile()))
  {
    CLog::Log(LOGDEBUG,"Loading fileitems [%s]",m_strPath.c_str());
    CArchive ar(&file, CArchive::load);
    ar >> *this;
    CLog::Log(LOGDEBUG,"  -- items: %i, directory: %s sort method: %i, ascending: %s",Size(),m_strPath.c_str(), m_sortMethod, m_sortOrder ? "true" : "false");
    ar.Close();
    file.Close();
    return true;
  }

  return false;
}

bool CFileItemList::Save()
{
  int iSize = Size();
  if (iSize <= 0)
    return false;

  CLog::Log(LOGDEBUG,"Saving fileitems [%s]",m_strPath.c_str());

  CFile file;
  if (file.OpenForWrite(GetDiscCacheFile(), true)) // overwrite always
  {
    CArchive ar(&file, CArchive::store);
    ar << *this;
    CLog::Log(LOGDEBUG,"  -- items: %i, sort method: %i, ascending: %s",iSize,m_sortMethod, m_sortOrder ? "true" : "false");
    ar.Close();
    file.Close();
    return true;
  }

  return false;
}

void CFileItemList::RemoveDiscCache() const
{
  CLog::Log(LOGDEBUG,"Clearing cached fileitems [%s]",m_strPath.c_str());
  if (CFile::Exists(GetDiscCacheFile()))
    CFile::Delete(GetDiscCacheFile());
}

CStdString CFileItemList::GetDiscCacheFile() const
{
  CStdString strPath=m_strPath;
  CUtil::RemoveSlashAtEnd(strPath);

  Crc32 crc;
  crc.ComputeFromLowerCase(strPath);

  CStdString cacheFile;
  if (IsCDDA() || IsOnDVD())
    cacheFile.Format("special://temp/r-%08x.fi", (unsigned __int32)crc);
  else if (IsMusicDb())
    cacheFile.Format("special://temp/mdb-%08x.fi", (unsigned __int32)crc);
  else if (IsVideoDb())
    cacheFile.Format("special://temp/vdb-%08x.fi", (unsigned __int32)crc);
  else
    cacheFile.Format("special://temp/%08x.fi", (unsigned __int32)crc);
  return cacheFile;
}

bool CFileItemList::AlwaysCache() const
{
  // some database folders are always cached
  if (IsMusicDb())
    return CMusicDatabaseDirectory::CanCache(m_strPath);
  if (IsVideoDb())
    return CVideoDatabaseDirectory::CanCache(m_strPath);
  return false;
}

void CFileItemList::SetCachedVideoThumbs()
{
  CSingleLock lock(m_lock);
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedVideoThumb();
  }
}

void CFileItemList::SetCachedProgramThumbs()
{
  CSingleLock lock(m_lock);
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedProgramThumb();
  }
}

void CFileItemList::SetCachedMusicThumbs()
{
  CSingleLock lock(m_lock);
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedMusicThumb();
  }
}

CStdString CFileItem::GetCachedPictureThumb() const
{
  return GetCachedThumb(m_strPath,g_settings.GetPicturesThumbFolder(),true);
}

void CFileItem::SetCachedMusicThumb()
{
  // if it already has a thumbnail, then return
  if (HasThumbnail() || m_bIsShareOrDrive) return ;

  // streams do not have thumbnails
  if (IsInternetStream()) return ;

  //  music db items already have thumbs or there is no thumb available
  if (IsMusicDb()) return;

  // ignore the parent dir items
  if (IsParentFolder()) return;

  CStdString cachedThumb(GetPreviouslyCachedMusicThumb());
  if (!cachedThumb.IsEmpty())
    SetThumbnailImage(cachedThumb);
    // SetIconImage(cachedThumb);
}

CStdString CFileItem::GetPreviouslyCachedMusicThumb() const
{
  // look if an album thumb is available,
  // could be any file with tags loaded or
  // a directory in album window
  CStdString strAlbum, strArtist;
  if (HasMusicInfoTag() && m_musicInfoTag->Loaded())
  {
    strAlbum = m_musicInfoTag->GetAlbum();
    if (!m_musicInfoTag->GetAlbumArtist().IsEmpty())
      strArtist = m_musicInfoTag->GetAlbumArtist();
    else
      strArtist = m_musicInfoTag->GetArtist();
  }
  if (!strAlbum.IsEmpty() && !strArtist.IsEmpty())
  {
    // try permanent album thumb using "album name + artist name"
    CStdString thumb(CUtil::GetCachedAlbumThumb(strAlbum, strArtist));
    if (CFile::Exists(thumb))
      return thumb;
  }

  // if a file, try to find a cached filename.tbn
  if (!m_bIsFolder)
  {
    // look for locally cached tbn
    CStdString thumb(CUtil::GetCachedMusicThumb(m_strPath));
    if (CFile::Exists(thumb))
      return thumb;
  }

  // try and find a cached folder thumb (folder.jpg or folder.tbn)
  CStdString strPath;
  if (!m_bIsFolder)
    CUtil::GetDirectory(m_strPath, strPath);
  else
    strPath = m_strPath;
  // music thumbs are cached without slash at end
  CUtil::RemoveSlashAtEnd(strPath);

  CStdString thumb(CUtil::GetCachedMusicThumb(strPath));
  if (CFile::Exists(thumb))
    return thumb;

  return "";
}

CStdString CFileItem::GetUserMusicThumb(bool alwaysCheckRemote /* = false */) const
{
  if (m_strPath.IsEmpty() || m_bIsShareOrDrive || IsInternetStream() || CUtil::IsUPnP(m_strPath) || IsParentFolder() || IsMusicDb())
    return "";

  // we first check for <filename>.tbn or <foldername>.tbn
  CStdString fileThumb(GetTBNFile());
  if (CFile::Exists(fileThumb))
    return fileThumb;
  // if a folder, check for folder.jpg
  if (m_bIsFolder && (!IsRemote() || alwaysCheckRemote || g_guiSettings.GetBool("musicfiles.findremotethumbs")))
  {
    CStdStringArray thumbs;
    StringUtils::SplitString(g_advancedSettings.m_musicThumbs, "|", thumbs);
    for (unsigned int i = 0; i < thumbs.size(); ++i)
    {
      CStdString folderThumb(GetFolderThumb(thumbs[i]));
      if (CFile::Exists(folderThumb))
      {
        return folderThumb;
      }
    }
  }
  // this adds support for files which inherit a folder.jpg icon which has not been cached yet.
  // this occurs when queueing a top-level folder which has not been traversed yet.
  else if (!IsRemote() || alwaysCheckRemote || g_guiSettings.GetBool("musicfiles.findremotethumbs"))
  {
    CStdString strFolder, strFile;
    CUtil::Split(m_strPath, strFolder, strFile);
    CFileItem folderItem(strFolder, true);
    folderItem.SetMusicThumb(alwaysCheckRemote);
    if (folderItem.HasThumbnail())
      return folderItem.GetThumbnailImage();
  }
  // No thumb found
  return "";
}

void CFileItem::SetUserMusicThumb(bool alwaysCheckRemote /* = false */)
{
  // caches as the local thumb
  CStdString thumb(GetUserMusicThumb(alwaysCheckRemote));
  if (!thumb.IsEmpty())
  {
    CStdString cachedThumb(CUtil::GetCachedMusicThumb(m_strPath));
    CPicture pic;
    pic.DoCreateThumbnail(thumb, cachedThumb);
  }

  SetCachedMusicThumb();
}

void CFileItem::SetCachedPictureThumb()
{
  if (IsParentFolder()) return;
  CStdString cachedThumb(GetCachedPictureThumb());
  if (CFile::Exists(cachedThumb))
    SetThumbnailImage(cachedThumb);
}

CStdString CFileItem::GetCachedVideoThumb() const
{
  if (IsStack())
    return GetCachedThumb(CStackDirectory::GetFirstStackedFile(m_strPath),g_settings.GetVideoThumbFolder(),true);
  else
    return GetCachedThumb(m_strPath,g_settings.GetVideoThumbFolder(),true);
}

CStdString CFileItem::GetCachedEpisodeThumb() const
{
  // get the locally cached thumb
  CStdString strCRC;
  strCRC.Format("%sepisode%i",GetVideoInfoTag()->m_strFileNameAndPath.c_str(),GetVideoInfoTag()->m_iEpisode);
  return GetCachedThumb(strCRC,g_settings.GetVideoThumbFolder(),true);
}

void CFileItem::SetCachedVideoThumb()
{
  if (IsParentFolder()) return;
  CStdString cachedThumb(GetCachedVideoThumb());
  if (CFile::Exists(cachedThumb))
    SetThumbnailImage(cachedThumb);
}

// Gets the .tbn filename from a file or folder name.
// <filename>.ext -> <filename>.tbn
// <foldername>/ -> <foldername>.tbn
CStdString CFileItem::GetTBNFile() const
{
  CStdString thumbFile;
  CStdString strFile = m_strPath;

  if (IsStack())
  {
    CStdString strPath, strReturn;
    CUtil::GetParentPath(m_strPath,strPath);
    CFileItem item(CStackDirectory::GetFirstStackedFile(strFile),false);
    CStdString strTBNFile = item.GetTBNFile();
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strReturn);
    if (CFile::Exists(strReturn))
      return strReturn;

    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(CStackDirectory::GetStackedTitlePath(strFile)),strFile);
  }

  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  if (m_bIsFolder && !IsFileFolder())
  {
    CURL url(strFile);

    // Don't try to get "foldername".tbn for empty filenames
    if (!url.GetFileName().IsEmpty())
    {
      CUtil::RemoveSlashAtEnd(strFile);
      thumbFile = strFile + ".tbn";
    }
  }
  else
  {
    CUtil::ReplaceExtension(strFile, ".tbn", thumbFile);
  }
  return thumbFile;
}

CStdString CFileItem::GetUserVideoThumb() const
{
  if (m_strPath.IsEmpty() || m_bIsShareOrDrive || IsInternetStream() || CUtil::IsUPnP(m_strPath) || IsParentFolder() || IsVTP())
    return "";

  if (IsTuxBox())
  {
    if (!m_bIsFolder)
      return g_tuxbox.GetPicon(GetLabel());
    else return "";
  }

  // 1. check <filename>.tbn or <foldername>.tbn
  CStdString fileThumb(GetTBNFile());
  if (CFile::Exists(fileThumb))
    return fileThumb;

  // 2. - check movie.tbn, as long as it's not a folder
  if (!m_bIsFolder)
  {
    CStdString strPath, movietbnFile;
    CUtil::GetParentPath(m_strPath, strPath);
    CUtil::AddFileToFolder(strPath, "movie.tbn", movietbnFile);
    if (CFile::Exists(movietbnFile))
      return movietbnFile;
  }

  // 3. check folder image in_m_dvdThumbs (folder.jpg)
  if (m_bIsFolder)
  {
    CStdStringArray thumbs;
    StringUtils::SplitString(g_advancedSettings.m_dvdThumbs, "|", thumbs);
    for (unsigned int i = 0; i < thumbs.size(); ++i)
    {
      CStdString folderThumb(GetFolderThumb(thumbs[i]));
      if (CFile::Exists(folderThumb))
      {
        return folderThumb;
      }
    }
  }
  // No thumb found
  return "";
}

CStdString CFileItem::GetFolderThumb(const CStdString &folderJPG /* = "folder.jpg" */) const
{
  CStdString folderThumb;
  CStdString strFolder = m_strPath;

  if (IsStack())
  {
    CStdString strPath;
    CUtil::GetParentPath(m_strPath,strPath);
    CStdString strFolder = CStackDirectory::GetStackedTitlePath(m_strPath);
  }

  if (CUtil::IsInRAR(strFolder) || CUtil::IsInZIP(strFolder))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFolder,strPath);
    CUtil::GetParentPath(strPath,strParent);
  }

  if (IsMultiPath())
    strFolder = CMultiPathDirectory::GetFirstPath(m_strPath);

  CUtil::AddFileToFolder(strFolder, folderJPG, folderThumb);
  return folderThumb;
}

void CFileItem::SetVideoThumb()
{
  if (HasThumbnail()) return;
  SetCachedVideoThumb();
  if (!HasThumbnail())
    SetUserVideoThumb();
}

void CFileItem::SetUserVideoThumb()
{
  if (m_bIsShareOrDrive) return;
  if (IsParentFolder()) return;

  // caches as the local thumb
  CStdString thumb(GetUserVideoThumb());
  if (!thumb.IsEmpty())
  {
    CStdString cachedThumb(GetCachedVideoThumb());
    CPicture pic;
    pic.DoCreateThumbnail(thumb, cachedThumb);
  }
  SetCachedVideoThumb();
}

///
/// If a cached fanart image already exists, then we're fine.  Otherwise, we look for a local fanart.jpg
/// and cache that image as our fanart.
CStdString CFileItem::CacheFanart(bool probe) const
{
  if (IsVideoDb())
  {
    if (!HasVideoInfoTag())
      return ""; // nothing can be done
    CFileItem dbItem(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath, m_bIsFolder);
    return dbItem.CacheFanart();
  }

  CStdString cachedFanart(GetCachedFanart());
  if (!probe)
  {
    // first check for an already cached fanart image
    if (CFile::Exists(cachedFanart))
      return "";
  }

  CStdString strFile2;
  CStdString strFile = m_strPath;
  if (IsStack())
  {
    CStdString strPath;
    CUtil::GetParentPath(m_strPath,strPath);
    CStackDirectory dir;
    CStdString strPath2;
    strPath2 = dir.GetStackedTitlePath(strFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strPath2),strFile);
    CFileItem item(dir.GetFirstStackedFile(m_strPath),false);
    CStdString strTBNFile = item.GetTBNFile();
    CUtil::ReplaceExtension(strTBNFile, "-fanart",strTBNFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strFile2);
  }
  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  // no local fanart available for these
  if (IsInternetStream() || CUtil::IsUPnP(strFile) || IsTV() || IsPlugin())
    return "";

  // we don't have a cached image, so let's see if the user has a local image ..
  CStdString strDir;
  CUtil::GetDirectory(strFile, strDir);
  if (strDir.IsEmpty()) return "";

  bool bFoundFanart = false;
  CStdString localFanart;
  CFileItemList items;
  CDirectory::GetDirectory(strDir, items, g_stSettings.m_pictureExtensions, false, false, DIR_CACHE_ALWAYS, false);
  CUtil::RemoveExtension(strFile);
  strFile += "-fanart";
  CStdString strFile3 = CUtil::AddFileToFolder(strDir, "fanart");

  for (int i = 0; i < items.Size(); i++)
  {
    CStdString strCandidate = items[i]->m_strPath;
    CUtil::RemoveExtension(strCandidate);
    if (strCandidate.CompareNoCase(strFile) == 0 ||
        strCandidate.CompareNoCase(strFile2) == 0 ||
        strCandidate.CompareNoCase(strFile3) == 0)
    {
      bFoundFanart = true;
      localFanart = items[i]->m_strPath;
      break;
    }
  }

  // no local fanart found
  if(!bFoundFanart)
    return "";

  if (!probe)
  {
    CPicture pic;
    pic.CacheImage(localFanart, cachedFanart);
  }

  return localFanart;
}

CStdString CFileItem::GetCachedFanart() const
{
  // get the locally cached thumb
  if (IsVideoDb())
  {
    if (!HasVideoInfoTag())
      return "";
    if (!GetVideoInfoTag()->m_strArtist.IsEmpty())
      return GetCachedThumb(GetVideoInfoTag()->m_strArtist,g_settings.GetMusicFanartFolder());
    if (!m_bIsFolder && !GetVideoInfoTag()->m_strShowTitle.IsEmpty())
    {
      CVideoDatabase database;
      database.Open();
      int iShowId = database.GetTvShowId(GetVideoInfoTag()->m_strPath);
      CStdString showPath;
      database.GetFilePathById(iShowId,showPath,VIDEODB_CONTENT_TVSHOWS);
      return GetCachedThumb(showPath,g_settings.GetVideoFanartFolder()); 
    }
    return GetCachedThumb(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath,g_settings.GetVideoFanartFolder());
  }
  if (HasMusicInfoTag())
    return GetCachedThumb(GetMusicInfoTag()->GetArtist(),g_settings.GetMusicFanartFolder());

  return GetCachedThumb(m_strPath,g_settings.GetVideoFanartFolder());
}

CStdString CFileItem::GetCachedThumb(const CStdString &path, const CStdString &path2, bool split)
{
  // get the locally cached thumb
  Crc32 crc;
  crc.ComputeFromLowerCase(path);

  CStdString thumb;
  if (split)
  {
    CStdString hex;
    hex.Format("%08x", (__int32)crc);
    thumb.Format("%c\\%08x.tbn", hex[0], (unsigned __int32)crc);
  }
  else
    thumb.Format("%08x.tbn", (unsigned __int32)crc);

  return CUtil::AddFileToFolder(path2, thumb);
}

CStdString CFileItem::GetCachedProgramThumb() const
{
  return GetCachedThumb(m_strPath,g_settings.GetProgramsThumbFolder());
}

CStdString CFileItem::GetCachedGameSaveThumb() const
{
  return "";
}

void CFileItem::SetCachedProgramThumb()
{
  // don't set any thumb for programs on DVD, as they're bound to be named the
  // same (D:\default.xbe).
  if (IsParentFolder()) return;
  CStdString thumb(GetCachedProgramThumb());
  if (CFile::Exists(thumb))
    SetThumbnailImage(thumb);
}

void CFileItem::SetUserProgramThumb()
{
  if (m_bIsShareOrDrive) return;
  if (IsParentFolder()) return;

  if (IsShortCut())
  {
    CShortcut shortcut;
    if ( shortcut.Create( m_strPath ) )
    {
      // use the shortcut's thumb
      if (!shortcut.m_strThumb.IsEmpty())
        m_strThumbnailImage = shortcut.m_strThumb;
      else
      {
        CFileItem item(shortcut.m_strPath,false);
        item.SetUserProgramThumb();
        m_strThumbnailImage = item.m_strThumbnailImage;
      }
      return;
    }
  }
  // 1.  Try <filename>.tbn
  CStdString fileThumb(GetTBNFile());
  CStdString thumb(GetCachedProgramThumb());
  if (CFile::Exists(fileThumb))
  { // cache
    CPicture pic;
    if (pic.DoCreateThumbnail(fileThumb, thumb))
      SetThumbnailImage(thumb);
  }
  else if (m_bIsFolder)
  {
    // 3. cache the folder image
    CStdString folderThumb(GetFolderThumb());
    if (CFile::Exists(folderThumb))
    {
      CPicture pic;
      if (pic.DoCreateThumbnail(folderThumb, thumb))
        SetThumbnailImage(thumb);
    }
  }
}

/*void CFileItem::SetThumb()
{
  // we need to know the type of file at this point
  // as differing views have differing inheritance rules for thumbs:

  // Videos:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn
  //  * Thumbs are cached from here using file or folder path

  // Music:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn or the album/path cached thumb or inherit from the folder
  //  * Thumbs are cached from here using file or folder path

  // Programs:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn or the embedded xbe.  Shortcuts have the additional step of the <thumbnail> tag to check
  //  * Thumbs are cached from here using file or folder path

  // Pictures:
  // Folders use <foldername>/folder.jpg or <foldername>.tbn, or auto-generated from 4 images in the folder
  // Files use <filename>.tbn or a resized version of the picture
  //  * Thumbs are cached from here using file or folder path

}*/

void CFileItemList::SetProgramThumbs()
{
  // TODO: Is there a speed up if we cache the program thumbs first?
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->IsParentFolder())
      continue;
    pItem->SetCachedProgramThumb();
    if (!pItem->HasThumbnail())
      pItem->SetUserProgramThumb();
  }
}

bool CFileItem::LoadMusicTag()
{
  // not audio
  if (!IsAudio())
    return false;
  // already loaded?
  if (HasMusicInfoTag() && m_musicInfoTag->Loaded())
    return true;
  // check db
  CMusicDatabase musicDatabase;
  if (musicDatabase.Open())
  {
    CSong song;
    if (musicDatabase.GetSongByFileName(m_strPath, song))
    {
      GetMusicInfoTag()->SetSong(song);
      SetThumbnailImage(song.strThumb);
      return true;
    }
    musicDatabase.Close();
  }
  // load tag from file
  CLog::Log(LOGDEBUG, "%s: loading tag information for file: %s", __FUNCTION__, m_strPath.c_str());
  CMusicInfoTagLoaderFactory factory;
  auto_ptr<IMusicInfoTagLoader> pLoader (factory.CreateLoader(m_strPath));
  if (NULL != pLoader.get())
  {
    if (pLoader->Load(m_strPath, *GetMusicInfoTag()))
      return true;
  }
  // no tag - try some other things
  if (IsCDDA())
  {
    // we have the tracknumber...
    int iTrack = GetMusicInfoTag()->GetTrackNumber();
    if (iTrack >= 1)
    {
      CStdString strText = g_localizeStrings.Get(554); // "Track"
      if (strText.GetAt(strText.size() - 1) != ' ')
        strText += " ";
      CStdString strTrack;
      strTrack.Format(strText + "%i", iTrack);
      GetMusicInfoTag()->SetTitle(strTrack);
      GetMusicInfoTag()->SetLoaded(true);
      return true;
    }
  }
  else
  {
    CStdString fileName = CUtil::GetFileName(m_strPath);
    CUtil::RemoveExtension(fileName);
    for (unsigned int i = 0; i < g_advancedSettings.m_musicTagsFromFileFilters.size(); i++)
    {
      CLabelFormatter formatter(g_advancedSettings.m_musicTagsFromFileFilters[i], "");
      if (formatter.FillMusicTag(fileName, GetMusicInfoTag()))
      {
        GetMusicInfoTag()->SetLoaded(true);
        return true;
      }
    }
  }
  return false;
}

void CFileItem::SetCachedGameSavesThumb()
{
  if (IsParentFolder()) return;
  CStdString thumb(GetCachedGameSaveThumb());
  if (CFile::Exists(thumb))
    SetThumbnailImage(thumb);
}

void CFileItemList::SetCachedGameSavesThumbs()
{
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedGameSavesThumb();
  }
}

void CFileItemList::SetGameSavesThumbs()
{
  // No User thumbs
  // TODO: Is there a speed up if we cache the program thumbs first?
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->IsParentFolder())
      continue;
    pItem->SetCachedGameSavesThumb();  // was  pItem->SetCachedProgramThumb(); oringally
  }
}

void CFileItemList::Swap(unsigned int item1, unsigned int item2)
{
  if (item1 != item2 && item1 < m_items.size() && item2 < m_items.size())
    std::swap(m_items[item1], m_items[item2]);
}

void CFileItemList::UpdateItem(const CFileItem *item)
{
  if (!item) return;
  CFileItemPtr oldItem = Get(item->m_strPath);
  if (oldItem)
    *oldItem = *item;
}

void CFileItemList::AddSortMethod(SORT_METHOD sortMethod, int buttonLabel, const LABEL_MASKS &labelMasks)
{
  SORT_METHOD_DETAILS sort;
  sort.m_sortMethod=sortMethod;
  sort.m_buttonLabel=buttonLabel;
  sort.m_labelMasks=labelMasks;

  m_sortDetails.push_back(sort);
}

void CFileItemList::SetReplaceListing(bool replace)
{
  m_replaceListing = replace;
}

void CFileItemList::ClearSortState()
{
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
}

CVideoInfoTag* CFileItem::GetVideoInfoTag()
{
  if (!m_videoInfoTag)
    m_videoInfoTag = new CVideoInfoTag;

  return m_videoInfoTag;
}

CPictureInfoTag* CFileItem::GetPictureInfoTag()
{
  if (!m_pictureInfoTag)
    m_pictureInfoTag = new CPictureInfoTag;

  return m_pictureInfoTag;
}

MUSIC_INFO::CMusicInfoTag* CFileItem::GetMusicInfoTag()
{
  if (!m_musicInfoTag)
    m_musicInfoTag = new MUSIC_INFO::CMusicInfoTag;

  return m_musicInfoTag;
}

CStdString CFileItem::FindTrailer() const
{
  CStdString strFile2, strTrailer;
  CStdString strFile = m_strPath;
  if (IsStack())
  {
    CStdString strPath;
    CUtil::GetParentPath(m_strPath,strPath);
    CStackDirectory dir;
    CStdString strPath2;
    strPath2 = dir.GetStackedTitlePath(strFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strPath2),strFile);
    CFileItem item(dir.GetFirstStackedFile(m_strPath),false);
    CStdString strTBNFile = item.GetTBNFile();
    CUtil::ReplaceExtension(strTBNFile, "-trailer",strTBNFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strFile2);
  }
  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  // no local trailer available for these
  if (IsInternetStream() || CUtil::IsUPnP(strFile) || IsTV() || IsPlugin())
    return strTrailer;

  CStdString strDir;
  CUtil::GetDirectory(strFile, strDir);
  CFileItemList items;
  CDirectory::GetDirectory(strDir, items, g_stSettings.m_videoExtensions, true, false, DIR_CACHE_ALWAYS, false);
  CUtil::RemoveExtension(strFile);
  strFile += "-trailer";
  CStdString strFile3 = CUtil::AddFileToFolder(strDir, "movie-trailer");

  for (int i = 0; i < items.Size(); i++)
  {
    CStdString strCandidate = items[i]->m_strPath;
    CUtil::RemoveExtension(strCandidate);
    if (strCandidate.CompareNoCase(strFile) == 0 ||
        strCandidate.CompareNoCase(strFile2) == 0 ||
        strCandidate.CompareNoCase(strFile3) == 0)
    {
      strTrailer = items[i]->m_strPath;
      break;
    }
  }

  return strTrailer;
}


