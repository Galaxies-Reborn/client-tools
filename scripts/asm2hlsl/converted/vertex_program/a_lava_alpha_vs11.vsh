//hlsl vs_1_1

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


#define flow cMaterial_emissiveColor.rgb
#define tcScale cMaterial_specularColor.b

#define colorScale cMaterial_specularColor.r
#define colorBias cMaterial_specularColor.g

struct InputVertex
{
	float4  position  : POSITION0;
	DECLARE_textureCoordinateSets
};

struct OutputVertex
{
	float4  position  : POSITION0;
	float   fog       : FOG;
	float2  tcs_MAIN  : TEXCOORD0;
	float3  noiseTc   : TEXCOORD1;
};

OutputVertex main (InputVertex inputVertex)
{
	OutputVertex outputVertex;

	//-- setup some starting values
	float4 position    = inputVertex.position;

	//-- transform vertex
	outputVertex.position = transform3d(position);

	//-- calculate fog
	outputVertex.fog = calculateFog(position);

	// copy texture coordinates
	outputVertex.tcs_MAIN = inputVertex.textureCoordinateSetMAIN;

	//-- set up tex coords
	outputVertex.noiseTc.xzy = float3(inputVertex.textureCoordinateSetMAIN, 0) * tcScale;
	outputVertex.noiseTc += flow * fmod(cCurrentTime.x, 1000);

	return outputVertex;
}
