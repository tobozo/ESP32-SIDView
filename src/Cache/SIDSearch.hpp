#pragma once

#include "../config.h"

#if defined ENABLE_HVSC_SEARCH

  #include <map>
  #include <vector>

  #define SHARDS_FOLDER MD5_FOLDER "/shards"
  #define SHARDS_INDEX SHARDS_FOLDER "/.shards"
  #define KEYWORDS_INDEX SHARDS_FOLDER "/.keywords"


  namespace SIDView
  {

    struct ban_keyword_count_t
    {
      String word;
      size_t count;
    } ;

    struct ban_map_t
    {
      size_t kwlen;
      std::vector<ban_keyword_count_t> vecptr;
    } ;

    extern std::map<size_t, std::vector<ban_keyword_count_t>> ban_map;

    template <class T> void swap_any(T& a, T& b) { T t = a; a = b; b = t; }


    // abstraction of the indexed items
    struct sidpath_t
    {
      size_t offset;         // offset in md5 HVSC file, where the path/song-lengths are
      folderItemType_t type; // item type (sid file or folder)
    } ;


    // keyword => paths container
    struct wordpath_t
    {
      char* word = nullptr; // keyword
      size_t pathcount = 0; // amount of paths for this keyword
      sidpath_t** paths = nullptr;
    } ;


    struct shard_t
    {
      char name = '\0';
      size_t wordscount = 0; // how many keywords in this shard
      size_t memsize = 0;
      wordpath_t** words = nullptr;

      size_t blocksize() {
        return 16;
      };

      void setSize( size_t size ) {
        if( memsize == 0 ) {
          assert( words == nullptr );
          words = (wordpath_t**)sid_calloc( size, sizeof( wordpath_t* ) );
        } else {
          assert( words != nullptr );
          if( size == memsize ) return; // no change
          assert( size > memsize ); // TODO: handle shrink
          if( size>1 && size == memsize+1 ) { // increment by block
            size = memsize+blocksize();
          }
          words = (wordpath_t**)sid_realloc( words, size*sizeof( wordpath_t* ) );
        }
        //log_w("Shard Size=%d", size );
        memsize = size;
      }

      void addWord( wordpath_t* w ) {
        assert(w);
        if( memsize == 0 ) {
          setSize( 1 );
        } else {
          if( wordscount+1 > memsize ) {
            setSize( memsize+blocksize() );
          }
        }
        words[wordscount] = w;
        wordscount++;
      }

      void sortWords() {
        if( wordscount <= 1 ) {
          //log_e("Nothing to sort!");
          return;
        }
        unsigned long swap_start = millis();
        size_t swapped = 0;
        bool slow = wordscount > 100;
        int i = 0, j = 0, n = wordscount;
        for (i = 0; i < n; i++) {   // loop n times - 1 per element
          for (j = 0; j < n - i - 1; j++) { // last i elements are sorted already
            // alpha sort
            if ( strcmp(words[j]->word, words[j + 1]->word) > 0 ) {
              swap_any( words[j], words[j + 1] );
              swapped++;
            }
          }
          if( slow ) vTaskDelay(1);
        }
        unsigned long sorting_time = millis() - swap_start;
        if( sorting_time > 0 && swapped > 0 ) {
          log_d("Sorting %d/%d words took %d millis", swapped, n,  sorting_time );
        }
      }
    /*
      void clearWords() {
        for( int i=wordscount-1;i>=0;i-- ) {
          if( words[i] != NULL ) {
            words[i]->clear();
          }
        }
        wordscount = 0;
      }
    */
    };


    struct shard_io_t
    {
      char name[3] = {0,0,0};
      char filename[255];
      char keywordStart[255] = {0};
      char keywordEnd[255] = {0};
    } ;


    // search result container
    struct sidpath_rank_t
    {
      int16_t rank = 0;       // search result rank (for sorting)
      sidpath_t sidpath; // abstract indexed item
      char path[255];     // realistic path (HVSC path + local path prefix)
    } ;



    // ranked search results (shard offsets really)
    static std::vector<sidpath_rank_t> searchResults;

    /*
    // some sweetener to use std::vector with psram
    template <class T>
    struct PSallocator {
      typedef T value_type;
      PSallocator() = default;
      template <class U> constexpr PSallocator(const PSallocator<U>&) noexcept {}
      #if __cplusplus >= 201703L
      [[nodiscard]]
      #endif
      T* allocate(std::size_t n) {
        if(n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
        if(auto p = static_cast<T*>(ps_malloc(n*sizeof(T)))) return p;
        throw std::bad_alloc();
      }
      void deallocate(T* p, std::size_t) noexcept { std::free(p); }
    };
    template <class T, class U>
    bool operator==(const PSallocator<T>&, const PSallocator<U>&) { return true; }
    template <class T, class U>
    bool operator!=(const PSallocator<T>&, const PSallocator<U>&) { return false; }
    //std::vector<int, PSallocator<int> > v;
    */

    class ShardStream
    {
      public:
        ShardStream( wordpath_t *_wp ) : wp(_wp) { };
        void set(wordpath_t *_wp) { wp = _wp; }
        bool set(size_t offset, folderItemType_t itemType, const char* keyword);
        bool addPath(size_t offset, folderItemType_t itemType, wordpath_t *_wp);
        wordpath_t* getwp() { return wp; }
        size_t size(wordpath_t *_wp) { set(_wp); return size(); };

        size_t size();
        void sortPaths();
        void clearPaths();

        void setShardYard( uint8_t _yardsize=16, size_t _totalbytes=0, size_t _blocksize=256 );
        void freeShardYard();
        void listShardYard();
        bool loadShardYard( bool force = false );
        bool saveShardYard();
        uint8_t guessShardNum( const char* keyword );
        size_t writeShard();
        size_t readShard();
        void freeItem();
        fs::File* openShardFile( const char* path, const char* mode = nullptr );
        void closeShardFile();
        fs::File* openNextShardFile( const char* mode = nullptr );
        //fs::File* openKeywordsFile( const char* path, const char* mode = nullptr );
        size_t findKeyword( uint8_t shardId, const char* keyword, size_t offset, size_t maxitems, bool exact, offsetInserter insertOffset);
        bool isBannedKeyword( const char* keyword );
        void resetBanMap();
        void addBannedKeyword( const char* keyword, size_t count );

        size_t max_paths_per_keyword = 32; // keyword->paths count
        size_t max_paths_in_folder = 4; // folder->keyword->paths count

        fs::File keywordsFile;
        std::map<const char*,size_t> keywordsInfolder;
        //char shardFilename[255];
        //char keywordsFilename[255];
        fs::FS *fs;
        bool loaded = false;
      private:
        wordpath_t* wp = nullptr;
        fs::File shardFile;
        size_t bytes_per_shard;
        //size_t total_bytes;
        uint8_t shardnum;
        size_t processed;
        uint8_t yardsize=0;
        size_t totalbytes=0;
        size_t blocksize=0;
        shard_io_t** shardyard = nullptr;

    };



    static ShardStream *wpio = new ShardStream(nullptr);

    // #ifdef ARDUINO
    //   #include "SIDSearch.cpp"
    // #endif


  }; // end namespace SIDView


#endif // defined ENABLE_HVSC_SEARCH
