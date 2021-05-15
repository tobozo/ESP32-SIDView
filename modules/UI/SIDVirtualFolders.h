


typedef struct
{
  String name;
  String path;
  folderItemType_t type;
} folderTypeItem_t;
//std::vector<folderTypeItem_t> myFolder;

/*
bool myFolderSorter(const folderTypeItem_t &a, const folderTypeItem_t &b)
{
  if( a.name == b.name )
    return true;
  if( b.type == F_PARENT_FOLDER ) return false; // hoist ".."
  if( b.type == F_FOLDER && a.type == F_PARENT_FOLDER ) return true;
  if( b.type == F_FOLDER ) return false; // hoist playlist files
  if( a.type == F_FOLDER ) return true; // hoist playlist files
  // TODO: sort F_SUBFOLDER and F_SUBFOLDER_NOCACHE separately from F_SID_FILE
  uint8_t al = a.name.length(), bl = b.name.length(), max = 0;
  const char *as = a.name.c_str(), *bs = b.name.c_str();
  bool ret = false;
  if( al >= bl ) {
    max = bl;
    ret = false;
  } else {
    max = al;
    ret = true;
  }
  for( uint8_t i=0;i<max;i++ ) {
    if( as[i] != bs[i] ) return as[i] < bs[i];
  }
  return ret;
}
*/




struct VirtualFolder
{
  VirtualFolder() { };
  size_t _size = 0;
  std::vector<folderTypeItem_t> folder;
  size_t size()
  {
    return _size;
  }
  inline void clear();
  inline folderTypeItem_t get( int index );
  inline void push_back( folderTypeItem_t item );
  inline std::vector<folderTypeItem_t>::iterator begin();
  inline std::vector<folderTypeItem_t>::iterator end();
};


struct VirtualFolderNoRam : public VirtualFolder
{

  const char* folderName;
  SongCacheManager* songCache;

  VirtualFolderNoRam( SongCacheManager* cache ) : songCache(cache) { }

  void clear()
  {
    //std::vector<folderTypeItem_t>().swap(folder);
    folder.clear();
    _size = 0;
  }
  folderTypeItem_t get( int index )
  {
    if( index<0 || index+1 > folder.size() ) {
      log_e("Out of bounds query (requesting index %d vs %d), returning first item instead", index, folder.size()-1 );
      songCache->deleteCache( folderName );
      return folder[0];
    }
    return folder[index];
  }
  void push_back( folderTypeItem_t item )
  {
    folder.push_back( item );
    _size = folder.size();
  }
  std::vector<folderTypeItem_t>::iterator begin()
  {
    return folder.begin();
  }
  std::vector<folderTypeItem_t>::iterator end()
  {
    return folder.end();
  }
};


VirtualFolderNoRam *myVirtualFolder = nullptr;

#if 0

static void VirtualFolderNoRamPrintCb( const char* fullpath, folderItemType_t type );
static bool VirtualFolderNoRamTickerCb( const char* title, size_t totalitems );


struct VirtualFolderNoRam : public VirtualFolder
{

  size_t totalitems = 0;
  size_t maxitems = 0; // sync with [itemsPerPage]
  size_t offset = 0; // sync with [pageStart]
  const char* folderName;

  VirtualFolderNoRam( size_t _maxitems ) : maxitems(_maxitems) { }

  size_t size()
  {
    return totalitems;
  }

  void setFolderName( const char* name )
  {
    folderName = name;
    // TODO: load folder
  }

  void clear()
  {
    folder.clear();
    totalitems = 0;
  }

  folderTypeItem_t get( int index )
  {
    if( index >= offset && index < offset + maxitems ) {
      return folder[index-offset];
    }
    clear();
    // TODO: retrieve [maxitems] from SongCache
    /*
    if( !SongCache->scanPaginatedFolder( folderName, &VirtualFolderNoRamPrintCb, &VirtualFolderNoRamTickerCb, offset, maxitems ) ) {
      log_e("[%d] Failed to scan paginated folder %s for offset %d (%d items)", ESP.getFreeHeap(), folderName, offset, maxitems );
      push_back( { String(".."), "/", F_PARENT_FOLDER } );
      return folder[0];
    }
    */
    return folder[index-offset];
  }

  void push_back( folderTypeItem_t item )
  {
    folder.push_back( item );
    _size = folder.size();
  }

  std::vector<folderTypeItem_t>::iterator begin()
  {
    return folder.begin();
  }

  std::vector<folderTypeItem_t>::iterator end()
  {
    return folder.end();
  }
};


VirtualFolderNoRam *myVirtualFolderNoRam = new VirtualFolderNoRam( 8 );



static void VirtualFolderNoRamPrintCb( const char* fullpath, folderItemType_t type )
{
  String basepath = gnu_basename( fullpath );
  switch( type )
  {
    case F_FOLDER:            log_w("[%d] Playlist Found [%s] @ %s/.sidcache", ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
    case F_SUBFOLDER:         log_w("[%d] Sublist Found [%s] @ %s/.sidcache",  ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
    case F_SUBFOLDER_NOCACHE: log_w("[%d] Subfolder Found :%s @ %s",           ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
    case F_PARENT_FOLDER:     log_w("[%d] Parent folder: %s @ %s",             ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
    case F_SID_FILE:          log_w("[%d] SID File: %s @ %s",                  ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
    case F_TOOL_CB:           log_w("[%d] Tool file: %s @ %s",                 ESP.getFreeHeap(), basepath.c_str(), fullpath ); break;
  }
  myVirtualFolderNoRam->push_back( { basepath, String(fullpath), type } );
}


static bool VirtualFolderNoRamTickerCb( const char* title, size_t totalitems )
{
  // not really a progress function but can show
  // some "activity" during slow folder scans

  if( totalitems > myVirtualFolderNoRam->maxitems ) {
    log_e("[%d] Too many items", ESP.getFreeHeap() );
    return false;
  }
  myVirtualFolderNoRam->totalitems++;
  //myVirtualFolderNoRam->totalitems = totalitems;
  log_w("[%d] %s", totalitems, title);
  return true; // continue signal, send false to stop and break the scan loop
}



#endif
