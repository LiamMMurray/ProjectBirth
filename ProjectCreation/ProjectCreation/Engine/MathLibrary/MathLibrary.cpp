#include "MathLibrary.h"
#include <random>
using namespace DirectX;
using namespace std;

random_device         rd;
default_random_engine generator(rd());

uniform_real_distribution<float> distribution(0.0f, 1.0f);

float MathLibrary::RandomFloatInRange(float min, float max)
{
        return distribution(generator) * (max - min) + min;
}

void MathLibrary::OrthoNormalize(DirectX::XMVECTOR normal, DirectX::XMVECTOR& tangent)
{
        normal = XMVector3Normalize(normal);
        XMVECTOR proj;
        proj = XMVectorMultiply(normal, XMVector3Dot(tangent, normal));

        tangent = XMVector3Normalize(tangent - proj);
}


XMMATRIX MathLibrary::LookAt(DirectX::XMVECTOR vPos, DirectX::XMVECTOR tPos, DirectX::XMVECTOR up)
{
        XMMATRIX output;
        XMVECTOR zValue = tPos - vPos;
        zValue          = XMVector3Normalize(zValue);

        XMVECTOR xValue = XMVector3Cross(up, zValue);
        xValue          = XMVector3Normalize(xValue);

        XMVECTOR yValue = XMVector3Cross(zValue, xValue);
        yValue          = XMVector3Normalize(yValue);

        output.r[0] = xValue;
        output.r[1] = yValue;
        output.r[2] = zValue;
        output.r[3] = vPos;

        return output;
}

void MathLibrary::TurnTo(DirectX::XMMATRIX& matrix, DirectX::XMVECTOR targetPosition, float speed)
{
        XMVECTOR pos = matrix.r[3];
        matrix.r[3]  = XMVectorZero();

        XMVECTOR current = XMQuaternionRotationMatrix(matrix);
        XMVECTOR target  = XMQuaternionRotationMatrix(LookAt(pos, targetPosition, XMVectorSet(0, 1, 0, 0)));

        XMVECTOR blend = XMQuaternionSlerp(current, target, speed);

        XMMATRIX output = XMMatrixRotationQuaternion(blend);
        output.r[3]     = pos;

        matrix = output;
}

DirectX::XMVECTOR MathLibrary::GetClosestPointFromLine(DirectX::XMVECTOR startPoint,
                                                       DirectX::XMVECTOR endPoint,
                                                       DirectX::XMVECTOR point)
{
        XMVECTOR output;
        XMVECTOR length       = XMVector3Normalize(endPoint - startPoint);
        XMVECTOR targetVector = point - startPoint;
        XMVECTOR dotValue     = XMVector3Dot(length, targetVector);
        XMVECTOR distance     = XMVectorMultiply(length, dotValue);
        output                = startPoint + distance;
        XMVECTOR minv         = XMVectorMin(startPoint, endPoint);
        XMVECTOR maxv         = XMVectorMin(startPoint, endPoint);

        output = XMVectorMin(output, maxv);
        output = XMVectorMax(output, minv);

        return output;
}

DirectX::XMVECTOR MathLibrary::GetMidPointFromTwoVector(DirectX::XMVECTOR a, DirectX::XMVECTOR b)
{
        float x = (XMVectorGetX(a) + XMVectorGetX(b)) / 2;
        float y = (XMVectorGetY(a) + XMVectorGetY(b)) / 2;
        float z = (XMVectorGetZ(a) + XMVectorGetZ(b)) / 2;

        XMVECTOR output = XMVectorSet(x, y, z, 0.0f);
        return output;
}

float MathLibrary::CalulateDistance(DirectX::XMVECTOR a, DirectX::XMVECTOR b)
{

        float output;
        output = sqrtf(CalulateDistanceSq(a, b));
        return output;
}

float MathLibrary::CalulateDistanceSq(DirectX::XMVECTOR a, DirectX::XMVECTOR b)
{
        float    output;
        XMVECTOR temp = (a - b);
        temp          = XMVector3Dot(temp, temp);
        output        = XMVectorGetX(temp);
        return output;
}

float MathLibrary::CalulateVectorLength(DirectX::XMVECTOR vector)
{
        return sqrtf(XMVectorGetX(vector) * XMVectorGetX(vector) + XMVectorGetY(vector) * XMVectorGetY(vector) +
                     XMVectorGetZ(vector) * XMVectorGetZ(vector));
}

float MathLibrary::VectorDotProduct(DirectX::XMVECTOR m, DirectX::XMVECTOR n)
{
        return ((XMVectorGetX(m) * XMVectorGetX(n)) + (XMVectorGetY(m) * XMVectorGetY(n)) +
                (XMVectorGetZ(m) * XMVectorGetZ(n)));
}

float MathLibrary::ManhattanDistance(Shapes::FAabb& a, Shapes::FAabb& b)
{
        float    output;
        XMVECTOR dis  = a.center - b.center;
        float    xDis = XMVectorGetX(dis);
        float    yDis = XMVectorGetY(dis);
        float    zDis = XMVectorGetZ(dis);
        output        = abs(xDis) + abs(yDis) + abs(zDis);

        return output;
}

float MathLibrary::GetRandomFloat()
{
        return RANDOMFLOAT;
}

float MathLibrary::GetRandomFloatInRange(float min, float max)
{
        float output = (rand() / (float)RAND_MAX * (max - min)) + min;
        return output;
}

double MathLibrary::GetRandomDouble()
{
        return RANDOMDOUBLE;
}

double MathLibrary::GetRandomDoubleInRange(double min, double max)
{
        double r      = (double)rand() / RAND_MAX;
        double output = min + r * (max - min);
        return output;
}

DirectX::XMVECTOR MathLibrary::GetRandomVector()
{
        return XMVectorSet(RANDOMFLOAT, RANDOMFLOAT, RANDOMFLOAT, 0.0f);
}

DirectX::XMVECTOR MathLibrary::GetRandomVectorInRange(float min, float max)
{
        float value = GetRandomFloatInRange(min, max);
        return XMVectorSet(value, value, value, 0.0f);
}

DirectX::XMVECTOR MathLibrary::GetRandomUnitVector()
{
        XMVECTOR output  = GetRandomVector();
        XMVECTOR unitVec = output / MathLibrary::VectorDotProduct(output, output);
        if (MathLibrary::CalulateVectorLength(unitVec) == 1)
        {
                return unitVec;
        }
}

DirectX::XMFLOAT2 MathLibrary::ConvertToNDC(const DirectX::XMMATRIX& matrix)
{
        DirectX::XMFLOAT2 output;
        // loacal space (model matrix)
        // world space (view matrix)
        // view space (projection matrix)
        // clip space (viewport transform)
        // screen space
        return output;
}
