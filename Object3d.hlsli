// �}�e���A�����i�F�E���C�e�B���O�L���t���O�EUV�ϊ��Ȃǁj
struct Material
{
    float4 color; // �F�iRGBA�j
    bool enableLighting; // ���C�e�B���O���g�����ǂ���
    float3 padding; // �p�f�B���O�ibool �̌���16�o�C�g�ɑ�����j

    // UV�ϊ��s��i�K�v�ł���΁j���g��Ȃ��Ȃ�폜��
    float4x4 uvTransform;
};

// �f�B���N�V���i�����C�g�̏��i�����E���x�E�F�j
struct DirectionalLight
{
    float3 direction; // ���C�g�̕���
    float intensity; // ���x
    float3 color; // ���C�g�̐F
    float pad; // �p�f�B���O�i16�o�C�g�����j
};

// �s��f�[�^�i���[���h�s��EWVP�s��j
struct TransformationMatrix
{
    float4x4 WVP; // World-View-Projection
    float4x4 World; // ���[���h�s��i�@���ϊ��ȂǂɎg�p�j
};

// ���_�V�F�[�_�[�o�͍\���́i�s�N�Z���V�F�[�_�[���͂ƈ�v�j
struct VertexShaderOutput
{
    float4 position : SV_POSITION; // �ŏI�I�ȃX�N���[�����W
    float2 texcoord : TEXCOORD0; // �e�N�X�`�����W
    float3 normal : NORMAL0; // �@���x�N�g���i�Ɩ��v�Z�Ɏg�p�j
};
