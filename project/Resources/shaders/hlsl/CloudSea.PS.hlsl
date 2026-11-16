// Background-only volumetric cloud sea. Fixed 24 steps, no secondary ray march.
#define main ObjectPixelMain
#include "Object3d.PS.hlsl"
#undef main

// 雲のノイズ計算に使う、座標由来の擬似乱数を求める。
float HashCloud(float3 p) {
    p = frac(p * .1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}
// 空間座標から連続した密度の揺らぎを作り、雲の形状へ使う。
float NoiseCloud(float3 p) {
    float3 i = floor(p), f = frac(p);
    f = f * f * (3 - 2 * f);
    return lerp(lerp(lerp(HashCloud(i), HashCloud(i + float3(1, 0, 0)), f.x),
                     lerp(HashCloud(i + float3(0, 1, 0)), HashCloud(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(HashCloud(i + float3(0, 0, 1)), HashCloud(i + float3(1, 0, 1)), f.x),
                     lerp(HashCloud(i + float3(0, 1, 1)), HashCloud(i + 1), f.x), f.y),
                f.z);
}
// 視線に沿って雲の密度を積算し、光と透過を持つ雲海の色を求める。
float4 main(VertexShaderOutput input) : SV_TARGET {
    float3 origin = gDirectionalLight.cameraPosition;
    float3 ray = normalize(input.worldPosition - origin);
    float daylight = saturate((gDirectionalLight.intensity - .12) / .75);
    float3 sky = lerp(float3(.025, .045, .12),
                      lerp(float3(.66, .83, .96), float3(.23, .52, .84), saturate(ray.y * .7 + .35)), daylight);
    if (abs(ray.y) < .002) {
        return float4(sky, 1);
    }
    float low = ray.y < 0 ? -32 : 62;
    float high = ray.y < 0 ? -8 : 84;
    float a = (low - origin.y) / ray.y, b = (high - origin.y) / ray.y;
    float begin = max(min(a, b), 0), end = min(max(a, b), 600);
    if (end <= begin) {
        return float4(sky, 1);
    }
    float step = (end - begin) / 24, transmittance = 1;
    float3 accumulated = 0;
    [loop] for (int n = 0; n < 24; ++n) {
        float3 p = origin + ray * (begin + (n + .35 + .3 * HashCloud(float3(input.position.xy, 0))) * step);
        p.x += gDirectionalLight.paddingLight * .45;
        float shape = NoiseCloud(p * float3(.033, .018, .033));
        float detail = NoiseCloud(p * .11);
        float height = saturate((p.y - low) / (high - low));
        float envelope = smoothstep(0, .22, height) * (1 - smoothstep(.65, 1, height));
        float density = saturate((shape * .8 + detail * .2 - .38) * 3) * envelope;
        float opacity = 1 - exp(-density * step * .11);
        float3 cloud =
            lerp(float3(.045, .065, .14), lerp(float3(.49, .65, .78), float3(1, .98, .91), height * .7 + .2), daylight);
        accumulated += transmittance * opacity * cloud;
        transmittance *= 1 - opacity;
        if (transmittance < .025) {
            break;
        }
    }
    float horizon = smoothstep(.005, .07, abs(ray.y));
    return float4(lerp(sky, accumulated + transmittance * sky, horizon), 1);
}
