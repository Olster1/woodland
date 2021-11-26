struct float2
{
    float x, y;
};

struct float4
{
    float x, y, z, w;
}; 

static float4 make_float4(float x0, float y0, float x1, float y1) {
	float4 result = {};

	result.x = x0;
	result.x = y0;
	result.z = x1;
	result.w = y1;

	return result;
}

struct float16
{
    float E[16];
}; 