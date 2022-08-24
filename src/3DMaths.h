#include <float.h> //NOTE: For FLT_MAX

static inline float get_abs_value(float value) {
	if(value < 0) {
		value *= -1.0f;
	}
	return value;
}

struct float2
{
    float x, y;
};

struct float3
{
	union {
		struct 
		{
			float x, y, z;	
		};
		struct 
		{
			float2 xy;
			float z;	
		};
    	
	};
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

static float2 lerp_float2(float2 a, float2 b, float t) {
	return make_float2((b.x - a.x)*t + a.x, (b.y - a.y)*t + a.y);
}

static bool are_float2_equal(float2 a, float2 b) {
	return (a.x == b.x && a.y == b.y);
}


static float3 make_float3(float x0, float y0, float z0) {
	float3 result = {};

	result.x = x0;
	result.y = y0;
	result.z = z0;

	return result;
}


static float4 make_float4(float x, float y, float z, float w) {
	float4 result = {};

	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;

	return result;
}

struct Rect2f {
	float minX;
	float minY;
	float maxX;
	float maxY;
};

static Rect2f make_rect2f(float minX, float minY, float maxX, float maxY) {
	Rect2f result = {};

	result.minX = minX;
	result.minY = minY;
	result.maxX = maxX;
	result.maxY = maxY;

	return result; 
}

static Rect2f make_rect2f_center_dim(float2 centre, float2 dim) {
	Rect2f result = {};

	result.minX = centre.x - 0.5f*dim.x;
	result.minY = centre.y - 0.5f*dim.y;
	result.maxX = centre.x + 0.5f*dim.x;
	result.maxY = centre.y + 0.5f*dim.y;

	return result; 
}

static float2 get_centre_rect2f(Rect2f r) {
	float2 result = {};

	result.x = 0.5f*(r.maxX - r.minX) + r.minX;
	result.y = 0.5f*(r.maxY - r.minY) + r.minY;

	return result;
}

static float2 get_scale_rect2f(Rect2f r) {
	float2 result = {};

	result.x = (r.maxX - r.minX);
	result.y = (r.maxY - r.minY);

	return result;
}

static Rect2f rect2f_union(Rect2f a, Rect2f b) {
	Rect2f result = a;

	if(b.minX < a.minX) {
		result.minX = b.minX;
	}

	if(b.minY < a.minY) {
		result.minY = b.minY;
	}

	if(b.maxX > a.maxX) {
		result.maxX = b.maxX;
	}

	if(b.maxY > a.maxY) {
		result.maxY = b.maxY;
	}

	return result;
}

struct float16
{
    float E[16];
}; 

#define MATH_3D_NEAR_CLIP_PlANE 0.1f
#define MATH_3D_FAR_CLIP_PlANE 1000.0f

static float16 make_ortho_matrix_bottom_left_corner(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = -1; //NOTE: Defined in NDC space
	float originOffsetY = -1; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}

static float16 make_ortho_matrix_top_left_corner(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = -1; //NOTE: Defined in NDC space
	float originOffsetY = 1; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}

static float16 make_ortho_matrix_origin_center(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = 0; //NOTE: Defined in NDC space
	float originOffsetY = 0; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}


static bool in_rect2f_bounds(Rect2f bounds, float2 point) {
	bool result = false;

	if(point.x >= bounds.minX && point.x < bounds.maxX && point.y >= bounds.minY && point.y < bounds.maxY) {
		result = true;
	}

	return result;
}