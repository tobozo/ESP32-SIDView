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

#define _SID_ARCHIVE_MANAGER_H_

#include "../../config.h"


#if defined SID_DOWNLOAD_ARCHIVES

  #include "web.h"


  class SID_Archive
  {

    public:

      char *sidRealPath;
      char *tgzFileName;

      ~SID_Archive();
      SID_Archive( MD5FileConfig *c);

      bool check();
      bool expand();
      bool exists();
      bool isExpanded();
      bool hasCache();
      #ifdef _SID_HVSC_DOWNLOADER_H_
      bool download();
      #endif

    private:

      MD5FileConfig *cfg;
      char          *cachePath;

      void setExpanded( bool toggle = true );

  };



  class SID_Archive_Checker
  {

    public:

      SID_Archive_Checker( MD5FileConfig *c );

      void checkArchive();
      bool checkGzFile( bool force = false );
      bool checkGzExpand( bool force = false );
      bool checkMd5File( bool force = false );
      #ifdef _SID_HVSC_DOWNLOADER_H_
      bool wifi_occured = false;
      #endif

    private:
      MD5FileConfig *cfg;
      SID_Archive *archive = nullptr;

  };


#endif // #if defined SID_DOWNLOAD_ARCHIVES


