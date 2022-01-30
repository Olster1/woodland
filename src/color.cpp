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
};

//alpha is at 24 place
static inline float4 color_hexARGBTo01(unsigned int color) {
    float4 result = {};
    
    result.x = (float)((color >> 16) & 0xFF) / 255.0f; //red
    result.y = (float)((color >> 8) & 0xFF) / 255.0f;
    result.z = (float)((color >> 0) & 0xFF) / 255.0f;
    result.w = (float)((color >> 24) & 0xFF) / 255.0f;
    return result;
}