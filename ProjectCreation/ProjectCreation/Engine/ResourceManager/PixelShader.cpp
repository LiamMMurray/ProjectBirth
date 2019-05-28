#include "PixelShader.h"

#include <d3d11_1.h>
#include "../../Utility/Macros/DirectXMacros.h"

void PixelShader::Release()
{
        SAFE_RELEASE(m_PixelShader);
}