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



// folder items
typedef enum
{
  F_PARENT_FOLDER,     // parent folder
  F_PLAYLIST,          // SID file list for current folder
  F_SUBFOLDER,         // subfolder with SID file list
  F_SUBFOLDER_NOCACHE, // subfolder with no SID file list
  F_SID_FILE,          // single SID file
  F_TOOL_CB            // virtual file representing a program/callback
} folderItemType_t;

// display list items
typedef struct
{
  String name; // label
  String path; // full path
  folderItemType_t type;
} folderTypeItem_t;


// default folder item (root folder)
folderTypeItem_t rootFolder = {
  "root",
  "/",
  F_SUBFOLDER
};


#include "SIDSearch.hpp"

// some callbacks CacheManager can use to nofity SIDExplorer UI
typedef bool (*activityTickerCb)( const char* title, size_t totalitems ); // callback for i/o activity during folder enumeration
typedef void (*folderElementPrintCb)( const char* fullpath, folderItemType_t type ); // adding a folder element to an enumerated list
typedef void (*keywordItemPrintCb)( sidpath_t* item, size_t itemnum ); // search result
typedef void (*activityProgressCb)( size_t progress, size_t total, size_t words_count, size_t files_count, size_t folders_count ); // progress cb
typedef bool (*searchResultInsertCb)( sidpath_t* sp, int rank );

// ranked search results (shard offsets really)
static std::vector<sidpath_rank_t> searchResults;

// callback type = folderElementPrintCb
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

// callback type = activityTickerCb
static bool debugFolderTicker( const char* title, size_t totalitems )
{
  // not really a progress function but can show
  // some "activity" during slow folder scans
  log_d("[%d] %s", totalitems, title);
  return true; // continue signal, send false to stop and break the scan loop
}

// callback type = searchResultInsertCb
static bool resultInserter( sidpath_t* sp, int rank )
{
  bool found = false;
  if( sp->type == F_SUBFOLDER ) rank *= 10; // folders first
  for( int i=0; i<searchResults.size(); i++ ) {
    if( searchResults[i].sidpath.offset == sp->offset ) {
      found = true;
      searchResults[i].rank += rank; // increase rank
      break;
    }
  }
  if( !found ) {
    // create entry
    sidpath_rank_t sr;
    sr.sidpath.offset = sp->offset;
    sr.sidpath.type   = sp->type;
    sr.rank = rank; // init rank
    searchResults.push_back( sr );
    return true;
  }
  return false;
}



class SongCacheManager
{

  public:

    ~SongCacheManager();
    SongCacheManager( SIDTunesPlayer *_sidPlayer );

    SID_Meta_t  *currenttrack = nullptr;

    size_t      CacheItemNum = 0;
    size_t      CacheItemSize = 0;

    void init( const char* path );

    bool   loadCache( const char* _cachepath );
    size_t getDirCacheSize( const char* pathname );
    void   deleteCache( const char* pathname );

    bool folderHasPlaylist( const char* fname );
    bool folderHasCache( const char* fname );
    bool folderHasPlaylist() { return folder_has_playlist; }
    bool folderHasCache() { return folder_has_cache; }

    bool getInfoFromCacheItem( size_t itemNum );
    bool getInfoFromCacheItem( const char* _cachepath, size_t itemNum );

    bool getInfoFromSIDFile( const char * path);
    bool getInfoFromSIDFile( const char * path, SID_Meta_t *songinfo );

    bool scanFolderCache( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb );
    bool scanPaginatedFolderCache( const char* pathname, size_t offset, size_t maxitems, folderElementPrintCb printCb, activityTickerCb tickerCb );

    bool scanFolder( const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTickerCb tickerCb = &debugFolderTicker, size_t maxitems = 0, bool force_regen = false );
    bool scanPaginatedFolder( const char* pathname, folderElementPrintCb printCb = &debugFolderElement, activityTickerCb tickerCb = &debugFolderTicker, size_t offset = 0, size_t maxitems = 0 );

    // Search implementations
    bool        buildSIDWordsIndex( const char* folderPath, activityTickerCb _ticker = nullptr, activityProgressCb _progress = nullptr );
    size_t      indexItemWords( const char* itemPath, folderItemType_t itemType, int64_t offset/* = -1*/, size_t KeywordMinLen/* = 2*/ );
    void        clearDict();
    void        addWordPathToDict(  wordpath_t* wp );
    wordpath_t* getWordPathFrom( const char* word );
    bool        parseKeywords( bool parse_files = true, bool parse_folders = true, size_t KeywordMinLenFolder = 3,  size_t KeywordMinLenFile = 3 );
    void        keywordsToShards();
    bool        cleanupShards();
    bool        loadShardsIndex();
    bool        find( const char* sentence, folderElementPrintCb printCb, size_t offset, size_t maxitems );
    bool        findFolder( const char* keyword, folderElementPrintCb cb, size_t offset = 0, size_t maxitems = 8, bool exact = true );

  private:

    SIDTunesPlayer *sidPlayer;
    fs::FS         *fs;
    fs::File       songCacheFile;

    char   *basename;
    char   *basepath;
    char   *cachepath;
    char   *md5path;
    String cleanPath;

    size_t maxItemsPerFolder = 1024; // hard limit for this app

    // Search utils
    std::map<char,size_t> shardsIdx; // shards index (index=first keywords letter, value=keywords count)
    std::map<char,shard_t*> shards; // loaded shards
    //std::vector<keyword_t, PSallocator<keyword_t> > keywords;
    //std::map<const char*, keyword_t*> keywordsmap;
    wordpath_t **SidDict = nullptr; // words dictionary (for keywords indexing)
    size_t SidDictSize = 0; // loaded words (for keywords indexing)
    size_t SidDictMemSize = 0; // allocated space for words (for keywords indexing)
    const size_t SidDictBlockSize = 512; // min allocated words (for keywords indexing)

    activityTickerCb ticker = nullptr;
    activityProgressCb progress = nullptr;
    searchResultInsertCb insertResult = &resultInserter;

    void         makeCachePath( const char* _cachepath, char* cachepath );
    const String makeCachePath( const char* path, const char* cachename );
    const String playListCachePath( const char * path );
    const String dirCachePath( const char * path );

    bool folder_has_playlist = false;
    bool folder_has_cache    = false;

    bool   dirCacheUpdate( fs::File *dirCache, const char*_itemName, folderItemType_t type );
    bool   dirCacheAdd( fs::File *dirCache, const char*itemName, folderItemType_t type );
    void   dirCacheSort(fs::File *cacheFile, size_t maxitems, activityTickerCb tickerCb = &debugFolderTicker );
    void   setDirCacheSize( fs::File *dirCacheFile, size_t totalitems = 0);
    size_t getDirCacheSize( fs::File *cacheFile );

    void setCleanFileName( char* dest, size_t len, const char* format, const char* src );

    // cache accessors
    bool exists( SID_Meta_t *song );
    bool get( const char* md5path, SID_Meta_t *song );
    bool add( const char * path, SID_Meta_t *song, bool checkdupes = true );
    int  addSong( const char * path, bool checkdupes = true );
    bool insert( SID_Meta_t *song, bool append = false );
    bool update( const char* path, SID_Meta_t *song );
    bool purge( const char * path );

    bool buildSidCacheFromDirCache( fs::File *sortedDirCache, activityTickerCb tickerCb = &debugFolderTicker );

    // provide common grounds to pre-2.0.0-alpha sdk's where fs::File->name() behave differently
    const char* fs_file_path( fs::File *file ) {
      #if defined ESP_IDF_VERSION_MAJOR && ESP_IDF_VERSION_MAJOR >= 4
        return file->path();
      #else
        return file->name();
      #endif
    }


};


  // dafuq Arduino IDE ???
  #ifdef ARDUINO
    #include "SIDCacheManager.cpp"
  #endif



#endif
