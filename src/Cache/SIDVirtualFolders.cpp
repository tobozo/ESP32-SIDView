#include "SIDVirtualFolders.hpp"


namespace SIDView
{

  FolderNav *Nav = nullptr;
  FolderNav* getFolderNav() { return Nav; }


  void VirtualFolderNoRam::clear()
  {
    //std::vector<folderTypeItem_t>().swap(folder);
    folder.clear();
    _size = 0;
    _actionitem_size=0;
    _listitem_size=0;
  }
  folderTypeItem_t VirtualFolderNoRam::get( int index )
  {
    if( index<0 || index+1 > folder.size() ) {
      //if( songCache->getCacheType()==CACHE_DEEPSID ) {
        log_e("Out of bounds query (requesting index %d vs %d) for folder '%s', serving null folder", index, folder.size()-1, folderName );
        //while(1) vTaskDelay(1);
      //} else {
        //songCache->deleteCache( folderName );
      //}
      return nullFolder;
      //return folder[0];
    }
    return folder[index];
  }
  void VirtualFolderNoRam::push_back( folderTypeItem_t item )
  {
    folder.push_back( item );
    _size = folder.size();
  }



  void FolderNav::clearFolder()
  {
    items->clear();
    items->folderName = dir;
  }

  SongCacheManager* FolderNav::songCache()
  {
    return items->songCache;
  }


  void FolderNav::pushItem( folderTypeItem_t item )
  {
    switch( item.type )
    {
      case F_PLAYLIST:          pushActionItem(item); break;
      case F_PARENT_FOLDER:     pushActionItem(item); break;
      case F_TOOL_CB:           pushActionItem(item); break;
      case F_DEEPSID_FOLDER:    pushListItem(item); break;
      case F_SUBFOLDER:         pushListItem(item); break;
      case F_SUBFOLDER_NOCACHE: pushListItem(item); break;
      case F_SID_FILE:          pushListItem(item); break;
      case F_UNSUPPORTED_FILE:  pushListItem(item); break;
      case F_SYMLINK:           pushListItem(item); break;
    }
  }


  void FolderNav::pushActionItem( folderTypeItem_t item )
  {
    if( page_num == 0 ) items->push_back( item );
    items->_actionitem_size++;
    switch( item.type )
    {
      case F_PLAYLIST:          log_d("[Count:%-3d][Playlist] %s/.sidcache : [%s]", items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_PARENT_FOLDER:     log_d("[Count:%-3d][Parent] %s : %s",               items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_TOOL_CB:           log_d("[Count:%-3d][Tool] %s : %s",                 items->size(), item.name.c_str(), item.path.c_str() ); break;
      default: break;
    }
  }

  void FolderNav::pushListItem( folderTypeItem_t item )
  {
    items->push_back( item );
    items->_listitem_size++;

    switch( item.type )
    {
      case F_DEEPSID_FOLDER:    log_d("[Count:%-3d][Deepsid folder] %s: [%s]",      items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_SUBFOLDER:         log_d("[Count:%-3d][Sublist] %s/.sidcache : [%s]",  items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_SUBFOLDER_NOCACHE: log_d("[Count:%-3d][Subfolder] %s : %s",            items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_SID_FILE:          log_d("[Count:%-3d][SID File] %s : %s",             items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_UNSUPPORTED_FILE:  log_d("[Count:%-3d][Ignored File] %s : %s",         items->size(), item.name.c_str(), item.path.c_str() ); break;
      case F_SYMLINK:           log_d("[Count:%-3d][Symlink] %s : %s",              items->size(), item.name.c_str(), item.path.c_str() ); break;
      default: break;
    }
  }

  folderTypeItem_t FolderNav::getPagedItem( uint16_t item_id )
  {
    return items->get(item_id%itemsPerPage);
  }

  folderTypeItem_t FolderNav::parentFolder( String label )
  {
    String dirname = gnu_basename( dir );
    String parentFolder = String( dir ).substring( 0, strlen( dir ) - (dirname.length()+1) );
    if( parentFolder == "" ) parentFolder = "/";
    //log_d("Pushing %s as '..' parent folder (from dir=%s, dirname=%s)", parentFolder.c_str(), dir, dirname.c_str() );
    return { label, parentFolder, F_PARENT_FOLDER };
  }

  size_t FolderNav::depth()
  {
    size_t depth = 0;
    for( int i=0; i<strlen(dir); i++ ) {
      if( dir[i] == '/' ) depth++;
    }
    return depth;
  }

  size_t FolderNav::size()
  {
    return items->size();
  }

  size_t FolderNav::pageStart()
  {
    return start;
  }

  size_t FolderNav::pageEnd()
  {
    size_t hard_end = start+itemsPerPage;
    return hard_end<items->size() ? hard_end : items->size();
  }

  void FolderNav::setPath( const char* p )
  {
    snprintf( path, 255, "%s", p );
  }

  void FolderNav::setDir( const char* d )
  {
    snprintf( dir, 255, "%s", d );
  }

  void FolderNav::prev()
  {
    if( pos > 0 ) pos--;
    else pos = items->size()-1;
  }

  void FolderNav::next()
  {
    if( pos+1 < items->size() ) pos++;
    else pos = 0;
  }

  bool FolderNav::nav_changed()
  {
    return page_num != last_page_num;
  }

  bool FolderNav::page_changed()
  {
    last_page_num = page_num; // memoize cursor position
    page_num      = pos/itemsPerPage; // recalculate cursor position
    if( items->size() > 0 ) {
      pages_count = ceil( float(items->size())/float(itemsPerPage) );
    } else {
      pages_count = 1;
    }
    start = page_num*itemsPerPage;
    return nav_changed();
  }

  void FolderNav::freeze() {
    if( frozen ) {
      log_e("Should thaw first !! Halting...");
      while(1) vTaskDelay(1);
      return;
    }
    strcpy( _path, path );
    strcpy( _dir,  dir );
    //snprintf( _path, 255, "%s", path );
    //snprintf( _dir,  255, "%s", dir );
    _pos           = pos;
    _page_num      = page_num;
    _last_page_num = last_page_num;
    _type          = type;
    _pages_count   = pages_count;
    _start         = start;
    frozen         = true;
  }

  void FolderNav::thaw() {
    if( !frozen ) {
      log_e("Nothing to thaw! Halting...");
      while(1) vTaskDelay(1);
      return;
    }
    strcpy( path, _path );
    strcpy( dir,  _dir );
    //snprintf( path, 255, "%s", _path );
    //snprintf( dir,  255, "%s", _dir );
    pos           = _pos;
    page_num      = _page_num;
    last_page_num = _last_page_num;
    type          = _type;
    pages_count   = _pages_count;
    start         = _start;
    frozen        = false;
  }

}; // end namespace SIDView
