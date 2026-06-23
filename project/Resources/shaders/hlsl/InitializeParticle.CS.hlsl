static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 velocity;
    float3 scale;
    float lifeTime;
    float currentTime;
    float4 color;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

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
    particle.velocity = float3(0.0f, 0.0f, 0.0f);
    particle.scale = float3(0.0f, 0.0f, 0.0f);
    particle.lifeTime = 1.0f;
    particle.currentTime = 0.0f;
    particle.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

    gParticles[particleIndex] = particle;
    gFreeList[particleIndex] = particleIndex;

    if (particleIndex == 0)
    {
        gFreeListIndex[0] = kMaxParticles - 1;
    }
}
