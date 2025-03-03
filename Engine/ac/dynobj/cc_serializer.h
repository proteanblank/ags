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

#ifndef __AC_SERIALIZER_H
#define __AC_SERIALIZER_H

#include "ac/dynobj/cc_scriptobject.h"

struct AGSDeSerializer : ICCObjectCollectionReader {

    void Unserialize(int index, const char *objectType, const char *serializedData, int dataSize) override;
};

extern AGSDeSerializer ccUnserializer;

#endif // __AC_SERIALIZER_H