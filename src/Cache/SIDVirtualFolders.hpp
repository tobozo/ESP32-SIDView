#pragma once
#include "../config.h"

#include "../helpers/types.hpp"
#include "../Cache/SIDCacheManager.hpp"


namespace SIDView
{


  // default folder item (root folder)
  const folderTypeItem_t rootFolder =
  {
    "root",
    "/",
    F_SUBFOLDER
  };

  const folderTypeItem_t nullFolder =
  {
    "null",
    "/",
    F_UNSUPPORTED_FILE
  };


  struct VirtualFolder
  {
    VirtualFolder() { };
    size_t _size = 0;
    size_t _actionitem_size=0;
    size_t _listitem_size=0;
    std::vector<folderTypeItem_t> folder;
    size_t size() { return _size; }
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
    void clear();
    folderTypeItem_t get( int index );
    void push_back( folderTypeItem_t item );
    std::vector<folderTypeItem_t>::iterator begin() { return folder.begin(); }
    std::vector<folderTypeItem_t>::iterator end() { return folder.end(); }
  };


  class FolderNav
  {
    public:
      FolderNav(VirtualFolderNoRam *i, uint16_t per_page ) : items(i), itemsPerPage(per_page) { }
      VirtualFolderNoRam *items = nullptr; // virtual folder
      const uint16_t itemsPerPage;
      char path[256]         = {0}; // full path to selected item
      char dir[256]          = {0}; // folder path where item resides
      uint16_t pos           = 0;   // item position in folder list
      uint16_t page_num      = 0;   // page number
      uint16_t last_page_num = -1;  // previous page number
      folderItemType_t type;        // item type
      uint16_t pages_count   = 0;   // pages count
      uint16_t start         = 0;   // page start

      void clearFolder();
      SongCacheManager* songCache();
      void pushItem( folderTypeItem_t item );
      void pushActionItem( folderTypeItem_t item );
      void pushListItem( folderTypeItem_t item );
      folderTypeItem_t getPagedItem( uint16_t item_id );
      folderTypeItem_t parentFolder( String label );
      size_t depth();
      size_t size();
      size_t pageStart();
      size_t pageEnd();
      void setPath( const char* p );
      void setDir( const char* d );
      void cdRoot() { setPath("/"); setDir("/"); }
      void prev();
      void next();
      bool nav_changed();
      bool page_changed();
      void freeze();
      void thaw();
    private:

      bool frozen = false;
      char _path[256] = {0};
      char _dir[256]  = {0};
      uint16_t _pos;
      uint16_t _page_num;
      uint16_t _last_page_num;
      uint16_t _pages_count;
      uint16_t _start;
      folderItemType_t _type;

  };


  FolderNav* getFolderNav();
  extern FolderNav *Nav;


}; // end namespace SIDView
