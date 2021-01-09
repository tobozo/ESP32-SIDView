/*\

  ESP32-SIDView
  https://github.com/tobozo/ESP32-SIDView

  MIT License

  Copyright (c) 2020 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

\*/

#ifndef _SID_CACHE_MANAGER_H_
#define _SID_CACHE_MANAGER_H_


#define SID_PLAYER
#define SID_INSTRUMENTS
#include <SID6581.h> // https://github.com/hpwit/SID6581


// item types for the SID Player
typedef enum
{
  F_PARENT_FOLDER,     // parent folder
  F_PLAYLIST,          // SID file list for current folder
  F_SUBFOLDER,         // subfolder with SID file list
  F_SUBFOLDER_NOCACHE, // subfolder with no SID file list
  F_SID_FILE,          // single SID file
  F_TOOL_CB            // virtual file representing a program/callback
} folderItemType_t;


typedef bool (*activityTicker)( const char* title, size_t totalitems ); // callback for i/o activity during folder enumeration
typedef void (*folderElementPrintCb)( const char* fullpath, folderItemType_t type );


static void debugFolderElement( const char* fullpath, folderItemType_t type )
{
  String basepath = gnu_basename( fullpath );
  switch( type )
  {
    case F_PLAYLIST:          log_d("Cache Found :%s", basepath.c_str() ); break;
    case F_SUBFOLDER:         log_d("Subfolder Cache Found :%s", basepath.c_str() ); break;
    case F_SUBFOLDER_NOCACHE: log_d("Subfolder Found :%s", basepath.c_str() ); break;
    case F_PARENT_FOLDER:     log_d("Parent folder"); break;
    case F_SID_FILE:          log_d("SID File"); break;
    case F_TOOL_CB:           log_d("Tool"); break;
  }
}


static bool debugFolderTicker( const char* title, size_t totalitems )
{
  // not really a progress function but can show
  // some "activity" during slow folder scans
  log_d("[%d] %s", totalitems, title);
  return true; // continue signal, send false to stop and break the scan loop
}




class SongCacheManager
{

  public:

    ~SongCacheManager();
    SongCacheManager( SIDTunesPlayer *_sidPlayer );

    void init( const char* path );

    bool folderHasPlaylist( const char* fname );
    bool folderHasCache( const char* fname );

    bool getInfoFromCacheItem( size_t itemNum );
    bool getInfoFromCacheItem( const char* _cachepath, size_t itemNum );
    bool getInfoFromSIDFile( const char * path);

    bool scanPaginatedFolderCache( const char* pathname, size_t offset, size_t maxitems, folderElementPrintCb printCb, activityTicker tickerCb );
    bool scanFolderCache( const char* pathname, folderElementPrintCb printCb, activityTicker tickerCb );

    bool scanPaginatedFolder( const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTicker tickerCb = &debugFolderTicker, size_t offset = 0, size_t maxitems = 0 );
    bool scanFolder( const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTicker tickerCb = &debugFolderTicker, size_t maxitems = 0, bool force_regen = false );

    bool loadCache( const char* _cachepath );
    size_t getDirCacheSize( const char* pathname );

    void deleteCache( const char* pathname );

    size_t CacheItemNum = 0;
    size_t CacheItemSize = 0;
    char   *cachepath;

  private:

    fs::FS         *fs;
    fs::File       songCacheFile;

    SIDTunesPlayer *sidPlayer;

    char   *basename;
    char   *basepath;

    char   *md5path;
    String cleanPath;
    size_t maxItemsPerFolder = 1024; // hard limit for this app

    bool exists( SID_Meta_t *song );
    bool insert( SID_Meta_t *song, bool append = false );

    void         makeCachePath( const char* _cachepath, char* cachepath );
    const String makeCachePath( const char* path, const char* cachename );
    const String playListCachePath( const char * path );
    const String dirCachePath( const char * path );

    bool   dirCacheUpdate( fs::File *dirCache, const char*_itemName, folderItemType_t type );
    bool   dirCacheAdd( fs::File *dirCache, const char*itemName, folderItemType_t type );
    void   dirCacheSort(fs::File *cacheFile, size_t maxitems, activityTicker tickerCb = &debugFolderTicker );
    void   setDirCacheSize( fs::File *dirCacheFile, size_t totalitems = 0);
    size_t getDirCacheSize( fs::File *cacheFile );

    void setCleanFileName( char* dest, size_t len, const char* format, const char* src );

    int  addSong( const char * path, bool checkdupes = true );
    bool purge( const char * path );
    bool add( const char * path, SID_Meta_t *song, bool checkdupes = true );
    bool update( const char* path, SID_Meta_t *song );
    bool get( const char* md5path, SID_Meta_t *song );
    bool buildSidCacheFromDirCache( fs::File *sortedDirCache, activityTicker tickerCb = &debugFolderTicker );

    bool getInfoFromSIDFile( const char * path, SID_Meta_t *songinfo );

};


  // dafuq Arduino IDE ???
  #ifdef ARDUINO
    #include "SIDCacheManager.cpp"
  #endif



#endif
