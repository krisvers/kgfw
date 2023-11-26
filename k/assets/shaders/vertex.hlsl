struct VS_INPUT
{
	float3 pos : POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer cbuffer0 : register(b0)
{
	float4x4 mvp;
}

PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;
	output.pos = mul(mvp, float4(input.pos, 1));
	output.color = input.color;
	output.normal = input.normal;
	output.uv = input.uv;
	return output;
}