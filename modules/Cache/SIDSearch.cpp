
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
    snprintf(shardyard[i]->name, 2, "%x", i );
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
