// miniaudio's implementation, with Ogg Vorbis decoding from stb_vorbis (the order is the one
// miniaudio documents: stb_vorbis header, miniaudio, then stb_vorbis's implementation). Kept in
// its own file so their macros stay out of the rest of the app.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
