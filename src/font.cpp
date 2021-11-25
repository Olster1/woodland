
typedef struct {
    u32 textureHandle;
    float4 uvCoords;
    u32 unicodePoint;

    int xoffset;
    int yoffset;

    bool hasTexture;

} GlyphInfo;

#define MY_MAX_GLYPH_COUNT 256

typedef struct FontSheet FontSheet;
typedef struct FontSheet {
    // GLuint handle;

    u32 glyphCount;
    GlyphInfo glyphs[MY_MAX_GLYPH_COUNT];

    u32 minText;
    u32 maxText;

    FontSheet *next;

} FontSheet;

typedef struct {
    char *fileName;
    int fontHeight;
    FontSheet *sheets;
} Font;


FontSheet *createFontSheet(Font *font, u32 firstChar, u32 endChar) {

    FontSheet *sheet = (FontSheet *)Win32HeapAlloc(sizeof(FontSheet), true);
    sheet->minText = firstChar;
    sheet->maxText = endChar;
    sheet->next = 0;
    sheet->glyphCount = 0;
    
    void *contents = 0;
    size_t contentsSize = 0; 
    Platform_LoadEntireFile_utf8(font->fileName, &contents, &contentsSize);

    //NOTE(ollie): This stores all the info about the font
    stbtt_fontinfo *fontInfo = (stbtt_fontinfo *)Win32HeapAlloc(sizeof(stbtt_fontinfo), true);
    
    //NOTE(ollie): Fill out the font info
    stbtt_InitFont(fontInfo, (const unsigned char *)contents, 0);
    
    //NOTE(ollie): Get the 'scale' for the max pixel height 
    float maxHeightForFontInPixels = 32;//pixels
    float scale = stbtt_ScaleForPixelHeight(fontInfo, maxHeightForFontInPixels);
    
    //NOTE(ollie): Scale the padding around the glyph proportional to the size of the glyph
    s32 padding = (s32)(maxHeightForFontInPixels / 3);
    //NOTE(ollie): The distance from the glyph center that determines the edge (i.e. all the 'in' pixels)
    u8 onedge_value = (u8)(0.8f*255); 
    //NOTE(ollie): The rate at which the distance from the center should increase
    float pixel_dist_scale = (float)onedge_value/(float)padding;

    font->fontHeight = maxHeightForFontInPixels + (2*padding);
    
    ///////////////////////************ Kerning Table *************////////////////////
    // //NOTE(ollie): Get the kerning table length i.e. number of entries
    // int  kerningTableLength = stbtt_GetKerningTableLength(fontInfo);
    
    // //NOTE(ollie): Allocate kerning table
    // stbtt_kerningentry* kerningTable = pushArray(&globalPerFrameArena, kerningTableLength, stbtt_kerningentry);
    
    // //NOTE(ollie): Fill out the table
    // stbtt_GetKerningTable(fontInfo, kerningTable, kerningTableLength);
    
    for(s32 codeIndex = firstChar; codeIndex <= endChar; ++codeIndex) {
        
        int width;
        int height;
        int xoffset; 
        int yoffset;
        
        u8 *sdfBitmap = stbtt_GetCodepointSDF(fontInfo, scale, codeIndex, padding, onedge_value, pixel_dist_scale, &width, &height, &xoffset, &yoffset);    
        
        if(width > 0 && height > 0) {
                
            assert(sheet->glyphCount < MY_MAX_GLYPH_COUNT);
            GlyphInfo *info = &sheet->glyphs[sheet->glyphCount++];

            // info->textureHandle;
            // info->uvCoords;
            info->unicodePoint = codeIndex;

            info->xoffset = xoffset;
            info->yoffset = yoffset;
            info->hasTexture = sdfBitmap;

            // elm.tex = {};
            // elm.tex.width = width;
            // elm.tex.height = height;
            // elm.tex.aspectRatio_h_over_w = height / width;
            // elm.tex.uvCoords = rect2f(0, 0, 1, 1);
            
            
            if(sdfBitmap) {

                //NOTE(ollie): Blow out bitmap to 32bits per pixel instead of 8 so it's easier to load to the GPU
                u32 *sdfBitmap_32 = (u32 *)Win32HeapAlloc(width*height*sizeof(u32), true);

                for(int y = 0; y < height; ++y) {
                    for(int x = 0; x < width; ++x) {
                        u8 alpha = sdfBitmap[y*width + x];
                        if(alpha != 0) {
                            int j = 0;
                        }
                        sdfBitmap_32[y*width + x] = 0x00000000 | (u32)(((u32)alpha) << 24);
                    }
                }
                ////////////////////////////////////////////////////////////////////
                
                //NOTE(ollie): Load the texture to the GPU
                // elm.tex.id = renderLoadTexture(width, height, sdfBitmap_32, RENDER_TEXTURE_DEFAULT);
                
                //NOTE(ollie): Release memory from 32bit bitmap
                Win32HeapFree(sdfBitmap_32);
            } 
            
        }
        
        //NOTE(ollie): Free the bitmap data
        stbtt_FreeSDF(sdfBitmap, 0);
    }
    

    
    Win32HeapFree(contents);


    // sheet->handle = renderLoadTexture(FONT_SIZE, FONT_SIZE, bitmapTexture);
    // assert(sheet->handle);

    return sheet;
}

FontSheet *findFontSheet(Font *font, unsigned int character) {

    FontSheet *sheet = font->sheets;
    FontSheet *result = 0;
    while(sheet) {
        if(character >= sheet->minText && character < sheet->maxText) {
            result = sheet;
            break;
        }
        sheet = sheet->next;
    }

    if(!result) {
        unsigned int interval = 256;
        unsigned int firstUnicode = ((int)(character / interval)) * interval;
        result = sheet = createFontSheet(font, firstUnicode, firstUnicode + interval);

        //put at the start of the list
        sheet->next = font->sheets;
        font->sheets = sheet;
    }
    assert(result);

    return result;
}

Font initFont_(char *fileName, int firstChar, int endChar) {
    Font result = {}; 
    result.fileName = fileName;

    result.sheets = createFontSheet(&result, firstChar, endChar);
    return result;
}



Font initFont(char *fileName) {
    Font result = initFont_(fileName, 0, 256); // ASCII 0..256 not inclusive
    return result;
}

static inline GlyphInfo easyFont_getGlyph(Font *font, u32 unicodePoint) {
    GlyphInfo glyph = {};

    if(unicodePoint != '\n') {
        FontSheet *sheet = findFontSheet(font, unicodePoint);
        assert(sheet);

        int indexInArray = unicodePoint - sheet->minText;

        glyph = sheet->glyphs[indexInArray];

    }
    return glyph;

}
