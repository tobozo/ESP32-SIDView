#include <map>

#define SHARDS_FOLDER MD5_FOLDER "/shards"
#define SHARDS_INDEX SHARDS_FOLDER "/.shards"
#define KEYWORDS_INDEX SHARDS_FOLDER "/.keywords"


typedef struct
{
  String word;
  size_t count;
} ban_keyword_count_t;


std::vector<ban_keyword_count_t> ban_vec_3 = {
  {"bit",0}, {"off",0}, {"box",0}, {"too",0}, {"der",0}, {"hot",0}, {"eye",0}, {"who",0},
  {"air",0}, {"mas",0}, {"now",0}, {"zax",0}, {"fly",0}, {"boy",0}, {"top",0}, {"hit",0},
  {"jam",0}, {"ode",0}, {"sea",0}, {"got",0}, {"try",0}, {"spy",0}, {"cry",0}, {"god",0},
  {"say",0}, {"why",0}, {"was",0}, {"mad",0}, {"sex",0}, {"das",0}, {"rap",0}, {"max",0},
  {"pop",0}, {"let",0}, {"can",0}, {"out",0}, {"get",0}, {"all",0}, {"iii",0}, {"man",0},
  {"its",0}, {"sid",0}, {"old",0}, {"day",0}, {"way",0}, {"zak",0}, {"new",0}, {"end",0},
  {"one",0}, {"you",0}, {"and",0}, {"mix",0}, {"for",0}, {"c64",0}, {"ice",0}, {"sky",0},
  {"sad",0}, {"art",0}, {"are",0}, {"sun",0}, {"die",0}, {"two",0}, {"the",0}, {"run",0},
  {"red",0}, {"bad",0}, {"war",0}, {"fun",0}, {"not",0}, {"big",0}
};
std::vector<ban_keyword_count_t> ban_vec_4 = {
  {"wave",0}, {"home",0}, {"hell",0}, {"edit",0}, {"crap",0}, {"into",0}, {"stop",0}, {"live",0},
  {"road",0}, {"that",0}, {"eyes",0}, {"evil",0}, {"free",0}, {"ball",0}, {"gold",0}, {"6581",0},
  {"wind",0}, {"jump",0}, {"soul",0}, {"only",0}, {"deep",0}, {"main",0}, {"baby",0}, {"roll",0},
  {"news",0}, {"8580",0}, {"xmas",0}, {"real",0}, {"rave",0}, {"lame",0}, {"fire",0}, {"cant",0},
  {"trip",0}, {"wars",0}, {"walk",0}, {"boom",0}, {"bach",0}, {"turn",0}, {"drum",0}, {"mini",0},
  {"some",0}, {"cold",0}, {"want",0}, {"tech",0}, {"play",0}, {"soft",0}, {"have",0}, {"here",0},
  {"come",0}, {"land",0}, {"goes",0}, {"logo",0}, {"loop",0}, {"side",0}, {"kill",0}, {"work",0},
  {"race",0}, {"head",0}, {"best",0}, {"show",0}, {"easy",0}, {"fuck",0}, {"true",0}, {"year",0},
  {"king",0}, {"cool",0}, {"shit",0}, {"with",0}, {"like",0}, {"hard",0}, {"high",0}, {"dont",0},
  {"star",0}, {"bass",0}, {"this",0}, {"test",0}, {"funk",0}, {"plus",0}, {"your",0}, {"from",0},
  {"blue",0}, {"more",0}, {"just",0}, {"over",0}, {"life",0}, {"back",0}, {"last",0}, {"beat",0},
  {"note",0}, {"2sid",0}, {"rock",0}, {"game",0}, {"time",0}, {"love",0}, {"song",0}, {"demo",0},
  {"part",0}, {"digi",0}, {"girl",0}, {"role",0}, {"wild",0}, {"nice",0}, {"menu",0}, {"lets",0},
  {"days",0}, {"dead",0}, {"owen",0}, {"long",0}, {"rain",0}, {"mega",0}, {"disk",0}, {"away",0},
  {"take",0}, {"tune",0}, {"down",0}, {"city",0}, {"acid",0}, {"lost",0}, {"good",0}, {"fast",0},
  {"moon",0}, {"jazz",0}, {"name",0}, {"what",0}, {"axel",0}, {"mind",0}, {"slow",0}, {"dark",0},
  {"zone",0}
};
std::vector<ban_keyword_count_t> ban_vec_5 = {
  {"first",1}, {"voice",0}, {"white",0}, {"quest",0}, {"merry",0}, {"small",0}, {"alien",0}, {"games",0},
  {"track",0}, {"crack",0}, {"piece",0}, {"chaos",0}, {"after",0}, {"funny",0}, {"score",0}, {"brain",0},
  {"heavy",0}, {"story",0}, {"weird",0}, {"tunes",0}, {"great",0}, {"never",0}, {"sonic",0}, {"thing",0},
  {"touch",0}, {"beach",0}, {"scene",0}, {"three",0}, {"still",0}, {"train",0}, {"tears",0}, {"china",0},
  {"march",0}, {"stars",0}, {"blood",0}, {"noise",0}, {"ocean",0}, {"under",0}, {"going",0}, {"fight",0},
  {"earth",0}, {"delta",0}, {"total",0}, {"ghost",0}, {"mania",0}, {"night",0}, {"disco",0}, {"power",0},
  {"black",0}, {"crazy",0}, {"house",0}, {"super",0}, {"funky",0}, {"sound",0}, {"party",0}, {"world",0},
  {"light",0}, {"short",0}, {"magic",0}, {"dance",0}, {"dream",0}, {"happy",0}, {"space",0}, {"theme",0},
  {"basic",0}, {"remix",0}, {"music",0}, {"years",0}, {"intro",0}, {"amiga",0}, {"metal",0}, {"times",0},
  {"break",0}, {"compo",0}, {"sweet",0}, {"storm",0}, {"force",0}, {"speed",0}, {"bells",0}, {"again",0},
  {"heart",0}, {"cover",0}, {"level",0}, {"style",0}, {"blues",0}, {"final",0}, {"ninja",0}, {"title",0},
  {"death",0}
};
std::vector<ban_keyword_count_t> ban_vec_6 = {
  {"winter",0}, {"people",0}, {"broken",0}, {"simple",0}, {"living",0}, {"castle",0}, {"knight",0}, {"jungle",0},
  {"flying",0}, {"planet",0}, {"remake",0}, {"runner",0}, {"fields",0}, {"secret",0}, {"maniac",0}, {"vision",0},
  {"around",0}, {"legend",0}, {"strike",0}, {"killer",0}, {"flight",0}, {"heaven",0}, {"cosmic",0}, {"writer",0},
  {"always",0}, {"action",0}, {"loader",0}, {"little",0}, {"dreams",0}, {"future",0}, {"summer",0}, {"street",0},
  {"trance",0}, {"jingle",0}, {"groove",0}, {"melody",0}, {"double",0}, {"shadow",0}, {"techno",0}, {"dragon",0},
  {"return",0}, {"attack",0}, {"battle",0}, {"silent",0}, {"escape",0}, {"beyond",0}, {"monday",0}, {"second",0},
  {"sample",0}, {"master",0}
};
std::vector<ban_keyword_count_t> ban_vec_7 = {
  {"control",0}, {"nothing",0}, {"friends",0}, {"feeling",0}, {"special",0}, {"fighter",0}, {"project",0}, {"eternal",0},
  {"express",0}, {"popcorn",0}, {"morning",0}, {"silence",0}, {"madness",0}, {"dancing",0}, {"airwolf",0}, {"preview",0},
  {"contact",0}, {"tribute",0}, {"revenge",0}, {"fantasy",0}, {"mission",0}, {"machine",0}, {"strange",0}, {"warrior",0},
  {"unknown",0}, {"forever",0}, {"digital",0}, {"another",0}, {"version",0}
};
std::vector<ban_keyword_count_t> ban_vec_8 = {
  {"birthday",0}, {"magazine",0}, {"darkness",0}, {"hardcore",0}, {"midnight",0}, {"extended",0}, {"memories",0}, {"dreaming",0},
  {"magnetic",0}
};
std::vector<ban_keyword_count_t> ban_vec_9 = {
  {"christmas",89}, {"introtune",74}, {"adventure",42}, {"simulator",39}, {"worktunes",37}, {"nightmare",35}, {"challenge",33}, {"something",33}
};
std::vector<ban_keyword_count_t> ban_vec_10 = {
  {"collection",129}
};


typedef struct
{
  size_t kwlen;
  std::vector<ban_keyword_count_t> vecptr;
} ban_map_t;


static std::vector<ban_map_t> ban_vec = {
  { 3,  ban_vec_3 },
  { 4,  ban_vec_4 },
  { 5,  ban_vec_5 },
  { 6,  ban_vec_6 },
  { 7,  ban_vec_7 },
  { 8,  ban_vec_8 },
  { 9,  ban_vec_9 },
  { 10, ban_vec_10 }
};

static std::map<size_t, std::vector<ban_keyword_count_t>> ban_map = {
  { 3,  ban_vec_3 },
  { 4,  ban_vec_4 },
  { 5,  ban_vec_5 },
  { 6,  ban_vec_6 },
  { 7,  ban_vec_7 },
  { 8,  ban_vec_8 },
  { 9,  ban_vec_9 },
  { 10, ban_vec_10 }
};



template <class T> void swap_any(T& a, T& b) { T t = a; a = b; b = t; }


// abstraction of the indexed items
typedef struct
{
  size_t offset;         // offset in md5 HVSC file, where the path/song-lengths are
  folderItemType_t type; // item type (sid file or folder)
} sidpath_t;


// keyword => paths container
typedef struct _wordpath_t
{
  char* word = nullptr; // keyword
  size_t pathcount = 0; // amount of paths for this keyword
  sidpath_t** paths = nullptr;
} wordpath_t;


typedef struct // shard_t
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
} shard_t;


typedef struct
{
  char name[3] = {0,0,0};
  char filename[255];
  char keywordStart[255] = {0};
  char keywordEnd[255] = {0};
} shard_io_t;


// search result container
typedef struct
{
  int16_t rank = 0;       // search result rank (for sorting)
  sidpath_t sidpath; // abstract indexed item
  char path[255];     // realistic path (HVSC path + local path prefix)
} sidpath_rank_t;



typedef bool (*offsetInserter)( sidpath_t* sp, int rank );

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



ShardStream *wpio = new ShardStream(nullptr);

#ifdef ARDUINO
  #include "SIDSearch.cpp"
#endif




