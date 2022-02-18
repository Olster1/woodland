        ��  ��                  �      ��
 ��     0 	        cbuffer constants : register(b0)
{
    float4x4 orthoMatrix;
};

struct VS_Input {
    float3 pos : POS;
    float2 uv : TEX;
    float3 pos1 : POS_INSTANCE;
    float2 scale1 : SCALE_INSTANCE;
    float4 color1 : COLOR_INSTANCE;
    float4 uv1 : TEXCOORD_INSTANCE;
    float texture_array_index: TEX_ARRAY_INDEX;

};

struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    float texture_array_index: TEX_ARRAY_INDEX;
};

Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);


VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    
    float3 pos = input.pos;

    pos.x *= input.scale1.x;
    pos.y *= input.scale1.y;

    pos += input.pos1;

    output.pos = mul(orthoMatrix, float4(pos, 1.0f));

    // output.uv = input.uv;
    output.uv.x = lerp(input.uv1.x, input.uv1.z, input.uv.x);
    output.uv.y = lerp(input.uv1.y, input.uv1.w, input.uv.y);

    output.color = input.color1;//float4(1, 0, 0, 0);
    output.texture_array_index = input.texture_array_index;
    return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
    float smoothing = 0.1f;
    float boldness = 0.3f;
    float4 sample = mytexture.Sample(mysampler, input.uv); 

    float distance = sample.a;

    float alpha = smoothstep(1.0f - boldness, (1.0f - boldness) + smoothing, distance);

    float4 color = alpha * input.color;

    color.xyz /= color.a;

    return color;
    
}
�      ��
 ��     0 	        cbuffer constants : register(b0)
{
    float4x4 orthoMatrix;
};

struct VS_Input {
    float3 pos : POS;
    float2 uv : TEX;
    float3 pos1 : POS_INSTANCE;
    float2 scale1 : SCALE_INSTANCE;
    float4 color1 : COLOR_INSTANCE;
    float4 uv1 : TEXCOORD_INSTANCE;
    float texture_array_index: TEX_ARRAY_INDEX;

};

struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    float texture_array_index: TEX_ARRAY_INDEX;
};

Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);


VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    
    float3 pos = input.pos;

    pos.x *= input.scale1.x;
    pos.y *= input.scale1.y;

    pos += input.pos1;

    output.pos = mul(orthoMatrix, float4(pos, 1.0f));

    output.uv = input.uv;
    // output.uv.x = lerp(input.uv1.x, input.uv1.z, input.uv.x);
    // output.uv.y = lerp(input.uv1.y, input.uv1.w, input.uv.y);

    output.color = input.color1;//float4(1, 0, 0, 0);
    output.texture_array_index = input.texture_array_index;
    return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
    // float smoothing = 0.3f;
    float4 sample = mytexture.Sample(mysampler, input.uv); 

    // float alpha = result.w;
    // alpha = smoothstep(0.5f, 0.5f + smoothing, alpha);

    // float4 c = result;
    // c.a = alpha;
    // if(c.a <= 0.2f) discard;

    return sample*input.color;
    
}
  X      ��
 ��     0 	        cbuffer constants : register(b0)
{
    float4x4 orthoMatrix;
};

struct VS_Input {
    float3 pos : POS;
    float2 uv : TEX;
    float3 pos1 : POS_INSTANCE;
    float2 scale1 : SCALE_INSTANCE;
    float4 color1 : COLOR_INSTANCE;
    float4 uv1 : TEXCOORD_INSTANCE;
    float texture_array_index: TEX_ARRAY_INDEX;

};

struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    float2 scale_world_space: SCALE;
    float texture_array_index: TEX_ARRAY_INDEX;
    
};

Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);


VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    
    float3 pos = input.pos;

    pos.x *= input.scale1.x;
    pos.y *= input.scale1.y;

    pos += input.pos1;

    

    output.pos = mul(orthoMatrix, float4(pos, 1.0f));

    output.uv = input.uv;

    output.color = input.color1;//float4(1, 0, 0, 0);
    output.texture_array_index = input.texture_array_index;
    output.scale_world_space = input.scale1;



    return output;
}



float4 ps_main(VS_Output input) : SV_Target
{
    float outline_width = 2;    
    
    float alpha_value = 0.0f;

    float4 color = input.color;

    float xAt = input.uv.x*input.scale_world_space.x;
    float yAt = input.uv.y*input.scale_world_space.y;
    if(xAt < outline_width || (input.scale_world_space.x - xAt) < outline_width || yAt < outline_width || (input.scale_world_space.y - yAt) < outline_width) {
        alpha_value = 1.0f;
    }

    color.a = alpha_value;

    return color;
    
}
