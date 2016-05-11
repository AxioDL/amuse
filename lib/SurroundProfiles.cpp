#include "amuse/SurroundProfiles.hpp"
#include <cmath>
#include <cfloat>

namespace amuse
{

static float Dot(const Vector3f& a, const Vector3f& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float Length(const Vector3f& a)
{
    if (std::fabs(a[0]) <= FLT_EPSILON &&
        std::fabs(a[1]) <= FLT_EPSILON &&
        std::fabs(a[2]) <= FLT_EPSILON)
        return 0.f;
    return std::sqrt(Dot(a, a));
}

static float Normalize(Vector3f& out, const Vector3f& in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    float dist = Length(out);
    out[0] /= dist;
    out[1] /= dist;
    out[2] /= dist;
    return dist;
}

static void Cross(Vector3f& out, const Vector3f& a, const Vector3f& b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

class SimpleMatrix
{
    Vector3f m_mat[3];
public:
    SimpleMatrix(const Vector3f& dir, const Vector3f& up)
    {
        Vector3f temp;
        Normalize(temp, dir);
        m_mat[0][1] = temp[0];
        m_mat[1][1] = temp[1];
        m_mat[2][1] = temp[2];

        Normalize(temp, up);
        m_mat[0][2] = temp[0];
        m_mat[1][2] = temp[1];
        m_mat[2][2] = temp[2];

        Cross(temp, dir, up);
        m_mat[0][0] = temp[0];
        m_mat[1][0] = temp[1];
        m_mat[2][0] = temp[2];
    }

    void vecMult(Vector3f& out, const Vector3f& in)
    {
        out[0] = Dot(m_mat[0], in);
        out[1] = Dot(m_mat[1], in);
        out[2] = Dot(m_mat[2], in);
    }
};

struct ReferenceVector
{
    Vector3f vec;
    float bias;
    bool valid = false;
    ReferenceVector() = default;
    ReferenceVector(float x, float y, float z, float thres)
    {
        vec[0] = x;
        vec[1] = y;
        vec[2] = z;
        bias = thres;
        valid = true;
    }
};

static const ReferenceVector StereoVectors[8] =
{
    {-0.80901f, 0.58778f, 0.f, 0.3f},
    { 0.80901f, 0.58778f, 0.f, 0.3f},
};

static const ReferenceVector QuadVectors[8] =
{
    {-0.70710f,  0.70710f, 0.f, 0.1f},
    { 0.70710f,  0.70710f, 0.f, 0.1f},
    {-0.70710f, -0.70710f, 0.f, 0.1f},
    { 0.70710f, -0.70710f, 0.f, 0.1f},
};

static const ReferenceVector Sur51Vectors[8] =
{
    {-0.70710f,  0.70710f, 0.f, 0.1f},
    { 0.70710f,  0.70710f, 0.f, 0.1f},
    {-0.70710f, -0.70710f, 0.f, 0.1f},
    { 0.70710f, -0.70710f, 0.f, 0.1f},
    { 0.0f,      1.0f,     0.f, 0.1f},
    { 0.0f,      1.0f,     0.f, 1.0f},
};

static const ReferenceVector Sur71Vectors[8] =
{
    {-0.70710f,  0.70710f, 0.f, 0.1f},
    { 0.70710f,  0.70710f, 0.f, 0.1f},
    {-0.70710f, -0.70710f, 0.f, 0.1f},
    { 0.70710f, -0.70710f, 0.f, 0.1f},
    { 0.0f,      1.0f,     0.f, 0.1f},
    { 0.0f,      1.0f,     0.f, 1.0f},
    {-1.f,       0.0f,     0.f, 0.1f},
    { 1.f,       0.0f,     0.f, 0.1f},
};

void SurroundProfiles::SetupRefs(float matOut[8], const ChannelMap& map,
                                 const Vector3f& listenEmit, const ReferenceVector refs[])
{
    for (unsigned i=0 ; i<map.m_channelCount && i<8 ; ++i)
    {
        matOut[i] = 0.f;
        if (map.m_channels[i] == AudioChannel::Unknown)
            continue;
        const ReferenceVector& refVec = refs[int(map.m_channels[i])];
        if (!refVec.valid)
            continue;
        matOut[i] = std::max(1.f, Dot(listenEmit, refVec.vec) + refVec.bias);
    }
}

void SurroundProfiles::SetupMatrix(float matOut[8], const ChannelMap& map, AudioChannelSet set,
                                   const Vector3f& emitPos, const Vector3f& listenPos,
                                   const Vector3f& listenHeading, const Vector3f& listenUp)
{
    Vector3f listenDelta;
    listenDelta[0] = emitPos[0] - listenPos[0];
    listenDelta[1] = emitPos[1] - listenPos[1];
    listenDelta[2] = emitPos[2] - listenPos[2];

    Vector3f listenNorm;
    float dist = Normalize(listenNorm, listenDelta);

    SimpleMatrix listenerMat(listenHeading, listenUp);
    Vector3f listenEmit;
    listenerMat.vecMult(listenEmit, listenNorm);

    /* Factor for each channel in set */
    switch (set)
    {
    case AudioChannelSet::Stereo:
    default:
        SetupRefs(matOut, map, listenEmit, StereoVectors);
        break;
    case AudioChannelSet::Quad:
        SetupRefs(matOut, map, listenEmit, QuadVectors);
        break;
    case AudioChannelSet::Surround51:
        SetupRefs(matOut, map, listenEmit, Sur51Vectors);
        break;
    case AudioChannelSet::Surround71:
        SetupRefs(matOut, map, listenEmit, Sur71Vectors);
        break;
    }
}

}
