#include "RenderingSystem.h"
#include "../Engine/GEngine.h"

#include "../Engine/MathLibrary/MathLibrary.h"
#include "../FileIO/FileIO.h"
#include "PostProcess/Bloom.h"

#include <math.h>

#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")

#define WIN32_LEAN_AND_MEAN // Gets rid of bloat on Windows.h
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>

#include <wrl/client.h>
#include "../Utility/Macros/DirectXMacros.h"

#include "../Engine/ResourceManager/AnimationClip.h"
#include "../Engine/ResourceManager/Material.h"
#include "../Engine/ResourceManager/PixelShader.h"
#include "../Engine/ResourceManager/SkeletalMesh.h"
#include "../Engine/ResourceManager/StaticMesh.h"
#include "../Engine/ResourceManager/Texture2D.h"
#include "../Engine/ResourceManager/VertexShader.h"

#include "../Engine/GenericComponents/TransformComponent.h"
#include "Vertex.h"

#include "Components/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "../UI/UIManager.h"

#include "../Engine/MathLibrary/ColorConstants.h"
#include "DebugRender/debug_renderer.h"

void RenderSystem::CreateDeviceAndSwapChain()
{
        using namespace Microsoft::WRL;

        ID3D11Device*        device;
        ID3D11DeviceContext* context;
        HRESULT              hr;

#ifdef _DEBUG
        UINT flags = D3D11_CREATE_DEVICE_DEBUG;
#else
        UINT flags = 0;
#endif
        // flags = 0;

        D3D_FEATURE_LEVEL FeatureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        UINT              numFeatureLevels = ARRAYSIZE(FeatureLevels);
        D3D_FEATURE_LEVEL FeatureLevelObtained;


        hr = D3D11CreateDevice(nullptr,
                               D3D_DRIVER_TYPE_HARDWARE,
                               nullptr,
                               flags,
                               FeatureLevels,
                               numFeatureLevels,
                               D3D11_SDK_VERSION,
                               &device,
                               &FeatureLevelObtained,
                               &context);

        // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
        if (FAILED(hr))
        {
                hr = D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       flags,
                                       &FeatureLevels[1],
                                       numFeatureLevels - 1,
                                       D3D11_SDK_VERSION,
                                       &device,
                                       &FeatureLevelObtained,
                                       &context);
        }
        assert(SUCCEEDED(hr));

        hr = device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&m_Device));

        assert(SUCCEEDED(hr));

        hr = context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&m_Context));
        assert(SUCCEEDED(hr));

        // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
        IDXGIFactory1* dxgiFactory = nullptr;
        {
                IDXGIDevice* dxgiDevice = nullptr;
                hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
                if (SUCCEEDED(hr))
                {
                        IDXGIAdapter* adapter;
                        hr = dxgiDevice->GetAdapter(&adapter);
                        if (SUCCEEDED(hr))
                        {
                                hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
                                adapter->Release();
                        }
                        dxgiDevice->Release();
                }
        }

        assert(SUCCEEDED(hr));
        // Create swap chain
        IDXGIFactory2* dxgiFactory2 = nullptr;
        hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
        dxgiFactory->Release();

        assert(SUCCEEDED(hr));

        RECT rect;
        bool bWindowRect = GetWindowRect((HWND)m_WindowHandle, &rect);
        assert(bWindowRect);

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width              = UINT(rect.right - rect.left);
        sd.Height             = UINT(rect.bottom - rect.top);
        sd.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.Stereo             = false;
        sd.SampleDesc.Count   = 1; // Don't use multi-sampling.
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount        = 2;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; //| DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd{};
        fd.RefreshRate.Numerator   = 0;
        fd.RefreshRate.Denominator = 0;
        fd.Windowed                = true;
        fd.Scaling                 = DXGI_MODE_SCALING_STRETCHED;

        hr = dxgiFactory2->CreateSwapChainForHwnd(m_Device, (HWND)m_WindowHandle, &sd, &fd, nullptr, &m_Swapchain);
        // dxgiFactory->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);
        dxgiFactory2->Release();

#ifdef _DEBUG

        ID3D11Debug* debug = nullptr;
        hr                 = m_Device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&debug));
        assert(SUCCEEDED(hr));
        //debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        debug->Release();
#endif

        device->Release();
        context->Release();
}

void RenderSystem::CreateDefaultRenderTargets(D3D11_TEXTURE2D_DESC* backbufferDesc)
{
        HRESULT          hr;
        ID3D11Texture2D* pBackBuffer;

        // Release all outstanding references to the swap chain's buffers.
        for (int i = 0; i < E_RENDER_TARGET::COUNT; ++i)
        {
                SAFE_RELEASE(m_DefaultRenderTargets[i]);
        }


        m_Context->OMSetRenderTargets(0, 0, 0);

        // Preserve the existing buffer count and format.
        // Automatically choose the width and height to match the client rect for HWNDs.
        hr = m_Swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

        // Perform error handling here!
        assert(SUCCEEDED(hr));

        // Create backbuffer render target
        hr = m_Swapchain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        assert(SUCCEEDED(hr));
        hr = m_Device->CreateRenderTargetView(pBackBuffer, NULL, &m_DefaultRenderTargets[E_RENDER_TARGET::BACKBUFFER]);

        // Create Base Pass render target and resource view
        D3D11_TEXTURE2D_DESC textureDesc;
        pBackBuffer->GetDesc(&textureDesc);

        textureDesc.Format    = DXGI_FORMAT_R16G16B16A16_FLOAT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (backbufferDesc)
                *backbufferDesc = textureDesc;
        pBackBuffer->Release();

        m_BackBufferWidth  = (float)textureDesc.Width;
        m_BackBufferHeight = (float)textureDesc.Height;

        ID3D11Texture2D* texture;

        hr = m_Device->CreateTexture2D(&textureDesc, NULL, &texture);
        assert(SUCCEEDED(hr));
        hr = m_Device->CreateRenderTargetView(texture, NULL, &m_DefaultRenderTargets[E_RENDER_TARGET::BASE_PASS]);
        assert(SUCCEEDED(hr));
        hr = m_Device->CreateShaderResourceView(texture, nullptr, &m_PostProcessSRVs[E_POSTPROCESS_PIXEL_SRV::BASE_PASS]);
        assert(SUCCEEDED(hr));
        texture->Release();

        // Create Base Pass depth stencil and resource view

        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC descDepth{};
        descDepth.Width              = textureDesc.Width;
        descDepth.Height             = textureDesc.Height;
        descDepth.MipLevels          = 1;
        descDepth.ArraySize          = 1;
        descDepth.Format             = DXGI_FORMAT_R24G8_TYPELESS;
        descDepth.SampleDesc.Count   = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage              = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        descDepth.CPUAccessFlags     = 0;
        descDepth.MiscFlags          = 0;
        hr                           = m_Device->CreateTexture2D(&descDepth, nullptr, &texture);
        assert(SUCCEEDED(hr));

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV{};
        descDSV.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDSV.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = m_Device->CreateDepthStencilView(texture, &descDSV, &m_DefaultDepthStencil[E_DEPTH_STENCIL::BASE_PASS]);
        texture->Release();

        assert(SUCCEEDED(hr));
}

void RenderSystem::CreateRasterizerStates()
{
        CD3D11_RASTERIZER_DESC desc(D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0.f, 0.f, TRUE, FALSE, FALSE, FALSE);
        m_Device->CreateRasterizerState(&desc, &m_DefaultRasterizerStates[E_RASTERIZER_STATE::DEFAULT]);
}

void RenderSystem::CreateInputLayouts()
{
        D3D11_INPUT_ELEMENT_DESC vLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}};

        FileIO::FShaderData shaderData;
        EResult             er = FileIO::LoadShaderDataFromFile("Default", "_VS", &shaderData);

        assert(er.m_Flags == ERESULT_FLAG::SUCCESS);

        HRESULT hr = m_Device->CreateInputLayout(vLayout,
                                                 ARRAYSIZE(vLayout),
                                                 shaderData.bytes.data(),
                                                 shaderData.bytes.size(),
                                                 &m_DefaultInputLayouts[E_INPUT_LAYOUT::DEFAULT]);
        assert(SUCCEEDED(hr));


        D3D11_INPUT_ELEMENT_DESC vLayoutSkinned[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"JOINTINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}};

        er = FileIO::LoadShaderDataFromFile("DefaultSkinned", "_VS", &shaderData);

        assert(er.m_Flags == ERESULT_FLAG::SUCCESS);

        hr = m_Device->CreateInputLayout(vLayoutSkinned,
                                         ARRAYSIZE(vLayoutSkinned),
                                         shaderData.bytes.data(),
                                         shaderData.bytes.size(),
                                         &m_DefaultInputLayouts[E_INPUT_LAYOUT::SKINNED]);
        assert(SUCCEEDED(hr));

        D3D11_INPUT_ELEMENT_DESC vLayoutDebug[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}};

        er = FileIO::LoadShaderDataFromFile("Debug", "_VS", &shaderData);

        assert(er.m_Flags == ERESULT_FLAG::SUCCESS);

        hr = m_Device->CreateInputLayout(vLayoutDebug,
                                         ARRAYSIZE(vLayoutDebug),
                                         shaderData.bytes.data(),
                                         shaderData.bytes.size(),
                                         &m_DefaultInputLayouts[E_INPUT_LAYOUT::DEBUG]);
        assert(SUCCEEDED(hr));
}

void RenderSystem::CreateCommonShaders()
{
        m_CommonVertexShaderHandles[E_VERTEX_SHADERS::DEFAULT] = m_ResourceManager->LoadVertexShader("Default");
        m_CommonVertexShaderHandles[E_VERTEX_SHADERS::SKINNED] = m_ResourceManager->LoadVertexShader("DefaultSkinned");
        m_CommonVertexShaderHandles[E_VERTEX_SHADERS::DEBUG]   = m_ResourceManager->LoadVertexShader("Debug");
        m_CommonPixelShaderHandles[E_PIXEL_SHADERS::DEFAULT]   = m_ResourceManager->LoadPixelShader("Default");
        m_CommonPixelShaderHandles[E_PIXEL_SHADERS::DEBUG]     = m_ResourceManager->LoadPixelShader("Debug");
}

void RenderSystem::CreateCommonConstantBuffers()
{
        HRESULT           hr;
        D3D11_BUFFER_DESC bd{};
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth      = sizeof(CTransformBuffer);
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr                = m_Device->CreateBuffer(&bd, nullptr, &m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::MVP]);

        bd.ByteWidth = sizeof(CSceneInfoBuffer);
        hr           = m_Device->CreateBuffer(&bd, nullptr, &m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::SCENE]);

        bd.ByteWidth = sizeof(FSurfaceProperties);
        hr           = m_Device->CreateBuffer(&bd, nullptr, &m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::SURFACE]);

        bd.ByteWidth = sizeof(CAnimationBuffer);
        hr           = m_Device->CreateBuffer(&bd, nullptr, &m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::ANIM]);
}

void RenderSystem::CreateSamplerStates()
{
        HRESULT hr;

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.MipLODBias         = 0.0f;
        sampDesc.MaxAnisotropy      = 1;
        sampDesc.ComparisonFunc     = D3D11_COMPARISON_ALWAYS;
        sampDesc.BorderColor[0]     = 0;
        sampDesc.BorderColor[1]     = 0;
        sampDesc.BorderColor[2]     = 0;
        sampDesc.BorderColor[3]     = 0;
        sampDesc.MinLOD             = 0;
        sampDesc.MaxLOD             = D3D11_FLOAT32_MAX;

        hr = m_Device->CreateSamplerState(&sampDesc, &m_DefaultSamplerStates[E_SAMPLER_STATE::WRAP]);
        assert(SUCCEEDED(hr));

        sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.MipLODBias     = 0.0f;
        sampDesc.MaxAnisotropy  = 1;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampDesc.BorderColor[0] = 0;
        sampDesc.BorderColor[1] = 0;
        sampDesc.BorderColor[2] = 0;
        sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD         = 0;
        sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;

        hr = m_Device->CreateSamplerState(&sampDesc, &m_DefaultSamplerStates[E_SAMPLER_STATE::CLAMP]);
        assert(SUCCEEDED(hr));


        sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_Device->CreateSamplerState(&sampDesc, &m_DefaultSamplerStates[E_SAMPLER_STATE::SKY]);
        assert(SUCCEEDED(hr));


        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        hr                = m_Device->CreateSamplerState(&sampDesc, &m_DefaultSamplerStates[E_SAMPLER_STATE::NEAREST]);
        assert(SUCCEEDED(hr));


        // Create the sample state clamp
        float               black[] = {0.f, 0.f, 0.f, 1.f};
        CD3D11_SAMPLER_DESC SamDescShad(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
                                        D3D11_TEXTURE_ADDRESS_BORDER,
                                        D3D11_TEXTURE_ADDRESS_BORDER,
                                        D3D11_TEXTURE_ADDRESS_BORDER,
                                        0.f,
                                        0,
                                        D3D11_COMPARISON_LESS,
                                        black,
                                        0,
                                        0);
        hr = m_Device->CreateSamplerState(&SamDescShad, &m_DefaultSamplerStates[E_SAMPLER_STATE::SHADOWS]);
        assert(SUCCEEDED(hr));


        m_Context->VSSetSamplers(0, E_SAMPLER_STATE::COUNT, m_DefaultSamplerStates);
        m_Context->PSSetSamplers(0, E_SAMPLER_STATE::COUNT, m_DefaultSamplerStates);
        m_Context->HSSetSamplers(0, E_SAMPLER_STATE::COUNT, m_DefaultSamplerStates);
        m_Context->DSSetSamplers(0, E_SAMPLER_STATE::COUNT, m_DefaultSamplerStates);
}

void RenderSystem::CreateBlendStates()
{
        CD3D11_BLEND_DESC blendDescOpaque{D3D11_DEFAULT};


        m_Device->CreateBlendState(&blendDescOpaque, &m_BlendStates[E_BLEND_STATE::Opaque]);
}

void RenderSystem::CreateDepthStencilStates()
{
        CD3D11_DEPTH_STENCIL_DESC desc = CD3D11_DEPTH_STENCIL_DESC();

        m_Device->CreateDepthStencilState(&desc, &m_DepthStencilStates[E_DEPTH_STENCIL_STATE::BASE_PASS]);
}

void RenderSystem::CreateDebugBuffers()
{
        D3D11_BUFFER_DESC desc{};
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth      = UINT(sizeof(FDebugVertex) * debug_renderer::get_line_vert_capacity());
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_Device->CreateBuffer(&desc, nullptr, &m_DebugVertexBuffer);

        assert(SUCCEEDED(hr));
}

void RenderSystem::DrawDebug()
{
        UINT stride = sizeof(FDebugVertex);
        UINT offset = 0;

        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        m_Context->Map(m_DebugVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData,
               debug_renderer::get_line_verts(),
               debug_renderer::get_line_vert_count() * sizeof(FDebugVertex));
        m_Context->Unmap(m_DebugVertexBuffer, 0);

        m_Context->IASetVertexBuffers(0, 1, &m_DebugVertexBuffer, &stride, &offset);
        m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_Context->IASetInputLayout(m_DefaultInputLayouts[E_INPUT_LAYOUT::DEBUG]);
        ID3D11VertexShader* vs =
            m_ResourceManager->GetResource<VertexShader>(m_CommonVertexShaderHandles[E_VERTEX_SHADERS::DEBUG])->m_VertexShader;
        ID3D11PixelShader* ps =
            m_ResourceManager->GetResource<PixelShader>(m_CommonVertexShaderHandles[E_PIXEL_SHADERS::DEBUG])->m_PixelShader;

        m_Context->VSSetShader(vs, 0, 0);
        m_Context->VSSetConstantBuffers(
            E_CONSTANT_BUFFER_BASE_PASS::MVP, 1, &m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::MVP]);
        m_Context->PSSetShader(ps, 0, 0);
        m_Context->Draw((UINT)debug_renderer::get_line_vert_count(), 0);

}

void RenderSystem::UpdateConstantBuffer(ID3D11Buffer* gpuBuffer, void* cpuBuffer, size_t size)
{
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        m_Context->Map(gpuBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, cpuBuffer, size);
        m_Context->Unmap(gpuBuffer, 0);
}

void RenderSystem::DrawOpaqueStaticMesh(StaticMesh* mesh, Material* material, DirectX::XMMATRIX* mtx)
{
        using namespace DirectX;

        const UINT strides[] = {sizeof(FVertex)};
        const UINT offsets[] = {0};

        m_Context->IASetVertexBuffers(0, 1, &mesh->m_VertexBuffer, strides, offsets);
        m_Context->IASetIndexBuffer(mesh->m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

        VertexShader* vs = m_ResourceManager->GetResource<VertexShader>(material->m_VertexShaderHandle);
        PixelShader*  ps = m_ResourceManager->GetResource<PixelShader>(material->m_PixelShaderHandle);

        m_Context->VSSetShader(vs->m_VertexShader, nullptr, 0);
        m_Context->PSSetShader(ps->m_PixelShader, nullptr, 0);

        m_ConstantBuffer_MVP.World = *mtx;

        FSurfaceProperties surfaceProps;

        UpdateConstantBuffer(
            m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::MVP], &m_ConstantBuffer_MVP, sizeof(m_ConstantBuffer_MVP));
        UpdateConstantBuffer(
            m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::SURFACE], &surfaceProps, sizeof(surfaceProps));


        m_Context->DrawIndexed(mesh->m_IndexCount, 0, 0);
}

void RenderSystem::DrawOpaqueSkeletalMesh(SkeletalMesh*               mesh,
                                          Material*                   material,
                                          DirectX::XMMATRIX*          mtx,
                                          const Animation::FSkeleton* skel)
{
        using namespace DirectX;

        const UINT strides[] = {sizeof(FSkinnedVertex)};
        const UINT offsets[] = {0};

        int jointCount = (int)skel->jointTransforms.size();


        for (int i = 0; i < jointCount; ++i)
        {
                m_ConstantBuffer_ANIM.jointTransforms[i] = skel->jointTransforms[i].transform.CreateMatrix();
        }

        for (int i = 1; i < jointCount; ++i)
        {
                XMMATRIX parent = m_ConstantBuffer_ANIM.jointTransforms[skel->jointTransforms[i].parent_index];
                m_ConstantBuffer_ANIM.jointTransforms[i] = m_ConstantBuffer_ANIM.jointTransforms[i] * parent;
        }
        /** If Debug? Debug renderer**/
        debug_renderer::AddBoneHierarchy(*skel, m_ConstantBuffer_ANIM.jointTransforms, *mtx, ColorConstants::White, 0.2f);

        for (int i = 0; i < jointCount; ++i)
        {
                m_ConstantBuffer_ANIM.jointTransforms[i] = XMMatrixTranspose(skel->inverseBindPose[i].transform.CreateMatrix() *
                                                                             m_ConstantBuffer_ANIM.jointTransforms[i]);
        }

        UpdateConstantBuffer(m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::ANIM],
                             &m_ConstantBuffer_ANIM,
                             sizeof(m_ConstantBuffer_ANIM));

        m_Context->IASetVertexBuffers(0, 1, &mesh->m_VertexBuffer, strides, offsets);
        m_Context->IASetIndexBuffer(mesh->m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

        VertexShader* vs = m_ResourceManager->GetResource<VertexShader>(material->m_VertexShaderHandle);
        PixelShader*  ps = m_ResourceManager->GetResource<PixelShader>(material->m_PixelShaderHandle);

        m_Context->VSSetShader(vs->m_VertexShader, nullptr, 0);
        m_Context->PSSetShader(ps->m_PixelShader, nullptr, 0);

        ID3D11ShaderResourceView* srvs[E_BASE_PASS_PIXEL_SRV::PER_MAT_COUNT];
        m_ResourceManager->GetSRVs(E_BASE_PASS_PIXEL_SRV::PER_MAT_COUNT, material->m_TextureHandles, srvs);

        m_Context->PSSetShaderResources(0, E_BASE_PASS_PIXEL_SRV::PER_MAT_COUNT, srvs);

        m_ConstantBuffer_MVP.World = *mtx;

        UpdateConstantBuffer(
            m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::MVP], &m_ConstantBuffer_MVP, sizeof(m_ConstantBuffer_MVP));
        UpdateConstantBuffer(m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::SURFACE],
                             &material->m_SurfaceProperties,
                             sizeof(FSurfaceProperties));


        m_Context->DrawIndexed(mesh->m_IndexCount, 0, 0);
}

void RenderSystem::DrawTransparentStaticMesh(StaticMesh* mesh, Material* material, DirectX::XMMATRIX* mtx)
{}

void RenderSystem::DrawTransparentSkeletalMesh(SkeletalMesh* mesh, Material* material, DirectX::XMMATRIX* mtx)
{}

void RenderSystem::RefreshMainCameraSettings()
{
        using namespace DirectX;

        CameraComponent* camera =
            static_cast<CameraComponent*>(m_ComponentManager->GetComponent<CameraComponent>(m_MainCameraHandle));
        auto& settings         = camera->m_Settings;
        settings.m_AspectRatio = GetWindowAspectRatio();
        float verticalFOV      = XMConvertToRadians(settings.m_HorizontalFOV / settings.m_AspectRatio);

        m_CachedMainProjectionMatrix =
            DirectX::XMMatrixPerspectiveFovLH(verticalFOV, settings.m_AspectRatio, settings.m_NearClip, settings.m_FarClip);
}

void RenderSystem::OnPreUpdate(float deltaTime)
{
        ID3D11ShaderResourceView* srvs[3] = {0};
        srvs[0] = m_ResourceManager->GetResource<Texture2D>(m_ResourceManager->LoadTexture2D("IBLTestDiffuseHDR"))->m_SRV;
        srvs[1] = m_ResourceManager->GetResource<Texture2D>(m_ResourceManager->LoadTexture2D("IBLTestSpecularHDR"))->m_SRV;
        srvs[2] = m_ResourceManager->GetResource<Texture2D>(m_ResourceManager->LoadTexture2D("IBLTestBrdf"))->m_SRV;

        m_Context->PSSetShaderResources(E_BASE_PASS_PIXEL_SRV::PER_MAT_COUNT, 3, srvs);
}

void RenderSystem::OnUpdate(float deltaTime)
{
        using namespace DirectX;

        ResourceManager* rm = GEngine::Get()->GetResourceManager();

        // Set base pass texture as render target
        m_Context->OMSetRenderTargets(
            1, &m_DefaultRenderTargets[E_RENDER_TARGET::BASE_PASS], m_DefaultDepthStencil[E_DEPTH_STENCIL::BASE_PASS]);

        /** Test clear render target **/
        static FLOAT color[] = {0.0f, 0.0f, 0.0f, 0.0f};
        m_Context->ClearRenderTargetView(m_DefaultRenderTargets[E_RENDER_TARGET::BASE_PASS], color);
        m_Context->ClearDepthStencilView(
            m_DefaultDepthStencil[E_DEPTH_STENCIL::BASE_PASS], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        /** Create viewport. This should be replaced**/
        D3D11_VIEWPORT viewport;
        viewport.Height   = m_BackBufferHeight;
        viewport.Width    = m_BackBufferWidth;
        viewport.MaxDepth = 1.0f;
        viewport.MinDepth = 0.0f;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;

        CameraComponent*    mainCamera       = m_ComponentManager->GetComponent<CameraComponent>(m_MainCameraHandle);
        EntityHandle        mainCameraEntity = mainCamera->GetOwner();
        TransformComponent* mainTransform    = m_ComponentManager->GetComponent<TransformComponent>(mainCameraEntity);

        XMMATRIX view = (XMMatrixInverse(nullptr, mainTransform->transform.CreateMatrix()));

        m_ConstantBuffer_MVP.ViewProjection = XMMatrixTranspose(view * m_CachedMainProjectionMatrix);

        m_Context->RSSetState(m_DefaultRasterizerStates[E_RASTERIZER_STATE::DEFAULT]);
        m_Context->RSSetViewports(1, &viewport);

        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        UINT  sampleMask     = 0xffffffff;
        m_Context->OMSetBlendState(m_BlendStates[E_BLEND_STATE::Opaque], blendFactor, sampleMask);
        m_Context->OMSetDepthStencilState(m_DepthStencilStates[E_DEPTH_STENCIL_STATE::BASE_PASS], 0);

        /** Draw Mesh. Should be replaced by loop **/

        m_Context->IASetInputLayout(m_DefaultInputLayouts[E_INPUT_LAYOUT::SKINNED]);
        m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_Context->VSSetConstantBuffers(0, E_CONSTANT_BUFFER_BASE_PASS::COUNT, m_BasePassConstantBuffers);
        m_Context->PSSetConstantBuffers(0, E_CONSTANT_BUFFER_BASE_PASS::COUNT, m_BasePassConstantBuffers);

        /** Get Drectional Light**/
        {
                if (m_ComponentManager->ComponentsExist<DirectionalLightComponent>())
                        return;

                auto dirLightItr = m_ComponentManager->GetActiveComponents<DirectionalLightComponent>();

                for (auto itr = dirLightItr.begin(); itr != dirLightItr.end(); itr++)
                {

                        auto dirLightComp = static_cast<DirectionalLightComponent*>(itr.data());
                        XMStoreFloat3(&m_ConstantBuffer_SCENE.directionalLightDirection,
                                      dirLightComp->m_LightRotation.GetForward());

                        m_ConstantBuffer_SCENE.directioanlLightColor = dirLightComp->m_LightColor;
                }
        }

        XMStoreFloat3(&m_ConstantBuffer_SCENE.eyePosition, mainTransform->transform.translation);
        m_ConstantBuffer_SCENE.time = (float)GEngine::Get()->GetTotalTime();
        UpdateConstantBuffer(m_BasePassConstantBuffers[E_CONSTANT_BUFFER_BASE_PASS::SCENE],
                             &m_ConstantBuffer_SCENE,
                             sizeof(m_ConstantBuffer_SCENE));


        /** Render all opaque static meshes **/
        if (false)
        {
                {
                        if (!m_ComponentManager->ComponentsExist<StaticMeshComponent>())
                                return;

                        auto     staticMeshCompItr = m_ComponentManager->GetActiveComponents<StaticMeshComponent>();
                        Material mat;
                        mat.m_VertexShaderHandle = m_CommonVertexShaderHandles[E_VERTEX_SHADERS::SKINNED];
                        mat.m_PixelShaderHandle  = m_CommonPixelShaderHandles[E_PIXEL_SHADERS::DEFAULT];

                        for (auto itr = staticMeshCompItr.begin(); itr != staticMeshCompItr.end(); itr++)
                        {
                                auto        staticMeshComp = static_cast<StaticMeshComponent*>(itr.data());
                                StaticMesh* staticMesh =
                                    m_ResourceManager->GetResource<StaticMesh>(staticMeshComp->m_StaticMeshHandle);
                                EntityHandle        entityHandle = staticMeshComp->GetOwner();
                                TransformComponent* tcomp =
                                    m_EntityManager->GetEntity(entityHandle)->GetComponent<TransformComponent>();

                                XMMATRIX world = tcomp->transform.CreateMatrix();

                                DrawOpaqueStaticMesh(staticMesh, &mat, &world);
                        }
                }
        }

        /** Render all opaque skeletal meshes **/
        {
                if (m_ComponentManager->ComponentsExist<SkeletalMeshComponent>())
                        return;

                auto skelCompItr = m_ComponentManager->GetActiveComponents<SkeletalMeshComponent>();

                for (auto itr = skelCompItr.begin(); itr != skelCompItr.end(); itr++)
                {
                        auto          skelMeshComp = static_cast<SkeletalMeshComponent*>(itr.data());
                        SkeletalMesh* skelMesh =
                            m_ResourceManager->GetResource<SkeletalMesh>(skelMeshComp->m_SkeletalMeshHandle);
                        EntityHandle        entityHandle = skelMeshComp->GetOwner();
                        TransformComponent* tcomp        = m_ComponentManager->GetComponent<TransformComponent>(entityHandle);

                        XMMATRIX world = tcomp->transform.CreateMatrix();

                        DrawOpaqueSkeletalMesh(skelMesh,
                                               m_ResourceManager->GetResource<Material>(skelMeshComp->m_MaterialHandle),
                                               &world,
                                               &skelMeshComp->m_Skeleton);
                }
        }


        // Set the backbuffer as render target
        m_Context->OMSetRenderTargets(1, &m_DefaultRenderTargets[E_RENDER_TARGET::BACKBUFFER], nullptr);

        /** Move from base pass to backbuffer. Should be replaced by post processing **/
        /*ID3D11Resource* dest;
        ID3D11Resource* src;
        m_DefaultRenderTargets[E_RENDER_TARGET::BACKBUFFER]->GetResource(&dest);
        m_DefaultRenderTargets[E_RENDER_TARGET::BASE_PASS]->GetResource(&src);
        m_Context->CopyResource(dest, src);
        dest->Release();
        src->Release();*/
        bloom->Render(&m_PostProcessSRVs[E_POSTPROCESS_PIXEL_SRV::BASE_PASS],
                      &m_DefaultRenderTargets[E_RENDER_TARGET::BACKBUFFER]);

        /** Draw Debug Renderer **/
        Shapes::FAabb               aabb;
        Shapes::FSphere sphere;

        aabb.center  = XMVectorSet(0.0f, 0.5f, 0.0f, 1.0f);
        aabb.extents = XMVectorSet(1.0f, 0.5f, 0.2f, 1.0f);

        sphere.center = XMVectorSet(0.0f, 0.2f, 0.5f, 2.0f);
        sphere.radius = 0.1f;

        debug_renderer::AddBox(aabb, XMMatrixIdentity(), ColorConstants::White);
        debug_renderer::AddSphere(sphere, 32, XMMatrixIdentity());
        debug_renderer::AddMatrix(XMMatrixTranslationFromVector(sphere.center), 0.05f);

		if (GEngine::Get()->IsDebugMode())
            DrawDebug();
        debug_renderer::clear_lines();

		//UI Manager Update
        UIManager::Update();

        DXGI_PRESENT_PARAMETERS parameters = {0};
        m_Swapchain->Present1(1, 0, &parameters);
}

void RenderSystem::OnPostUpdate(float deltaTime)
{
        ID3D11ShaderResourceView* srvs[7] = {};
        m_Context->VSSetShaderResources(0, 7, srvs);
        m_Context->PSSetShaderResources(0, 7, srvs);
        m_Context->OMSetRenderTargets(0, nullptr, nullptr);
}

void RenderSystem::OnInitialize()
{
        assert(m_WindowHandle);

        m_ResourceManager  = GEngine::Get()->GetResourceManager();
        m_EntityManager    = GEngine::Get()->GetEntityManager();
        m_ComponentManager = GEngine::Get()->GetComponentManager();

        CreateDeviceAndSwapChain();
        D3D11_TEXTURE2D_DESC desc;
        CreateDefaultRenderTargets(&desc);
        CreateRasterizerStates();
        CreateCommonShaders();
        CreateInputLayouts();
        CreateCommonConstantBuffers();
        CreateSamplerStates();
        CreateDebugBuffers();

        bloom = new Bloom;
        bloom->Initialize(m_Device, m_Context, &desc);
		//UI Manager Initialize
        UIManager::Initialize();
}

void RenderSystem::OnShutdown()
{
        SetFullscreen(false);

        for (int i = 0; i < E_RENDER_TARGET::COUNT; ++i)
        {
                SAFE_RELEASE(m_DefaultRenderTargets[i]);
        }

        for (int i = 0; i < E_POSTPROCESS_PIXEL_SRV::COUNT; ++i)
        {

                SAFE_RELEASE(m_PostProcessSRVs[i]);
        }

        for (int i = 0; i < E_DEPTH_STENCIL::COUNT; ++i)
        {

                SAFE_RELEASE(m_DefaultDepthStencil[i]);
        }

        for (int i = 0; i < E_INPUT_LAYOUT::COUNT; ++i)
        {

                SAFE_RELEASE(m_DefaultInputLayouts[i]);
        }

        for (int i = 0; i < E_RASTERIZER_STATE::COUNT; ++i)
        {

                SAFE_RELEASE(m_DefaultRasterizerStates[i]);
        }

        for (int i = 0; i < E_CONSTANT_BUFFER_BASE_PASS::COUNT; ++i)
        {

                SAFE_RELEASE(m_BasePassConstantBuffers[i]);
        }

        for (int i = 0; i < E_SAMPLER_STATE::COUNT; ++i)
        {

                SAFE_RELEASE(m_DefaultSamplerStates[i]);
        }

		for (int i = 0; i < E_BLEND_STATE::COUNT; ++i)
        {

                SAFE_RELEASE(m_BlendStates[i]);
        }

		for (int i = 0; i < E_DEPTH_STENCIL_STATE::COUNT; ++i)
        {

                SAFE_RELEASE(m_DepthStencilStates[i]);
        }

        SAFE_RELEASE(m_DebugVertexBuffer);

        SAFE_RELEASE(m_Swapchain);
        SAFE_RELEASE(m_Context);
        SAFE_RELEASE(m_Device);

        bloom->Shutdown();
        delete bloom;
		//UI Manager Shutdown
        UIManager::Shutdown();
}

void RenderSystem::OnResume()
{}

void RenderSystem::OnSuspend()
{}

void RenderSystem::SetWindowHandle(native_handle_type handle)
{
        m_WindowHandle = handle;
}

void RenderSystem::SetMainCameraComponent(ComponentHandle cameraHandle)
{
        m_MainCameraHandle = cameraHandle;

        RefreshMainCameraSettings();
}

void RenderSystem::OnWindowResize(WPARAM wParam, LPARAM lParam)
{
        if (m_Swapchain)
        {
                CreateDefaultRenderTargets();
        }
}

void RenderSystem::SetFullscreen(bool val)
{
        if (val)
        {
                DXGI_MODE_DESC desc{};
                desc.Format  = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.Height  = 1920;
                desc.Width   = 1080;
                desc.Scaling = DXGI_MODE_SCALING_STRETCHED;

                IDXGIOutput* target = nullptr;
                m_Swapchain->SetFullscreenState(true, target);
                m_Swapchain->ResizeTarget(&desc);
        }
        else
        {
                m_Swapchain->SetFullscreenState(false, nullptr);
        }
}

bool RenderSystem::GetFullscreen()
{
        BOOL val;
        m_Swapchain->GetFullscreenState(&val, nullptr);
        return val;
}
