#include "InstanceData.hlsl"
#include "Math.hlsl"
#include "Samplers.hlsl"
#include "Terrain_Includes.hlsl"
#include "Wrap.hlsl"

Texture2D MaskMap : register(t10);


RWStructuredBuffer<FInstanceData> InstanceTransforms : register(u0);
AppendStructuredBuffer<uint>      InstanceIndicesSteep : register(u1);
AppendStructuredBuffer<uint>      InstanceIndicesFlat : register(u2);


[numthreads(1, 1, 1)] void main(uint3 DTid
                                : SV_DispatchThreadID) {
        uint   id    = DTid.x;
        float3 pos   = float3(InstanceTransforms[id].mtx._41, InstanceTransforms[id].mtx._42, InstanceTransforms[id].mtx._43);
        float  scale = 100.0f;


        float2 MinLocal  = float2(-0.5f * scale, -0.5f * scale);
        float2 MinGlobal = float2(-0.5f * gScale, -0.5f * gScale);
        // Min        = float2(-10.0f, -10.0f);
        float2 MaxLocal  = -MinLocal;
        float2 MaxGlobal = -MinGlobal;


        pos.xz         = wrap(pos.xz, _EyePosition.xz + MinGlobal, _EyePosition.xz + MaxGlobal);
        float3 prevPos = pos;
        pos.xz         = wrap(pos.xz, _EyePosition.xz + MinLocal, _EyePosition.xz + MaxLocal);

        float2 tex = pos.xz / (float2(gScale, gScale)) - 0.5f;

        float heightSample = HeightMap.SampleLevel(sampleTypeWrap, tex, 0).r * 2625.f;
        pos.y              = heightSample * (gTerrainAlpha)-1260.0f * (gTerrainAlpha);
        pos.y /= 8000.0f / gScale;


        InstanceTransforms[id].mtx._42 = pos.y;


        float3 slopeMaskSample = MaskMap.SampleLevel(sampleTypeWrap, tex, 0).rgb;

        float deltaPos = distance(pos.xz, prevPos.xz);


        if (deltaPos < scale / 2.0f)
        {
                InstanceTransforms[id].lifeTime = saturate(InstanceTransforms[id].lifeTime + _DeltaTime);

                if (InstanceTransforms[id].flags & IS_FLAT)
                {
                        InstanceIndicesFlat.Append(id);
                }
                else if (InstanceTransforms[id].flags & IS_STEEP)
                {
                        InstanceIndicesSteep.Append(id);
                }
                return;
        }

        InstanceTransforms[id].mtx._41 = pos.x;
        InstanceTransforms[id].mtx._43 = pos.z;

        InstanceTransforms[id].lifeTime = 0.0f;
        if (slopeMaskSample.r > 0.5f)
        {
                InstanceTransforms[id].flags |= IS_FLAT;
                InstanceIndicesFlat.Append(id);
        }
        else if (slopeMaskSample.g > 0.5f)
        {
                InstanceTransforms[id].flags &= ~IS_FLAT;
                InstanceTransforms[id].flags |= IS_STEEP;
                InstanceIndicesSteep.Append(id);
        }
        else
        {
                InstanceTransforms[id].flags &= ~IS_FLAT;
                InstanceTransforms[id].flags &= ~IS_STEEP;
        }
}
