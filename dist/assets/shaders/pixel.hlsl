struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

sampler sample0 : register(s0);
Texture2D<float4> texture0 : register(t0);

float4 ps(PS_INPUT input) : SV_TARGET
{
	float4 col = texture0.Sample(sample0, input.uv);
	if (all(col == float4(0, 0, 0, 0)))
	{
		col = float4((input.normal + 1) / 2, 1);
	}
	
	return col;
}