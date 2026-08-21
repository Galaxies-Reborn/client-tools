//hlsl vs_1_1 vs_2_0

#define textureCoordinateSetMAIN	textureCoordinateSet0
#define DECLARE_textureCoordinateSets	\
	float2 textureCoordinateSet0 : TEXCOORD0;

#include "vertex_program/include/asm_constants.inc"

#define objectWorldCameraProjectionMatrix float4x4(c[0], c[1], c[2], c[3])

float4 precuTransform3d(float4 position_o)
{
	return mul(position_o, float4x4(c[0], c[1], c[2], c[3]));
}

float precuCalculateFog(float4 position_o)
{
	float4 position_w = mul(position_o, float4x4(c[4], c[5], c[6], c[7]));
	float3 viewer_w = c[8].xyz - position_w.xyz;
	return 1.0f / exp(dot(viewer_w, viewer_w) * c[10].w);
}

#define transform3d precuTransform3d
#define calculateFog precuCalculateFog


struct Input
{
	float4  position              : POSITION0;
	float4  diffuse               : COLOR0;
	DECLARE_textureCoordinateSets
};

struct Output
{
	float4  position              : POSITION0;
	float4  diffuse               : COLOR0;
	float   fog                   : FOG;
	float2  textureCoordinateSet0 : TEXCOORD0;
};

Output main(Input vertex)
{
	Output output;

	// transform vertex
	output.position = mul(vertex.position, objectWorldCameraProjectionMatrix);

	// copy lighting
	output.diffuse = vertex.diffuse;

	// copy lighting
	output.textureCoordinateSet0 = vertex.textureCoordinateSetMAIN;

	// calculate fog
	output.fog = pow(1.0f - output.textureCoordinateSet0.y, 6);

	return output;
}
