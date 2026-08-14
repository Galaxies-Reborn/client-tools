//hlsl vs_1_1

#define textureCoordinateSetMAIN textureCoordinateSet0
#define DECLARE_textureCoordinateSets \
	float2 textureCoordinateSet0 : TEXCOORD0; \
	float2 textureCoordinateSet1 : TEXCOORD1; \
	float2 textureCoordinateSet2 : TEXCOORD2;

#include "vertex_program/include/vertex_shader_constants.inc"
#include "vertex_program/include/functions.inc"

struct InputVertex
{
	float4 position : POSITION0;
	float4 normal : NORMAL0;
	DECLARE_textureCoordinateSets
};

struct OutputVertex
{
	float4 position : POSITION0;
	float4 diffuse : COLOR0;
	float fog : FOG;
	float2 textureCoordinateSet0 : TEXCOORD0;
	float2 textureCoordinateSet1 : TEXCOORD1;
	float2 textureCoordinateSet2 : TEXCOORD2;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	outputVertex.position = transform3d(inputVertex.position);
	outputVertex.fog = calculateFog(inputVertex.position);
	outputVertex.diffuse = lightData.ambient.ambientColor;
	outputVertex.diffuse = calculateDiffuseLighting(false, inputVertex.position, inputVertex.normal);
	outputVertex.diffuse = saturate(outputVertex.diffuse * 2);

	outputVertex.textureCoordinateSet0 = inputVertex.textureCoordinateSetMAIN;
	outputVertex.textureCoordinateSet1 = inputVertex.textureCoordinateSetMAIN * 2.0f;
	outputVertex.textureCoordinateSet2 = inputVertex.textureCoordinateSetMAIN * 0.125f;

	return outputVertex;
}
