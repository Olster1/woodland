struct Editor_Color_Palette
{
	float4 background;
	float4 standard;
	float4 variable;
	float4 bracket;
	float4 function;
	float4 keyword;
	float4 comment;
	float4 preprocessor;
	float4 string;
};

struct Color_Palettes {
	Editor_Color_Palette witness;
	Editor_Color_Palette handmade;

};
static inline float4 color_rgb255_to_01(float r, float g, float b) {
    float4 result = {};
    
    result.x = r / 255.0f; //red
    result.y = g / 255.0f;
    result.z = b / 255.0f;
    result.w = 1;
    return result;
}



//alpha is at 24 place
static inline float4 color_hexARGBTo01(unsigned int color) {
    float4 result = {};
    
    result.x = (float)((color >> 16) & 0xFF) / 255.0f; //red
    result.y = (float)((color >> 8) & 0xFF) / 255.0f;
    result.z = (float)((color >> 0) & 0xFF) / 255.0f;
    result.w = (float)((color >> 24) & 0xFF) / 255.0f;
    return result;
}

 

static Color_Palettes init_color_palettes() {

	Color_Palettes result = {};

	result.handmade.background = color_hexARGBTo01(0xFF161616);
	result.handmade.standard =  color_hexARGBTo01(0xFFA08563);
	result.handmade.variable = color_hexARGBTo01(0xFF6B8E23);
	result.handmade.bracket = color_hexARGBTo01(0xFFDAB98F);
	result.handmade.function = color_hexARGBTo01(0xFF008563);
	result.handmade.keyword = color_hexARGBTo01(0xFFCD950C);
	result.handmade.comment = color_hexARGBTo01(0xFF7D7D7D);
	result.handmade.preprocessor = color_hexARGBTo01(0xFFDAB98F);
	result.handmade.string = color_hexARGBTo01(0xFFDAB98F);


	result.witness.background = color_rgb255_to_01(6, 38, 38);
	result.witness.standard =  color_rgb255_to_01(194, 205, 187);
	result.witness.variable = color_rgb255_to_01(139, 194, 186);
	result.witness.bracket = color_rgb255_to_01(194, 205, 187);
	result.witness.function = color_rgb255_to_01(194, 205, 187);
	result.witness.keyword = color_rgb255_to_01(176, 200, 202);
	result.witness.comment = color_rgb255_to_01(92, 175, 87);
	result.witness.preprocessor = color_rgb255_to_01(118, 170, 138);
	result.witness.string = color_rgb255_to_01(118, 170, 138);


	return result;

}

enum EditorColorType {
	HANDMADE_HERO,
	THE_WITNESS
};



