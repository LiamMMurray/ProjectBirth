#include "BasePassTextureMaps.hlsl"
#include "Constants.hlsl"
#include "DefaultPixelIn.hlsl"
#include "DefaultVertexIn.hlsl"
#include "Samplers.hlsl"
#include "SceneBuffer.hlsl"

cbuffer CSurfaceProperties : register(b2)
{
        float3 _diffuseColor;
        float  _ambientIntensity;
        float3 _emissiveColor;
        float  _specular;
        float  _roughnessMin;
        float  _roughnessMax;
        float  _metallic;
        float  _normalIntensity;
        int    _textureFlags;
};

#include "PBRMath.hlsl"

// Colors define
// alpha is 1.0f in defult
#define White float4(1.0f, 1.0f, 1.0f, 1.0f)
#define Black float4(0.0f, 0.0f, 0.0f, 1.0f)
#define Red float4(1.0f, 0.0f, 0.0f, 1.0f)
#define Green float4(0.0f, 1.0f, 0.0f, 1.0f)
#define Blue float4(0.0f, 0.0f, 1.0f, 1.0f)
#define Cyan float4(0.0f, 1.0f, 1.0f, 1.0f)
#define Magenta float4(1.0f, 0.0f, 1.0f, 1.0f)
#define Yellow float4(1.0f, 1.0f, 0.0f, 1.0f)

float4 main(INPUT_PIXEL input) : SV_TARGET
{
        SurfacePBR surface;
        float3     color;
        float3     viewWS      = -normalize(input.PosWS - _EyePosition);
        float3     specColor   = 0.04f;
        float4     defultColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
        float      time        = _Time;
        float4     output      = (0.0f, 0.0f, 0.0f, 0.0f);
        surface.diffuseColor   = 1.0f;
        surface.metallic       = 0.0f;
        surface.emissiveColor  = 0.0f;
        surface.specular       = 0.04f;
        surface.roughness      = 1.0f;

        float3 normalSample = normalMap.Sample(sampleTypeWrap, input.Tex).xyz * 2.f - 1.f;
        float3 normalSample2 =
            normalMap.Sample(sampleTypeWrap, input.Tex * 2.0f + _Time * float2(1.0f, -0.2f) * 0.1f).xyz * 2.f - 1.f;
        normalSample.z  = sqrt(1 - normalSample.x * normalSample.x - normalSample.y * normalSample.y);
        normalSample2.z = sqrt(1 - normalSample2.x * normalSample2.x - normalSample2.y * normalSample2.y);
        normalSample    = normalize(normalSample + normalSample2);
        /*   surface.normal = _normalIntensity * input.TangentWS * normalSample.x +
                            _normalIntensity * input.BinormalWS * normalSample.y + input.NormalWS * normalSample.z;*/
        surface.normal = input.TangentWS * normalSample.x + input.BinormalWS * normalSample.y + input.NormalWS * normalSample.z;

        surface.normal = normalize(surface.normal);

        surface.ambient = 0.0f;

        float dist         = distance(_EyePosition.xz, input.PosWS.xz);
        float distanceClip = 1.0f - saturate((dist - 200.0f) / 20.0f);
        clip(distanceClip < 0.5f ? -1 : 1);

        float fresnel = saturate(dot(normalize(surface.normal.xz), normalize(viewWS.xz)));

        float clipMask   = saturate(pow(fresnel, 4.0f) * 5.0f);
        float fakeShadow = 1.0f - saturate(pow(fresnel, 10.0f) * 2.0f);

        clip(clipMask < 0.5f ? -1 : 1);
        // return fakeShadow;
        float4 diffuseSample;


        diffuseSample = 1.0f * diffuseMap.Sample(sampleTypeWrap, input.Tex);

        clip((diffuseSample.a) < 0.1f ? -1 : 1); // masked

        // return lerp(2.0f, 0.15f, fakeShadow);
        surface.diffuseColor *= diffuseSample.xyz;
        surface.diffuseColor = saturate(surface.diffuseColor);

        color = surface.diffuseColor * lerp(0.05f, 0.00f, fakeShadow);
        // Directional Light
        {
                float3 radiance = float3(50.0f, 0.0f, 0.0f);
                float3 lightDir = float3(0.0f, 1.0f, 0.0f);
                color += radiance * PBR(surface, -lightDir, viewWS, specColor) * input.linearDepth;
        }
        // point light
        /*{
                float  ptRange    = 6.0f;
                float3 ptColor    = {5.0f,0.0f,0.0f};
                float3 PtLightPos = float3(0.0f, 0.0f, 0.0f);
                float3 dir        = (PtLightPos - input.PosWS);
                float  distance   = length(dir);
                dir               = normalize(dir);

                float amountLight = saturate(dot(dir, input.NormalWS));

                float lightIntensity = saturate(dot(input.NormalWS, normalize(-dir)));
                float specular       = 0.0f;
                if (lightIntensity > 0.0f)
                {
                        ptColor += amountLight * lightIntensity;
                        float3 reflection = normalize(reflect(normalize(-dir), input.NormalWS));
                }

                if (amountLight > 0.0f)
                {
                        float attenuation = 1.0f - saturate(distance / ptRange);

                        ptColor += amountLight * surface.diffuseColor * attenuation;
                }

                color += ptColor * PBR(surface, -dir, viewWS, specColor);
        }*/

        output += float4(color, 1.0f);
        return output;
}