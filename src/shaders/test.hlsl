cbuffer constants : register(b0)
{
    float4x4 orthoMatrix;
};

struct VS_Input {
    float3 pos : POS;
    float2 uv : TEX;
};

struct VS_Input_Instance {
    float2 pos : POS;
};

struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);


VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    output.pos = float4(input.pos, 1.0f);
    output.uv = input.uv;
    output.color = float4(1, 1, 1, 1);
    return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
    float smoothing = 0.3f;
    float4 result = mytexture.Sample(mysampler, input.uv); 

    float alpha = result.w;
    alpha = smoothstep(0.5f, 0.5f + smoothing, alpha);

    float4 c = result;
    c.a = alpha;
    if(c.a <= 0.2f) discard;

    return c*input.color;
    
}
