struct float2
{
    float x, y;
};

struct float4
{
    float x, y, z, w;
}; 

static float2 make_float2(float x0, float y0) {
	float2 result = {};

	result.x = x0;
	result.y = y0;

	return result;
}

static float4 make_float4(float x0, float y0, float z0, float w0) {
	float4 result = {};

	result.x = x0;
	result.y = y0;
	result.z = z0;
	result.w = w0;

	return result;
}

struct float16
{
    float E[16];
}; 