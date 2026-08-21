//hlsl vs_2_0

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


float3 precuTransformTerrainDot3(float3 inputDirection, float3 vertexNormal_o)
{
	float3 j = cross(vertexNormal_o, float3(1.0f, 0.0f, 0.0f));
	float3 i = cross(j, vertexNormal_o);
	return mul(float3x3(i, j, vertexNormal_o), inputDirection);
}

float3 precuTransformTerrainDot3LightDirection(float3 vertexNormal_o)
{
	return normalize(precuTransformTerrainDot3(cLightData_dot3_0_direction.xyz, vertexNormal_o));
}

float4 precuCalculateDiffusePointLight(
	float3 position_w,
	float4 diffuseColor,
	float4 attenuation,
	float3 vertexPosition_w,
	float3 normal_w,
	float attenuationW)
{
	float3 lightDirection = position_w - vertexPosition_w;
	float lightDistanceSquared = dot(lightDirection, lightDirection);
	float oneOverLightDistance = rsqrt(lightDistanceSquared);
	lightDirection *= oneOverLightDistance;
	float4 attenuationFactors = float4(
		1.0f,
		lightDistanceSquared * oneOverLightDistance,
		lightDistanceSquared,
		attenuationW);
	float distanceAttenuation = 1.0f / dot(attenuation, attenuationFactors);
	return max(dot(normal_w, lightDirection), 0.0f) * distanceAttenuation * diffuseColor;
}

float4 precuCalculateDiffuseTerrainLighting(float4 vertexPosition_o, float3 vertexNormal_o)
{
	float4x4 objectWorld = float4x4(c[4], c[5], c[6], c[7]);
	float3 vertexPosition_w = mul(vertexPosition_o, objectWorld).xyz;
	float3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorld));
	float4 result =
		max(dot(normal_w, cLightData_parallel_0_direction.xyz), 0.0f) * cLightData_parallel_0_diffuseColor
		+ max(dot(normal_w, cLightData_parallel_1_direction.xyz), 0.0f) * cLightData_parallel_1_diffuseColor;
	result += precuCalculateDiffusePointLight(
		cLightData_pointSpecular_0_position.xyz,
		cLightData_pointSpecular_0_diffuseColor,
		cLightData_pointSpecular_0_attenuation,
		vertexPosition_w,
		normal_w,
		1.0f);
	result += precuCalculateDiffusePointLight(cLightData_point_0_position.xyz, cLightData_point_0_diffuseColor, cLightData_point_0_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_0_position.xyz - vertexPosition_w, cLightData_point_0_position.xyz - vertexPosition_w)));
	result += precuCalculateDiffusePointLight(cLightData_point_1_position.xyz, cLightData_point_1_diffuseColor, cLightData_point_1_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_1_position.xyz - vertexPosition_w, cLightData_point_1_position.xyz - vertexPosition_w)));
	result += precuCalculateDiffusePointLight(cLightData_point_2_position.xyz, cLightData_point_2_diffuseColor, cLightData_point_2_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_2_position.xyz - vertexPosition_w, cLightData_point_2_position.xyz - vertexPosition_w)));
	result += precuCalculateDiffusePointLight(cLightData_point_3_position.xyz, cLightData_point_3_diffuseColor, cLightData_point_3_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_3_position.xyz - vertexPosition_w, cLightData_point_3_position.xyz - vertexPosition_w)));
	return result;
}

float precuIntensity(float3 rgb)
{
	return dot(rgb, float3(0.3f, 0.59f, 0.11f));
}


struct InputVertex
{
	float4  position              : POSITION0;
	float4  normal                : NORMAL0;
	float4  color                 : COLOR0;
	float2  textureCoordinateSet0 : TEXCOORD0;
	float2  textureCoordinateSet1 : TEXCOORD1;
};

struct OutputVertex
{
	float4  position              : POSITION0;
	float4  diffuse               : COLOR0;
	float4  dot3Color             : COLOR1;
	float   fog                   : FOG;
	float3  lightDirection_t      : TEXCOORD0;
	float2  textureCoordinateSet1 : TEXCOORD1;
	float2  textureCoordinateSet2 : TEXCOORD2;
	float3 halfAngle_t            : TEXCOORD3;
	float emissive                : TEXCOORD4;
	float3 eyeVector_t            : TEXCOORD5;
};

OutputVertex main(InputVertex inputVertex)
{
	OutputVertex outputVertex;

	// transform vertex
	outputVertex.position = transform3d(inputVertex.position);

	// calculate fog
	outputVertex.fog = calculateFog(inputVertex.position);

	// store dot3 light modulated by vertex color
	outputVertex.dot3Color = inputVertex.color;

	// copy texture coordinates
	outputVertex.lightDirection_t = precuTransformTerrainDot3LightDirection(inputVertex.normal);
	outputVertex.textureCoordinateSet1 = inputVertex.textureCoordinateSet0;
	outputVertex.textureCoordinateSet2 = inputVertex.textureCoordinateSet1;

	// calculate lighting
	float4 precuDiffuse = precuCalculateDiffuseTerrainLighting(inputVertex.position, inputVertex.normal);
	outputVertex.diffuse  = cLightData_ambient_ambientColor + precuDiffuse;
	outputVertex.diffuse  = min(outputVertex.diffuse, 1.0) * inputVertex.color;
	
	//Get view direction and compute half-angle
	float3 h = cLightData_dot3_0_direction.xyz + normalize(cLightData_dot3_0_cameraPosition.xyz - inputVertex.position);
	outputVertex.halfAngle_t = precuTransformTerrainDot3(h, inputVertex.normal);

	outputVertex.emissive = precuIntensity(cMaterial_emissiveColor);

	outputVertex.eyeVector_t  = precuTransformTerrainDot3(normalize(cLightData_dot3_0_cameraPosition.xyz - inputVertex.position), inputVertex.normal);

	return outputVertex;
}
