cbuffer constants : register(b0)
{
    float4x4 orthoMatrix;
};

struct VS_Input {
    float3 pos : POS;
    float2 uv : TEX;
    float2 pos1 : POS_INSTANCE;
    float4 color1 : COLOR_INSTANCE;
    float4 uv1 : TEXCOORD_INSTANCE;

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

    // output.pos = float4(input.pos + instance_input.pos1, 0.0f, 1.0f);
    // output.uv.x = lerp(instance_input.uv1.x, instance_input.uv1.y, input.uv.x);
    // output.uv.y = lerp(instance_input.uv1.z, instance_input.uv1.w, input.uv.y);
    output.pos = float4(input.pos.x, input.pos.y, 0, 1.0f);
    output.uv = input.uv;
    output.color = input.color1;//float4(1, 0, 0, 0);
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
