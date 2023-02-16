#pragma once

#include "../config.h"

#if !defined SID_DOWNLOAD_ARCHIVES

  #include <FS.h>


  namespace SIDView
  {


    __attribute__((unused))
    static char *_memrchr(const char *m, int c, size_t n)
    {
      const char *s = m;
      c = (char)c;
      while (n--) if (s[n]==c) return (char *)(s+n);
      return 0;
    }


    __attribute__((unused))
    static char *dirname(char *path)
    {
      static const char dot[] = ".";
      char *last_slash;
      /* Find last '/'.  */
      last_slash = path != NULL ? strrchr (path, '/') : NULL;
      if (last_slash != NULL && last_slash != path && last_slash[1] == '\0') {
        /* Determine whether all remaining characters are slashes.  */
        char *runp;
        for (runp = last_slash; runp != path; --runp)
          if (runp[-1] != '/')
            break;
        /* The '/' is the last character, we have to look further.  */
        if (runp != path)
          last_slash = (char*)_memrchr(path, '/', runp - path);
      }
      if (last_slash != NULL) {
        /* Determine whether all remaining characters are slashes.  */
        char *runp;
        for (runp = last_slash; runp != path; --runp)
          if (runp[-1] != '/')
            break;
        /* Terminate the path.  */
        if (runp == path) {
          /* The last slash is the first character in the string.  We have to
              return "/".  As a special case we have to return "//" if there
              are exactly two slashes at the beginning of the string.  See
              XBD 4.10 Path Name Resolution for more information.  */
          if (last_slash == path + 1)
            ++last_slash;
          else
            last_slash = path + 1;
        } else
          last_slash = runp;
        last_slash[0] = '\0';
      } else
        /* This assignment is ill-designed but the XPG specs require to
          return a string containing "." in any case no directory part is
          found and so a static and constant string is required.  */
        path = (char *) dot;
      return path;
    }
    #ifndef strdupa // defined in sdk 2.0
      #define strdupa(a) strcpy((char*)alloca(strlen(a) + 1), a)
    #endif
    // create traversing directories from a path
    __attribute__((unused))
    static int mkpath(fs::FS *fs, char *dir)
    {
      if (!dir) return 1;
      if (strlen(dir) == 1 && dir[0] == '/') return 0;
      mkpath(fs, dirname(strdupa(dir)));
      return fs->mkdir( dir );
    }


    // create traversing directories from a file name
    __attribute__((unused))
    static void mkdirp( fs::FS *fs, const char* tempFile )
    {
      if( fs->exists( tempFile ) ) {
        log_v("Destination file already exists, no need to create subfolders");
        return; // no need to create folder if the file is there
      }

      char tmp_path[256] = {0};
      snprintf( tmp_path, 256, "%s", tempFile );

      for( size_t i=0;i<strlen(tmp_path);i++ ) {
        if( !isPrintable( tmp_path[i] ) ) {
          log_w("Non printable char detected in path at offset %d, setting null", i);
          tmp_path[i] = '\0';
        }
      }

      char *dir_name = dirname( tmp_path );

      if( !fs->exists( dir_name ) ) {
        log_v("Creating %s folder for path %s", dir_name, tmp_path);
        mkpath( fs, dir_name );
      } else {
        log_v("Folder %s already exists for path %s", dir_name, tmp_path);
      }
      //delete tmp_path;
    }

  }; // end namespace SIDView

#endif
