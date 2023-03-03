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

#pragma once

#include "../config.h"
#include "../helpers/Path_Utils.hpp"

namespace SIDView
{

  class SongCacheManager
  {

    public:

      ~SongCacheManager();
      SongCacheManager( SIDTunesPlayer *_sidPlayer, size_t _itemsPerPage );

      static SongCacheManager* getSongCache();

      SID_Meta_t *currenttrack = nullptr;

      size_t CacheItemNum = 0;
      size_t CacheItemSize = 0;
      size_t itemsPerPage;

      void init( const char* path );

      void setCacheType( SongCacheType_t type ) { cache_type = type; }
      SongCacheType_t getCacheType() { return cache_type; }

      size_t getDirSize( const char* pathname );
      bool folderHasCache( const char* fname );
      bool getInfoFromItem( const char* _cachepath, size_t itemNum );

      bool scanFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count=nullptr, size_t maxitems=0, bool force_regen=false );
      bool scanPaginatedFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count=nullptr, size_t offset=0, size_t maxitems=0 );

      Deepsid::DirItem fetchDeepSIDItem( size_t offset );
      Deepsid::Dir *getDeepsidDir(const char* pathname, activityTickerCb tickerCb );
      Deepsid::Dir *deepsidDir() { return deepsid_dir; }

      size_t getDeepsidCacheSize( const char* pathname );
      bool getInfoFromDeepsidItem( const char* _cachepath, size_t itemNum );
      bool getInfoFromDeepsidItem( size_t itemNum );
      bool scanDeepSIDFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t maxitems = 0, bool force_regen = false );
      bool scanPaginatedDeepSIDFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t offset = 0, size_t maxitems = 0 );
      size_t syncDeepSIDFiles( activityTickerCb tickerCb );


      bool scanLocalFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t maxitems = 0, bool force_regen = false );
      bool scanPaginatedLocalFolder( const char* pathname, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count, size_t offset = 0, size_t maxitems = 0 );
      bool scanPaginatedFolderCache( const char* pathname, size_t offset, size_t maxitems, folderElementPrintCb printCb, activityTickerCb tickerCb, size_t *count );
      bool getInfoFromCacheItem( const char* _cachepath, size_t itemNum );
      bool getInfoFromCacheItem( size_t itemNum );
      size_t getDirCacheSize( const char* pathname );
      bool folderHasPlaylist( const char* fname );
      bool folderHasPlaylist() { return folder_has_playlist; }
      bool   loadCache( const char* _cachepath );
      void   deleteCache( const char* pathname );

    private:

      SIDTunesPlayer *sidPlayer;
      fs::FS         *fs;
      fs::File       songCacheFile;

      //Deepsid::DirItem deepsid_diritem;

      Deepsid::Dir   *deepsid_dir = nullptr;

      SongCacheType_t cache_type = CACHE_DOTFILES;

      char   *basename;
      char   *basepath;
      char   *cachepath;
      char   *md5path;
      String cleanPath;

      size_t maxItemsPerFolder = 1024; // hard limit for this app

      activityTickerCb ticker = nullptr;
      activityProgressCb progress = nullptr;

      void         makeCachePath( const char* _cachepath, char* cachepath );
      const String makeCachePath( const char* path, const char* cachename );
      const String playListCachePath( const char * path );
      const String dirCachePath( const char * path );

      bool folder_has_playlist = false;
      bool folder_has_cache    = false;

      bool   dirCacheUpdate( fs::File *dirCache, const char*_itemName, folderItemType_t type );
      bool   dirCacheAdd( fs::File *dirCache, const char*itemName, folderItemType_t type );
      void   dirCacheSort(fs::File *cacheFile, size_t maxitems, activityTickerCb tickerCb );
      void   setDirCacheSize( fs::File *dirCacheFile, size_t totalitems = 0);
      size_t seekDirCacheSize( fs::File *cacheFile );

      void setCleanFileName( char* dest, size_t len, const char* format, const char* src );

      // cache accessors
      bool exists( SID_Meta_t *song );
      //bool get( const char* md5path, SID_Meta_t *song );
      bool add( const char * path, SID_Meta_t *song, bool checkdupes = true );
      int  addSong( const char * path, bool checkdupes = true );
      bool insert( SID_Meta_t *song, bool append = false );
      bool update( const char* path, SID_Meta_t *song );
      bool purge( const char * path );

      bool buildSidCacheFromDirCache( fs::File *sortedDirCache, activityTickerCb tickerCb );

      // provide common grounds to pre-2.0.0-alpha sdk's where fs::File->name() behave differently
      const char* fs_file_path( fs::File *file ) {
        #if defined ESP_IDF_VERSION_MAJOR && ESP_IDF_VERSION_MAJOR >= 4
          return file->path();
        #else
          return file->name();
        #endif
      }


    #if defined ENABLE_HVSC_SEARCH
      public:
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
        // callback type = searchResultInsertCb
        static bool resultInserter( sidpath_t* sp, int rank );/*
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
        }*/
      private:
        // Search utils
        std::map<char,size_t> shardsIdx; // shards index (index=first keywords letter, value=keywords count)
        std::map<char,shard_t*> shards; // loaded shards
        //std::vector<keyword_t, PSallocator<keyword_t> > keywords;
        //std::map<const char*, keyword_t*> keywordsmap;
        wordpath_t **SidDict = nullptr; // words dictionary (for keywords indexing)
        size_t SidDictSize = 0; // loaded words (for keywords indexing)
        size_t SidDictMemSize = 0; // allocated space for words (for keywords indexing)
        const size_t SidDictBlockSize = 512; // min allocated words (for keywords indexing)
        searchResultInsertCb insertResult = &resultInserter;
    #endif


  };

  extern SongCacheManager *SongCache;

}; // end namespace SIDView


#if defined ENABLE_HVSC_SEARCH
  #include "SIDSearch.hpp"
#endif


