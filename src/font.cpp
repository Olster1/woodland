
typedef struct {
    void *handle;

    float4 uvCoords;
    u32 unicodePoint;

    int xoffset;
    int yoffset;

    int width;
    int height;

    bool hasTexture;

    u8 *sdfBitmap;

} GlyphInfo;

#define MY_MAX_GLYPH_COUNT 256

typedef struct FontSheet FontSheet;
typedef struct FontSheet {
    void *handle; //NOTE: Platform agnostic

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

    //NOTE: This could go in permanent memory arena
    FontSheet *sheet = (FontSheet *)platform_alloc_memory(sizeof(FontSheet), true);
    sheet->minText = firstChar;
    sheet->maxText = endChar;
    sheet->next = 0;
    sheet->glyphCount = 0;
    
    void *contents = 0;
    size_t contentsSize = 0; 
    Platform_LoadEntireFile_utf8(font->fileName, &contents, &contentsSize);

    //NOTE(ollie): This stores all the info about the font
    stbtt_fontinfo *fontInfo = (stbtt_fontinfo *)platform_alloc_memory(sizeof(stbtt_fontinfo), true);
    
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

    font->fontHeight = maxHeightForFontInPixels;
    
    ///////////////////////************ Kerning Table *************////////////////////
    // //NOTE(ollie): Get the kerning table length i.e. number of entries
    // int  kerningTableLength = stbtt_GetKerningTableLength(fontInfo);
    
    // //NOTE(ollie): Allocate kerning table
    // stbtt_kerningentry* kerningTable = pushArray(&globalPerFrameArena, kerningTableLength, stbtt_kerningentry);
    
    // //NOTE(ollie): Fill out the table
    // stbtt_GetKerningTable(fontInfo, kerningTable, kerningTableLength);

    u32 maxWidth = 0;
    u32 maxHeight = 0;
    u32 totalWidth = 0;
    u32 totalHeight = 0;

    u32 counAt = 0;
    u32 rowCount = 1;
   
    for(s32 codeIndex = firstChar; codeIndex < endChar; ++codeIndex) {
        
        int width;
        int height;
        int xoffset; 
        int yoffset;

        assert((endChar - firstChar) <= 256);
        
        u8 *data = stbtt_GetCodepointSDF(fontInfo, scale, codeIndex, padding, onedge_value, pixel_dist_scale, &width, &height, &xoffset, &yoffset);    
            
        assert(sheet->glyphCount < MY_MAX_GLYPH_COUNT);
        GlyphInfo *info = &sheet->glyphs[sheet->glyphCount++];

        memset(info, 0, sizeof(GlyphInfo));

        info->unicodePoint = codeIndex;

        

            info->sdfBitmap = data;

            info->xoffset = xoffset;
            info->yoffset = yoffset;
            info->hasTexture = data;

            // char string[256];
            // sprintf(string, "%c: %d\n", codeIndex, info->hasTexture);
            // OutputDebugStringA((char *)string);

            info->width = width;
            info->height = height;
            // info->aspectRatio_h_over_w = height / width;
        if(data) {
            { 
                totalWidth += width;
                counAt++;

                if(height > maxHeight) {
                    maxHeight = height;
                }
            } 

            if((counAt % 16) == 0) {
                rowCount++;
                counAt = 0;
                if(totalWidth > maxWidth) { maxWidth = totalWidth; }
                totalWidth = 0;
            }
        }
    }

        
    totalWidth = maxWidth;
    totalHeight = maxHeight*rowCount;
    u32 *sdfBitmap_32 = (u32 *)platform_alloc_memory(totalWidth*totalHeight*sizeof(u32), true);

    u32 xAt = 0;
    u32 yAt = 0;

    u32 countAt = 0;

    for(int i = 0; i < sheet->glyphCount; ++i) {
        GlyphInfo *info = &sheet->glyphs[i];

        if(info->sdfBitmap) {

            if(info->unicodePoint == 'w') {
                int b = 0;
            }
            

            //NOTE: Calculate uv coords
            info->uvCoords = make_float4((float)xAt / (float)totalWidth, (float)yAt / (float)totalHeight, (float)(xAt + info->width) / (float)totalWidth, (float)(yAt + info->height) / (float)totalHeight);

            //NOTE(ollie): Blow out bitmap to 32bits per pixel instead of 8 so it's easier to load to the GPU
            for(int y = 0; y < info->height; ++y) {
                for(int x = 0; x < info->width; ++x) {
                    u32 stride = info->width*1;
                    u32 alpha = (u32)info->sdfBitmap[y*stride + x];
                    // sdfBitmap_32[(y + yAt)*totalWidth + (x + xAt)] = 0x00000000 | (u32)(((u32)alpha) << 24);
                    sdfBitmap_32[(y + yAt)*totalWidth + (x + xAt)] = 0x00000000 | (u32)((alpha) << 24);
                }
            }

            xAt += info->width;

            
            countAt++;

            if((countAt % 16) == 0) {
                yAt += maxHeight;
                xAt = 0;
            }

            assert(xAt < totalWidth);
            assert(yAt < totalHeight);


            //NOTE(ollie): Free the bitmap data
            stbtt_FreeSDF(info->sdfBitmap, 0);
        }
    }

    //NOTE Test purposes
    // for(int y = 0; y < totalHeight; ++y) {
    //     for(int x = 0; x < totalWidth; ++x) {
    //         sdfBitmap_32[y*totalWidth + x] = 0xFFF0FFF0;// | (u32)(((u32)alpha) << 24);
    //     }
    // }
    
    //NOTE(ollie): Load the texture to the GPU
    sheet->handle = Platform_loadTextureToGPU(sdfBitmap_32, totalWidth, totalHeight, 4);

    //NOTE(ollie): Release memory from 32bit bitmap
    platform_free_memory(sdfBitmap_32);
    platform_free_memory(contents);
    platform_free_memory(fontInfo);

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

        glyph.handle = sheet->handle;

    }
    return glyph;
}

struct DrawTextDetails {
    float2 cursorP;
    float2 size;
};

//NOTE: Returns cursor position and width
static DrawTextDetails draw_text(Renderer *renderer, Font *font, char *str, float startX, float yAt, float fontScale, float4 font_color, int cursor_in_bytes = 0) {
    // float yAt = -0.5f*font->fontHeight*fontScale + yAt_;
    DrawTextDetails result = {};

    bool newLine = true;

    float xAt = startX;

    char *at = str;

    float startY = yAt;

    //NOTE: cursor position default
    float2 cursorPosition = make_float2(xAt, yAt);

    while(*at) {

        u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);

        float factor = 1.0f;

        GlyphInfo g = easyFont_getGlyph(font, rune);

        assert(g.unicodePoint == rune);

        if(rune == ' ') {
            g.width = easyFont_getGlyph(font, 'y').width;
        }

        if(g.hasTexture) {


            float4 color = font_color;//make_float4(0, 0, 0, 1);
            float2 scale = make_float2(g.width*fontScale, g.height*fontScale);

            if(newLine) {
                xAt = 0.6f*scale.x + startX;
            }

            float offsetY = -0.5f*scale.y;

            float3 pos = {};
            pos.x = xAt + fontScale*g.xoffset;
            pos.y = yAt + -fontScale*g.yoffset + offsetY;
            pos.z = 1.0f;
            pushGlyph(renderer, g.handle, pos, scale, font_color, g.uvCoords);
        }

        xAt += (g.width + g.xoffset)*fontScale*factor;

        newLine = false;
        
        if((at - str) == cursor_in_bytes) {
            cursorPosition = make_float2(xAt, yAt);
        }
    }

    if((at - str) == cursor_in_bytes) {
        cursorPosition = make_float2(xAt, yAt);
    }
    result.cursorP = cursorPosition;
    result.size = make_float2(xAt - startX, (yAt - startY) + font->fontHeight*fontScale);

    return result;
}

static float font_getStringDimensions(Renderer *renderer, Font *font, char *str) {
    float xAt = 0;

    char *at = str;
    while(*at) {

        u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);

        GlyphInfo g = easyFont_getGlyph(font, rune);

        assert(g.unicodePoint == rune);

        if(rune == ' ') {
            g.width = easyFont_getGlyph(font, 'y').width;
        }

        xAt += (g.width + g.xoffset);

    }

    return (xAt); 
}