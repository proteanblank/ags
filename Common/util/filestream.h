//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================
//
//
//
//=============================================================================
#ifndef __AGS_CN_UTIL__FILESTREAM_H
#define __AGS_CN_UTIL__FILESTREAM_H

#include <stdio.h>
#include <functional>

#include "util/datastream.h"
#include "util/file.h" // TODO: extract filestream mode constants

namespace AGS
{
namespace Common
{

class FileStream : public DataStream
{
public:
    struct CloseNotifyArgs
    {
        String Filepath;
        FileWorkMode WorkMode;
    };

    // definition of function called when file closes
    typedef std::function<void(const CloseNotifyArgs &args)> FFileCloseNotify;

    static FFileCloseNotify FileCloseNotify;

    // Represents an open file object
    // The constructor may raise std::runtime_error if 
    // - there is an issue opening the file (does not exist, locked, permissions, etc)
    // - the open mode could not be determined
    FileStream(const String &file_name, FileOpenMode open_mode, FileWorkMode work_mode,
        DataEndianess stream_endianess = kLittleEndian);
    // Constructs a file stream over an open FILE handle;
    // Take an ownership over it and will close upon disposal
    static FileStream *OwnHandle(FILE *file, FileWorkMode work_mode, DataEndianess stream_end = kLittleEndian)
        { return new FileStream(file, true, work_mode, stream_end); }
    // Constructs a file stream over an open FILE handle;
    // does NOT take an ownership over it
    static FileStream *WrapHandle(FILE *file, FileWorkMode work_mode, DataEndianess stream_end = kLittleEndian)
        { return new FileStream(file, false, work_mode, stream_end); }
    ~FileStream() override;

    FileWorkMode GetWorkMode() const { return _workMode; }

    bool    HasErrors() const override;
    void    Close() override;
    bool    Flush() override;

    // Is stream valid (underlying data initialized properly)
    bool    IsValid() const override;
    // Is end of stream
    bool    EOS() const override;
    // Total length of stream (if known)
    soff_t  GetLength() const override;
    // Current position (if known)
    soff_t  GetPosition() const override;
    bool    CanRead() const override;
    bool    CanWrite() const override;
    bool    CanSeek() const override;

    size_t  Read(void *buffer, size_t size) override;
    int32_t ReadByte() override;
    size_t  Write(const void *buffer, size_t size) override;
    int32_t WriteByte(uint8_t b) override;

    bool    Seek(soff_t offset, StreamSeek origin) override;

private:
    FileStream(FILE *file, bool own, FileWorkMode work_mode, DataEndianess stream_end);
    void    Open(const String &file_name, FileOpenMode open_mode, FileWorkMode work_mode);

    FILE                *_file;
    bool                 _ownHandle;
    const FileOpenMode  _openMode;
    const FileWorkMode  _workMode;
};

} // namespace Common
} // namespace AGS

#endif // __AGS_CN_UTIL__FILESTREAM_H
