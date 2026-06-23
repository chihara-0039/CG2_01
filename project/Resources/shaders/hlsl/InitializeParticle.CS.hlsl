static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float currentTime;
    float4 color;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeCounter : register(u1);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    Particle particle = (Particle)0;
    particle.translate = float3(0.0f, 0.0f, 0.0f);
    particle.scale = float3(0.5f, 0.5f, 0.5f);
    particle.lifeTime = 1.0f;
    particle.currentTime = 0.0f;
    particle.color = float4(1.0f, 1.0f, 1.0f, 1.0f);

    gParticles[particleIndex] = particle;

    if (particleIndex == 0)
    {
        gFreeCounter[0] = 0;
    }
}
