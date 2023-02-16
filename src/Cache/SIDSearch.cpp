
#include "SIDSearch.hpp"
#include "SIDCacheManager.hpp"


namespace SIDView
{


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


  std::vector<ban_map_t> ban_vec = {
    { 3,  ban_vec_3 },
    { 4,  ban_vec_4 },
    { 5,  ban_vec_5 },
    { 6,  ban_vec_6 },
    { 7,  ban_vec_7 },
    { 8,  ban_vec_8 },
    { 9,  ban_vec_9 },
    { 10, ban_vec_10 }
  };

  std::map<size_t, std::vector<ban_keyword_count_t>> ban_map = {
    { 3,  ban_vec_3 },
    { 4,  ban_vec_4 },
    { 5,  ban_vec_5 },
    { 6,  ban_vec_6 },
    { 7,  ban_vec_7 },
    { 8,  ban_vec_8 },
    { 9,  ban_vec_9 },
    { 10, ban_vec_10 }
  };


  bool ShardStream::isBannedKeyword( const char* keyword )
  {
    assert( keyword );
    size_t kwlen = strlen(keyword);

    for( auto item : ban_map ) {
      size_t mapkwlen = item.first;
      if( kwlen != mapkwlen ) continue;
      for( int i=0; i<ban_map[mapkwlen].size(); i++ ) {
        ban_keyword_count_t* bkc = &ban_map[mapkwlen][i];
        if( strcmp( keyword, bkc->word.c_str() ) == 0 ) {
          bkc->count++;
          return true;
        }
      }
      return false;
    }

    return false;
  }


  void ShardStream::addBannedKeyword( const char* keyword, size_t count )
  {
    assert( keyword );
    size_t kwlen = strlen(keyword);
    ban_map[kwlen].push_back({ keyword, count });
  }



  void ShardStream::resetBanMap()
  {
    for( auto item : ban_map ) {
      size_t kwlen = item.first;
      for( int i=0; i<ban_map[kwlen].size(); i++ ) {
        ban_keyword_count_t* bkc = &ban_map[kwlen][i];
        bkc->count = 0;
      }
    }
  }





  bool ShardStream::set(size_t offset, folderItemType_t itemType, const char* keyword)
  {
    assert(keyword);
    size_t tokenLen = strlen(keyword);
    wp = (wordpath_t*)sid_calloc(1, sizeof(wordpath_t) );
    if( wp == NULL ) {
      log_e("Can't alloc %d bytes, aborting", sizeof(wordpath_t));
      return false;
    }
    wp->paths = (sidpath_t**)sid_calloc(1, sizeof(sidpath_t*));
    if( wp->paths == NULL ) {
      log_e("Can't alloc %d bytes, aborting", sizeof(sidpath_t*));
      return false;
    }
    sidpath_t* sp = (sidpath_t*)sid_calloc(1, sizeof(sidpath_t) );
    if( sp == NULL ) {
      log_e("Can't alloc %d bytes for sidpath_t", sizeof(sidpath_t) );
      return false;
    }
    sp->offset = size_t(offset);
    sp->type = itemType;
    char* w = (char*)sid_calloc(tokenLen+1, sizeof(char));
    if( w == NULL ) {
      log_e("Can't alloc %d bytes for keyword", (tokenLen+1)*sizeof(char) );
      return false;
    }
    memcpy( (char*)w, keyword, tokenLen );
    //sp->path = p;
    wp->paths[0] = sp;
    wp->pathcount = 1;
    wp->word = w;
    return true;
  }




  bool ShardStream::addPath(size_t offset, folderItemType_t itemType, wordpath_t *_wp)
  {
    assert(_wp);
    set(_wp);
    assert(wp->paths);
    wp->paths = (sidpath_t**)sid_realloc(wp->paths, (wp->pathcount+1)*sizeof(sidpath_t*));
    if( wp->paths == NULL ) {
      log_e("Can't alloc %d bytes, aborting", sizeof(sidpath_t*));
      return false;
    }
    sidpath_t* sp = (sidpath_t*)sid_calloc(1, sizeof(sidpath_t) );
    if( sp == NULL ) {
      log_e("Can't alloc %d bytes for sidpath_t", sizeof(sidpath_t) );
      return false;
    }
    sp->offset = size_t(offset);
    sp->type = itemType;
    wp->paths[wp->pathcount] = sp;
    wp->pathcount++;
    return true;
  }




  size_t ShardStream::size() {
    assert( wp );
    assert( wp->word );
    assert( wp->paths );
    size_t objsize = 0;
    objsize += sizeof(size_t); // header with full object size (can be used as integrity check or fast-skip)
    objsize += sizeof(size_t); // where wp->word's strlen is stored
    objsize += sizeof(size_t); // where wp->paths count is stored
    objsize += strlen( wp->word );
    objsize += (wp->pathcount)*sizeof(sidpath_t);
    return objsize;
  }


  void ShardStream::sortPaths()
  {
    assert( wp );
    assert( wp->word );
    assert( wp->paths );
    if( wp->pathcount <= 1 ) {
      //log_e("Nothing to sort!");
      return;
    }
    unsigned long swap_start = millis();
    size_t swapped = 0;
    int i = 0, j = 0, n = wp->pathcount;
    for (i = 0; i < n; i++) {   // loop n times - 1 per element
      for (j = 0; j < n - i - 1; j++) { // last i elements are sorted already
        if( wp->paths[j]->type != wp->paths[j + 1]->type ) {
          // folders first, predates alphabetical order
          if ( wp->paths[j + 1]->type == F_SUBFOLDER ) {
            swap_any( wp->paths[j], wp->paths[j + 1] );
            swapped++;
          }
          continue;
        }
        // alpha next
        if ( wp->paths[j]->offset > wp->paths[j + 1]->offset ) {
          swap_any( wp->paths[j], wp->paths[j + 1] );
          swapped++;
        }
      }
    }
    unsigned long sorting_time = millis() - swap_start;
    if( sorting_time > 0 && swapped > 0 ) {
      log_w("Sorting %d/%d paths took %d millis", swapped, wp->pathcount, sorting_time );
    }
  }


  void ShardStream::freeItem() {

    if( wp == nullptr ) return;
    for( int i=0;i<wp->pathcount;i++ ) {
      if( wp->paths[i] != nullptr ) {
        free( wp->paths[i] );
        wp->paths[i] = nullptr;
      }
    }
    if( wp->paths != nullptr) {
      free( wp->paths );
      wp->paths = nullptr;
    }
    if( wp->word != nullptr ) {
      free( wp->word );
      wp->word = nullptr;
    }
    wp->pathcount = 0;
  };


  void ShardStream::clearPaths()
  {
    assert( wp );
    if( wp->paths != NULL ) {
      for( int i=wp->pathcount-1;i>=0;i-- ) {
        if( wp->paths[i] != NULL ) {
          free( wp->paths[i] );
          wp->paths[i] = NULL;
          vTaskDelay(1);
        }
      }
      free( wp->paths );
      wp->paths = NULL;
    }
    if( wp->word != NULL ) {
      free( wp->word );
      wp->word = NULL;
    }
    wp->pathcount = 0;
  }




  size_t ShardStream::readShard() {
    assert( wp );
    assert( wp->word == nullptr );
    assert( wp->paths == nullptr );
    if( !shardFile.available() ) return 0;
    size_t objsize = 0;
    size_t wordlen = 0;
    size_t read_bytes = 0;
    read_bytes += shardFile.readBytes( (char*)&objsize, sizeof( size_t ) );
    read_bytes += shardFile.readBytes( (char*)&wordlen, sizeof( size_t ) );
    read_bytes += shardFile.readBytes( (char*)&wp->pathcount, sizeof( size_t ) );
    wp->word = (char*)sid_calloc( wordlen+1, sizeof(char));
    if( wp->word == NULL ) {
      log_e("Can't alloc %d bytes, aborting", wordlen);
      return 0;
    }
    read_bytes += shardFile.readBytes( (char*)wp->word, wordlen );
    wp->paths = (sidpath_t**)sid_calloc(wp->pathcount, sizeof(sidpath_t*));
    if( wp->paths == NULL ) {
      log_e("Can't alloc %d bytes, aborting", wp->pathcount*sizeof(sidpath_t*));
      free( wp->word );
      return 0;
    }
    for( int i=0;i<wp->pathcount;i++ ) {
      wp->paths[i] = (sidpath_t*)sid_calloc(wp->pathcount, sizeof(sidpath_t));
      if( wp->paths[i] == NULL ) {
        log_e("Can't alloc %d bytes, aborting", sizeof(sidpath_t));
        wp->pathcount = i-1;
        //wp->clear();
        return 0;
      }
      read_bytes += shardFile.readBytes( (char*)wp->paths[i], sizeof( sidpath_t ) );
    }
    if( read_bytes != objsize ) {
      log_w("read size mismatch (expected %d, got %d", objsize, read_bytes );
    }
    //vTaskDelay(1);
    return read_bytes;
  }



  size_t ShardStream::writeShard() {
    assert( wp );
    assert( wp->word );
    assert( wp->paths );
    size_t objsize = size();

    fs::File *_shardFile = nullptr;

    if( shardnum == 0 || processed + objsize > bytes_per_shard ) {
      // rotate file name
      if( shardyard[shardnum]->keywordStart[0] == '\0' ) {
        log_d("Assigning keyword start '%s' to shard %d", wp->word, shardnum );
        snprintf(shardyard[shardnum]->keywordStart, 255, "%s", wp->word );
        //shardyard[shardnum]->keywordStart = wp->word;
      }
      _shardFile = openNextShardFile("w");

      if( _shardFile == nullptr ) {
        log_e("Can't open next shard file, aborting");
        return 0;
      }
      shardnum++;
      processed = 0;
    } else {
      //shardyard[shardnum-1]->keywordEnd = wp->word;
      snprintf(shardyard[shardnum-1]->keywordEnd, 255, "%s", wp->word );
      _shardFile = &shardFile;
    }

    size_t written_bytes = 0;
    size_t wordlen = strlen( wp->word );
    written_bytes += _shardFile->write( (uint8_t*)&objsize, sizeof(size_t) ); // obj size
    written_bytes += _shardFile->write( (uint8_t*)&wordlen, sizeof(size_t) ); // keyword size
    written_bytes += _shardFile->write( (uint8_t*)&wp->pathcount, sizeof(size_t) ); // paths count
    written_bytes += _shardFile->write( (uint8_t*)wp->word, wordlen );
    for( int i=0;i<wp->pathcount;i++ ) {
      written_bytes += _shardFile->write( (uint8_t*)wp->paths[i], sizeof( sidpath_t ) );
    }
    if( written_bytes != objsize ) {
      log_w("write size mismatch (expected %d, got %d", objsize, written_bytes );
    }
    //vTaskDelay(1);
    processed += written_bytes;
    return written_bytes;
  }




  size_t ShardStream::findKeyword( uint8_t shardId, const char* keyword, size_t offset, size_t maxitems, bool exact, offsetInserter insertOffset)
  {

    assert( shardyard != nullptr );
    assert( shardId <= yardsize );

    if( !openShardFile( shardyard[shardId]->filename ) ) {
      log_e("Unable to open shard file");
      return 0;
    }

    freeItem();
    wp = (wordpath_t*)sid_calloc(1, sizeof(wordpath_t));

    if( wp == NULL ) {
      log_e("Unable to alloc %d bytes for shard #%d", sizeof(wordpath_t), shardId );
      closeShardFile();
      return 0;
    }

    size_t path_read = readShard();
    size_t kept  = 0;
    size_t found = 0;
    String keywordStr = String(keyword);

    do {
      int rank = 0;
      /*
      if( wp->word[0] != keyword[0] ) rank -= strlen(keyword); // first letter match
      if( strstr( wp->word, keyword ) !=NULL ) rank += strlen(keyword); // substring match
      if( strcmp( wp->word, keyword ) == 0 ) rank += 100; // full keyword match
      */
      if( String(wp->word).startsWith( keywordStr ) ) {
      //if( wp->word[0] == keyword[0] && strcmp( wp->word, keyword ) == 0 )
        for( int j=0;j<wp->pathcount;j++ ) {
          found++;
          if( found >= offset ) {
            if( kept+1 < maxitems ) {
              kept += insertOffset( wp->paths[j], rank ) ? 1 : 0;
            }
          }
          if( kept >= maxitems ) {
            freeItem();
            return kept;
          }
        }
      }

      freeItem();
      path_read = readShard();

    } while( path_read > 0 );

    return kept;
  }



  void ShardStream::closeShardFile()
  {
    if( shardFile ) {
      log_d("Closing %s", shardFile.name() );
      shardFile.close();
    }
  }



  fs::File* ShardStream::openShardFile( const char* path, const char* mode )
  {
    //fs = _fs;
    if( mode == nullptr ) {
      log_d("Opening file %s as readonly", path );
      shardFile = fs->open( path );
    } else {
      log_d("Opening file %s with mode %s", path, mode );
      shardFile = fs->open( path, mode );
    }
    return &shardFile;
  }



  fs::File* ShardStream::openNextShardFile( const char* mode )
  {
    assert( shardyard );
    //fs = _fs;
    //uint8_t nextShardnum = shardnum+1;
    if( shardnum > yardsize ) {
      log_e("requested shard (%d) exceeds yardsize (%d)", shardnum, yardsize );
      return nullptr;
    }
    closeShardFile();
    return openShardFile( shardyard[shardnum]->filename, mode);
  }



  uint8_t ShardStream::guessShardNum( const char* keyword )
  {
    for( uint8_t i=0;i<yardsize; i++ ) {
      if( keyword[0] >= shardyard[i]->keywordStart[0] && keyword[0] <= shardyard[i]->keywordEnd[0] ) {
        if( keyword[0] < shardyard[i]->keywordEnd[0] ) return i;
        // shares common first letter, see if it's ordered after or before the end
        size_t mlen = min( strlen(keyword), strlen(shardyard[i]->keywordEnd) );
        for( int j=1; j<mlen; j++ ) {
          if( keyword[j] > shardyard[i]->keywordEnd[j] ) {
            goto _next; // not in that shard
          }
        }
        log_w("Giving shard #%d (%s < %s > %s ", i, shardyard[i]->keywordStart, keyword, shardyard[i]->keywordEnd );
        return i;
      }
      _next:
      void(); // goto doesn't like closure
    }

    log_w("Keyword %s is outside words range", keyword);
    return 0xff;

  }



  void ShardStream::freeShardYard()
  {
    for( int i=0;i<yardsize; i++ ) {
      if( shardyard[i] != NULL ) {
        free( shardyard[i] );
        shardyard[i] = NULL;
      }
    }
    if( shardyard != NULL ) {
      free( shardyard );
      shardyard = NULL;
    }
    loaded          = false;
    bytes_per_shard = 0;
    shardnum        = 0;
    processed       = 0;
    yardsize        = 0;
    totalbytes      = 0;
    blocksize       = 0;
  }


  void ShardStream::listShardYard()
  {
    for( int i=0;i<yardsize; i++ ) {
      log_w("Shard index '%s' [%s-%s]", shardyard[i]->name, shardyard[i]->keywordStart, shardyard[i]->keywordEnd );
    }
  }


  bool ShardStream::loadShardYard( bool force ) // cost ~= 150ms
  {
    if( fs == nullptr ) return false;

    if( loaded || shardyard != nullptr ) {
      if( force ) {
        freeShardYard();
      } else {
        return true;
      }
    }

    fs::File shardIdxFile = fs->open( SHARDS_INDEX );

    if( !shardIdxFile ) {
      log_e("Unable to create %s shard index file, aborting", SHARDS_INDEX );
      return false;
    }

    String line = shardIdxFile.readStringUntil('\n');
    line.trim();
    line.replace("shards:", "");
    yardsize = line.toInt();

    if( yardsize > 16 || yardsize ==0 ) {
      log_e("Bad yardsize in idx file, aborting");
      return false;
    } else {
      log_w("ShardYard has %d shards", yardsize );
    }

    shardnum = 0;
    processed = 0;
    shardyard = (shard_io_t**)sid_calloc( yardsize, sizeof(shard_io_t*) );

    while( shardIdxFile.available() ) {
      line = shardIdxFile.readStringUntil('\n');
      line.trim();
      if( line.length() <= 2 ) { // EOF ?
        if( shardnum != yardsize ) {
          log_e("Bad shard count at EOF (got: %d, expected: %d)", shardnum, yardsize );
          return false;
        }
        break;
      }
      if( shardnum+1 > yardsize ) {
        log_e("Too many shards, aborting!");
        return false;
      }
      int idx1 = line.indexOf(";");
      int idx2 = line.indexOf(":");
      String kwname  = line.substring(0, idx1 );
      String kwstart = line.substring(idx1+1, idx2 );
      String kwend   = line.substring( idx2+1 );
      shardyard[shardnum] = (shard_io_t*)sid_calloc( 1, sizeof(shard_io_t) );
      snprintf(shardyard[shardnum]->name, 3, "%s", kwname.c_str() );
      snprintf(shardyard[shardnum]->keywordStart, 255, "%s", kwstart.c_str() );
      snprintf(shardyard[shardnum]->keywordEnd, 255, "%s", kwend.c_str() );
      snprintf(shardyard[shardnum]->filename, 255, "%s/%s", SHARDS_FOLDER, shardyard[shardnum]->name );
      log_w("Extracted '%s' => [%s - %s] @ %s", kwname.c_str(), kwstart.c_str(), kwend.c_str(), shardyard[shardnum]->filename );
      shardnum++;
    }
    shardnum = 0;
    loaded = true;
    return true;
  }


  bool ShardStream::saveShardYard()
  {
    if( fs == nullptr )        return false;
    if( yardsize<=0 )          return false;
    if( shardyard == nullptr ) return false;

    fs::File shardIdxFile = fs->open( SHARDS_INDEX, "w" );

    if( !shardIdxFile ) {
      log_e("Unable to create %s shard index file, aborting", SHARDS_INDEX );
      return false;
    }

    shardIdxFile.printf("shards:%d\n", yardsize );

    for( int i=0;i<yardsize; i++ ) {
      shardIdxFile.printf("%s;%s:%s\n", shardyard[i]->name, shardyard[i]->keywordStart, shardyard[i]->keywordEnd );
    }

    shardIdxFile.close();
    return true;

  }



  void ShardStream::setShardYard( uint8_t _yardsize, size_t _totalbytes, size_t _blocksize )
  {
    assert( _yardsize > 0 && _yardsize <=16 );
    assert( _blocksize > 16 );

    if( shardyard != nullptr && yardsize > 0 ) {
      freeShardYard();
    }

    yardsize   = _yardsize;
    shardnum = 0;
    processed = 0;

    shardyard = (shard_io_t**)sid_calloc( yardsize, sizeof(shard_io_t*) );

    for( int i=0;i<yardsize; i++ ) {
      shardyard[i] = (shard_io_t*)sid_calloc( 1, sizeof(shard_io_t) );
      snprintf(shardyard[i]->name, 3, "%x", (uint8_t)i );
      snprintf(shardyard[i]->filename, 255, "%s/%s", SHARDS_FOLDER, shardyard[i]->name );
      log_w("Shard %s goes in file %s", shardyard[i]->name, shardyard[i]->filename );
    }
    if( _totalbytes > 0 ) {
      totalbytes = _totalbytes;
      blocksize  = _blocksize;
      bytes_per_shard = (totalbytes)/yardsize;
      bytes_per_shard = bytes_per_shard+blocksize - (bytes_per_shard%blocksize);
      log_w("[Shard yard: total %d bytes in %d shards /w block size=%d ] ~= %d bytes per shard", totalbytes, yardsize, blocksize, bytes_per_shard );
    } else {
      log_w("[Shard yard: %d shards]", yardsize );
    }
  }

}; // end namespace SIDView
