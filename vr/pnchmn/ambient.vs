struct VS_INPUT
{
	float3 m_Position : POSITION;
	float4 m_TexCoord : TEXCOORD;
	float3 m_Normal : NORMAL;
	float3 m_Tangent : TANGENT;
	float3 m_Binormal : BINORMAL;
};

struct VS_OUTPUT
{
	float2 m_TexCoord : TEXCOORD0;
	float4 m_Position : SV_POSITION;
};

cbuffer TransInfo : register(b0)
{
	uniform float4x4 u_World;
	uniform float4x4 u_VP;
}

VS_OUTPUT vs_main(VS_INPUT a_Input)
{
	VS_OUTPUT l_Output = (VS_OUTPUT)0;
	l_Output.m_Position = mul(float4(a_Input.m_Position, 1.0), u_World);
	l_Output.m_Position = mul(float4(l_Output.m_Position.xyz, 1.0), u_VP);
	l_Output.m_TexCoord = a_Input.m_TexCoord.xy;
	return l_Output;
}