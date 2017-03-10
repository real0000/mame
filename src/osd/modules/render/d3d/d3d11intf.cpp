// license:BSD-3-Clause
// copyright-holders:Ian Wu
//============================================================
//
//  d3d11intf.cpp - Direct3D 11 interfave
//
//============================================================

// standard windows headers

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3d11.h>
#include <D3Dcompiler.h>
#undef interface

// MAME headers
#include "emu.h"

// MAMEOS headers
#include "d3dintf.h"

#include "strconv.h"
#include "winutf8.h"

//============================================================
//  TYPE DEFINITIONS
//============================================================

typedef HRESULT (WINAPI *direct3dcreate11_ptr)(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driver_type, HMODULE software, UINT Flags, const D3D_FEATURE_LEVEL *feature_levels, UINT num_feature_levels, UINT sdk_version, ID3D11Device **device, D3D_FEATURE_LEVEL *feature_level, ID3D11DeviceContext **context);
typedef HRESULT (WINAPI *d3dcompilefromfile_ptr)(LPCWSTR fileName, D3D_SHADER_MACRO *defines, ID3DInclude *include, const char *entrypoint, const char *target, UINT flags1, UINT flags2, ID3DBlob **code, ID3DBlob **error_messages);

//============================================================
//  PROTOTYPES
//============================================================

static void set_interfaces(d3d11_base *d3dptr);

//============================================================
//  drawd3d11_init
//============================================================

d3d11_base *drawd3d11_init(void)
{
    HINSTANCE dllhandle_d3d = LoadLibrary(TEXT("d3d11.dll"));
    if(dllhandle_d3d == nullptr)
    {
        osd_printf_verbose("Direct3D: Unable to access d3d11.dll\n");
        return nullptr;
    }

    HINSTANCE dllhandle_compiler = LoadLibrary(TEXT("d3dcompiler_47.dll"));
    if(dllhandle_compiler == nullptr)
    {
        osd_printf_verbose("Direct3D: Unable to access d3dcompiler_47.dll\n");
        return nullptr;
    }

    auto d3dptr = global_alloc(d3d11_base);
    d3dptr->dllhandle_d3d11 = dllhandle_d3d;
    d3dptr->dllhandle_d3dcompiler = dllhandle_compiler;
    set_interfaces(d3dptr);
    return d3dptr;
}

//============================================================
//  Direct3D 11 interfaces
//============================================================

static HRESULT create_device(d3d11_base *d3dptr, device11 **dev11, context11 **context)
{
    direct3dcreate11_ptr direct3dcreate11 = (direct3dcreate11_ptr)GetProcAddress(d3dptr->dllhandle_d3d11, "D3D11CreateDevice");

    UINT flag = 0;
#ifdef MAME_DEBUG
    flag = D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const unsigned int num_feature = sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL);
    return direct3dcreate11(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flag, featureLevels, num_feature, D3D11_SDK_VERSION, (ID3D11Device **)dev11, nullptr, (ID3D11DeviceContext **)context);
}

static HRESULT effect_compiler(d3d11_base *d3dptr, const char *fileName, D3D_SHADER_MACRO *defines, ID3DInclude *include, const char *entrypoint, const char *target, ID3DBlob **code, ID3DBlob **error_messages)
{
    d3dcompilefromfile_ptr compiler = (d3dcompilefromfile_ptr)GetProcAddress(d3dptr->dllhandle_d3dcompiler, "D3DCompileFromFile");

    UINT flag = 0;
#ifdef MAME_DEBUG
    flag |= D3DCOMPILE_DEBUG;
#endif
    TCHAR *tc_buf = tstring_from_utf8(fileName);
    HRESULT result = compiler((LPCWSTR)tc_buf, defines, include, entrypoint, target, flag, 0, code, error_messages);
    osd_free(tc_buf);

    return result;
}

static void release(d3d11_base *d3dptr)
{
    FreeLibrary(d3dptr->dllhandle_d3d11);
    FreeLibrary(d3dptr->dllhandle_d3dcompiler);
}

static const d3d_interface11 d3d11_interface = 
{
    create_device,
    effect_compiler,
    release
};

//============================================================
//  Direct3DDevice 11 interfaces
//============================================================

static HRESULT device_create_texture11(device11 *dev, UINT width, UINT height, UINT levels, D3D11_USAGE usage, DXGI_FORMAT format, void *rawpixel, UINT pitch, texture11 **texture, texture11_view **view)
{
    ID3D11Device *device = (ID3D11Device *)dev;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.MipLevels = levels;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Usage = usage;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    D3D11_SUBRESOURCE_DATA initdata = {};
    initdata.pSysMem = rawpixel;
    initdata.SysMemPitch = pitch;

    HRESULT result = device->CreateTexture2D(&desc, &initdata, (ID3D11Texture2D **)texture);
    if( S_OK != result ) return result;
    
    return device->CreateShaderResourceView(*(ID3D11Texture2D **)texture, nullptr, (ID3D11ShaderResourceView **)view);
}

static HRESULT device_create_buffer11(device11 *dev, UINT length, UINT usage, void *data, buffer11 **buf)
{
    ID3D11Device *device = (ID3D11Device *)dev;
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = length;
    desc.BindFlags = usage;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initdata = {};
    initdata.pSysMem = data;
    return device->CreateBuffer(&desc, &initdata, (ID3D11Buffer **)buf);
}

static HRESULT device_create_render_target11(device11 *dev, UINT sample, UINT quality, UINT width, UINT height, DXGI_FORMAT format, surface11 **surface, texture11 **texture, texture11_view **view)
{
    ID3D11Device *device = (ID3D11Device *)dev;

    bool is_depth = format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
                    format == DXGI_FORMAT_D32_FLOAT ||
                    format == DXGI_FORMAT_D24_UNORM_S8_UINT;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = sample;
    desc.SampleDesc.Quality = quality;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = (is_depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET) | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    DXGI_FORMAT depth_view_format = format;
    switch( format )
    {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            desc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
            depth_view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;

        case DXGI_FORMAT_D32_FLOAT:
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            depth_view_format = DXGI_FORMAT_R32_FLOAT;
            break;

        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            depth_view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;

        default:break;
    }

    HRESULT result = device->CreateTexture2D(&desc, nullptr, (ID3D11Texture2D **)texture);
    if( S_OK != result ) return result;
    if( !is_depth )
    {
        result = device->CreateRenderTargetView(*(ID3D11Texture2D **)texture, nullptr, (ID3D11RenderTargetView **)surface);
        if( S_OK != result ) return result;
        result = device->CreateShaderResourceView(*(ID3D11Texture2D **)texture, nullptr, (ID3D11ShaderResourceView **)view);
        if( S_OK != result ) return result;
    }
    else
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_desc = {};
        depth_desc.Format = format;
        depth_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        result = device->CreateDepthStencilView(*(ID3D11Texture2D **)texture, &depth_desc, (ID3D11DepthStencilView **)surface);
        if( S_OK != result ) return result;

        D3D11_SHADER_RESOURCE_VIEW_DESC depth_view_desc = {};
        depth_view_desc.Format = depth_view_format;
        depth_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        result = device->CreateShaderResourceView(*(ID3D11Texture2D **)texture, &depth_view_desc, (ID3D11ShaderResourceView **)view);
        if( S_OK != result ) return result;
    }
    return result;
}

static HRESULT device_create_shared_texture11(device11 *dev, HANDLE src, texture11 **dst, texture11_view **view)
{
    ID3D11Device *device = (ID3D11Device *)dev;

    ID3D11Resource *temp_resource = nullptr;
    HRESULT result = device->OpenSharedResource(src, __uuidof(ID3D11Resource), (void**)(&temp_resource)); 
    if( S_OK != result ) return result;

    result = temp_resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)dst); 
    temp_resource->Release();
    if( S_OK != result ) return result;
    
    return device->CreateShaderResourceView(*(ID3D11Texture2D **)dst, nullptr, (ID3D11ShaderResourceView **)view);
}

static void device_set_render_target11(context11 *con, surface11 *surf, surface11 *depth)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->OMSetRenderTargets(1, (ID3D11RenderTargetView **)&surf, (ID3D11DepthStencilView *)depth);
}

static void device_set_vertex_buffer11(context11 *con, buffer11 *buff, UINT stride, UINT offset)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->IASetVertexBuffers(0, 1, (ID3D11Buffer **)&buff, &stride, &offset);
}

static void device_set_index_buffer11(context11 *con, buffer11 *buff, DXGI_FORMAT format, UINT offset)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->IASetIndexBuffer((ID3D11Buffer *)buff, format, offset);
}

static void device_clear_render_target11(context11 *con, surface11 *surf, surface11 *dsurf, float color[4])
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->ClearRenderTargetView((ID3D11RenderTargetView *)surf, color);
    context->ClearDepthStencilView((ID3D11DepthStencilView *)dsurf, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

static void device_set_texture11(context11 *con, DWORD stage, texture11_view *view)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->PSSetShaderResources(stage, 1, (ID3D11ShaderResourceView **)&view);
}

static void device_set_viewport11(context11 *con, float topleft_x, float topleft_y, float width, float height, float min_depth, float max_depth)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    D3D11_VIEWPORT viewport = {topleft_x, topleft_y, width, height, min_depth, max_depth};
    context->RSSetViewports(1, &viewport);
}

static void device_draw_indexed11(context11 *con, D3D11_PRIMITIVE_TOPOLOGY topology, UINT count, UINT offset, int basevert)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->IASetPrimitiveTopology(topology);
    context->DrawIndexed(count, offset, basevert);
}

static void device_flush11(context11 *con)
{
    ID3D11DeviceContext *context = (ID3D11DeviceContext *)con;
    context->Flush();
}

static ULONG device_release11(device11 *dev, context11 *context)
{
    ID3D11Device *device = (ID3D11Device *)dev;
    ID3D11DeviceContext *device_context = (ID3D11DeviceContext *)context;

    HRESULT result = S_OK;
    if( nullptr != device )
    {
        result = device->Release();
        if( S_OK != result )
        {
            osd_printf_verbose("Direct3D: Warning - Unable release d3d11 device(%x)\n", (unsigned int)result);
        }
    }
    
    if( nullptr != device_context )
    {
        result = device_context->Release();
        if( S_OK != result )
        {
            osd_printf_verbose("Direct3D: Warning - Unable release d3d11 context(%x)\n", (unsigned int)result);
        }
    }

    return result;
}

static const d3d_device_interface11 d3d11_device_interface =
{
    device_create_texture11,
    device_create_buffer11,
    device_create_render_target11,
    device_create_shared_texture11,
    device_set_render_target11,
    device_set_vertex_buffer11,
    device_set_index_buffer11,
    device_set_texture11,
    device_set_viewport11,
    device_clear_render_target11,
    device_draw_indexed11,
    device_flush11,
    device_release11
};


//============================================================
//  set_interfaces
//============================================================
static void set_interfaces(d3d11_base *d3dptr)
{
    d3dptr->d3d = d3d11_interface;
    d3dptr->device = d3d11_device_interface;
}