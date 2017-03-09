// license:BSD-3-Clause
// copyright-holders:Aaron Giles
//============================================================
//
//  d3dintf.h - Direct3D 8/9 interface abstractions
//
//============================================================

#ifndef __WIN_D3DINTF__
#define __WIN_D3DINTF__

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3d11.h>
#include <math.h>
#undef interface

//============================================================
//  CONSTANTS
//============================================================

#ifndef D3DCAPS2_DYNAMICTEXTURES
#define D3DCAPS2_DYNAMICTEXTURES 0x20000000L
#endif

#ifndef D3DPRESENT_DONOTWAIT
#define D3DPRESENT_DONOTWAIT 0x00000001L
#endif


#define D3DTSS_ADDRESSU       13
#define D3DTSS_ADDRESSV       14
#define D3DTSS_BORDERCOLOR    15
#define D3DTSS_MAGFILTER      16
#define D3DTSS_MINFILTER      17
#define D3DTSS_MIPFILTER      18
#define D3DTSS_MIPMAPLODBIAS  19
#define D3DTSS_MAXMIPLEVEL    20
#define D3DTSS_MAXANISOTROPY  21

//============================================================
//  TYPE DEFINITIONS
//============================================================

struct context11;
struct d3d_base;
struct d3d11_base;
struct device;
struct device11;
struct surface;
struct surface11;
struct texture;
struct texture11;
struct texture11_view;
struct vertex_buffer;
struct index_buffer;
struct buffer11;
class effect;
typedef D3DXVECTOR4 vector;
typedef D3DMATRIX matrix;

//============================================================
//  Abstracted presentation parameters
//============================================================

struct present_parameters
{
	UINT BackBufferWidth;
	UINT BackBufferHeight;
	D3DFORMAT BackBufferFormat;
	UINT BackBufferCount;
	D3DMULTISAMPLE_TYPE MultiSampleType;
	DWORD MultiSampleQuality;
	D3DSWAPEFFECT SwapEffect;
	HWND hDeviceWindow;
	BOOL Windowed;
	BOOL EnableAutoDepthStencil;
	D3DFORMAT AutoDepthStencilFormat;
	DWORD Flags;
	UINT FullScreen_RefreshRateInHz;
	UINT PresentationInterval;
};


//============================================================
//  Abstracted device identifier
//============================================================

struct adapter_identifier
{
	char            Driver[512];
	char            Description[512];
	LARGE_INTEGER   DriverVersion;
	DWORD           VendorId;
	DWORD           DeviceId;
	DWORD           SubSysId;
	DWORD           Revision;
	GUID            DeviceIdentifier;
	DWORD           WHQLLevel;
};


//============================================================
//  Caps enumeration
//============================================================

enum caps_index
{
	CAPS_PRESENTATION_INTERVALS,
	CAPS_CAPS2,
	CAPS_DEV_CAPS,
	CAPS_SRCBLEND_CAPS,
	CAPS_DSTBLEND_CAPS,
	CAPS_TEXTURE_CAPS,
	CAPS_TEXTURE_FILTER_CAPS,
	CAPS_TEXTURE_ADDRESS_CAPS,
	CAPS_TEXTURE_OP_CAPS,
	CAPS_MAX_TEXTURE_ASPECT,
	CAPS_MAX_TEXTURE_WIDTH,
	CAPS_MAX_TEXTURE_HEIGHT,
	CAPS_STRETCH_RECT_FILTER,
	CAPS_MAX_PS30_INSN_SLOTS
};


//============================================================
//  Direct3D interfaces
//============================================================

struct d3d_interface
{
	HRESULT  (*check_device_format)(d3d_base *d3dptr, UINT adapter, D3DDEVTYPE devtype, D3DFORMAT adapterformat, DWORD usage, D3DRESOURCETYPE restype, D3DFORMAT format);
	HRESULT  (*check_device_type)(d3d_base *d3dptr, UINT adapter, D3DDEVTYPE devtype, D3DFORMAT format, D3DFORMAT backformat, BOOL windowed);
	HRESULT  (*create_device)(d3d_base *d3dptr, UINT adapter, D3DDEVTYPE devtype, HWND focus, DWORD behavior, present_parameters *params, device **dev);
	HRESULT  (*enum_adapter_modes)(d3d_base *d3dptr, UINT adapter, D3DFORMAT format, UINT index, D3DDISPLAYMODE *mode);
	UINT     (*get_adapter_count)(d3d_base *d3dptr);
	HRESULT  (*get_adapter_display_mode)(d3d_base *d3dptr, UINT adapter, D3DDISPLAYMODE *mode);
	HRESULT  (*get_adapter_identifier)(d3d_base *d3dptr, UINT adapter, DWORD flags, adapter_identifier *identifier);
	UINT     (*get_adapter_mode_count)(d3d_base *d3dptr, UINT adapter, D3DFORMAT format);
	HMONITOR (*get_adapter_monitor)(d3d_base *d3dptr, UINT adapter);
	HRESULT  (*get_caps_dword)(d3d_base *d3dptr, UINT adapter, D3DDEVTYPE devtype, caps_index which, DWORD *value);
	ULONG    (*release)(d3d_base *d3dptr);
};

struct d3d_interface11
{
	HRESULT (*create_device)(d3d11_base *d3dptr, device11 **dev11, context11 **context);
	HRESULT (*effect_compiler)(d3d11_base *d3dptr, const char *fileName, D3D_SHADER_MACRO *defines, ID3DInclude *include, const char *entrypoint, const char *target, ID3DBlob **code, ID3DBlob **error_messages);
	void (*release)(d3d11_base *d3dptr);
};


//============================================================
//  Direct3DDevice interfaces
//============================================================

struct d3d_device_interface
{
	HRESULT (*begin_scene)(device *dev);
	HRESULT (*clear)(device *dev, DWORD count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
	HRESULT (*create_offscreen_plain_surface)(device *dev, UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, surface **surface);
	HRESULT (*create_texture)(device *dev, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, texture **texture, HANDLE *shared);
	HRESULT (*create_vertex_buffer)(device *dev, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool, vertex_buffer **buf);
	HRESULT (*create_index_buffer)(device *dev, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, index_buffer **buf);
	HRESULT (*create_render_target)(device *dev, UINT width, UINT height, D3DFORMAT format, surface **surface);
	HRESULT (*draw_primitive)(device *dev, D3DPRIMITIVETYPE type, UINT start, UINT count);
	HRESULT (*draw_indexed_primitive)(device *dev, D3DPRIMITIVETYPE type, INT base_vertex_index, UINT min_index, UINT num_vertex, UINT start_index, UINT primitive_count);
	HRESULT (*end_scene)(device *dev);
	HRESULT (*get_raster_status)(device *dev, D3DRASTER_STATUS *status);
	HRESULT (*get_render_target)(device *dev, DWORD index, surface **surface);
	HRESULT (*get_render_target_data)(device *dev, surface *rendertarget, surface *destsurface);
	HRESULT (*present)(device *dev, const RECT *source, const RECT *dest, HWND override, RGNDATA *dirty, DWORD flags);
	ULONG   (*release)(device *dev);
	HRESULT (*reset)(device *dev, present_parameters *params);
	void    (*set_gamma_ramp)(device *dev, DWORD flags, const D3DGAMMARAMP *ramp);
	HRESULT (*set_render_state)(device *dev, D3DRENDERSTATETYPE state, DWORD value);
	HRESULT (*set_render_target)(device *dev, DWORD index, surface *surf);
	HRESULT (*set_stream_source)(device *dev, UINT number, vertex_buffer *vbuf, UINT stride);
	HRESULT (*set_indicies)(device *dev, index_buffer *ibuf);
	HRESULT (*set_texture)(device *dev, DWORD stage, texture *tex);
	HRESULT (*set_texture_stage_state)(device *dev, DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value);
	HRESULT (*set_vertex_format)(device *dev, D3DFORMAT format);
	HRESULT (*stretch_rect)(device *dev, surface *source, const RECT *srcrect, surface *dest, const RECT *dstrect, D3DTEXTUREFILTERTYPE filter);
	HRESULT (*test_cooperative_level)(device *dev);
};

struct d3d_device_interface11
{
	HRESULT (*create_texture)(device11 *dev, UINT width, UINT height, UINT levels, D3D11_USAGE usage, DXGI_FORMAT format, void *rawpixel, UINT pitch, texture11 **texture, texture11_view **view);
	HRESULT (*create_buffer)(device11 *dev, UINT length, UINT usage, void *data, buffer11 **buf);
	HRESULT (*create_render_target)(device11 *dev, UINT sample, UINT quality, UINT width, UINT height, DXGI_FORMAT format, surface11 **surface, texture11 **texture, texture11_view **view);
	HRESULT (*create_shared_texture)(device11 *dev, HANDLE src, texture11 **dst, texture11_view **view);
	void (*set_render_target)(context11 *con, surface11 *surf, surface11 *depth);
	void (*set_vertex_buffer)(context11 *con, buffer11 *buff, UINT stride, UINT offset);
	void (*set_index_buffer)(context11 *con, buffer11 *buff, DXGI_FORMAT format, UINT offset);
	void (*set_texture)(context11 *con, DWORD stage, texture11_view *view);
	void (*set_viewport)(context11 *con, float topleft_x, float topleft_y, float width, float height, float min_depth, float max_depth);
	void (*clear_render_target11)(context11 *con, surface11 *surf, surface11 *dsurf, float color[4]);
	void (*draw_indexed)(context11 *con, D3D11_PRIMITIVE_TOPOLOGY topology, UINT count, UINT offset, int basevert);
	void (*flush)(context11 *con);
	ULONG (*release)(device11 *dev, context11 *context);
};

//============================================================
//  Direct3DSurface interfaces
//============================================================

struct surface_interface
{
	HRESULT (*lock_rect)(surface *surf, D3DLOCKED_RECT *locked, const RECT *rect, DWORD flags);
	ULONG   (*release)(surface *tex);
	HRESULT (*unlock_rect)(surface *surf);
};


//============================================================
//  Direct3DTexture interfaces
//============================================================

struct texture_interface
{
	HRESULT (*get_surface_level)(texture *tex, UINT level, surface **surface);
	HRESULT (*lock_rect)(texture *tex, UINT level, D3DLOCKED_RECT *locked, const RECT *rect, DWORD flags);
	ULONG   (*release)(texture *tex);
	HRESULT (*unlock_rect)(texture *tex, UINT level);
};


//============================================================
//  Direct3DVertexBuffer interfaces
//============================================================

struct vertex_buffer_interface
{
	HRESULT (*lock)(vertex_buffer *vbuf, UINT offset, UINT size, VOID **data, DWORD flags);
	ULONG   (*release)(vertex_buffer *vbuf);
	HRESULT (*unlock)(vertex_buffer *vbuf);
};


//============================================================
//  Direct3DIndexBuffer interfaces
//============================================================

struct index_buffer_interface
{
	HRESULT (*lock)(index_buffer *ibuf, UINT offset, UINT size, VOID **data, DWORD flags);
	ULONG   (*release)(index_buffer *ibuf);
	HRESULT (*unlock)(index_buffer *ibuf);
};


//============================================================
//  Core D3D object
//============================================================

struct d3d_base
{
	// internal objects
	int                         version;
	void *                      d3dobj;
	HINSTANCE                   dllhandle;
	bool                        post_fx_available;
	HINSTANCE                   libhandle;

	// interface pointers
	d3d_interface               d3d;
	d3d_device_interface    device;
	
	surface_interface       surface;
	texture_interface       texture;
	vertex_buffer_interface vertexbuf;
	index_buffer_interface indexbuf;
};

struct d3d11_base
{
	HINSTANCE               dllhandle_d3d11;
	HINSTANCE               dllhandle_d3dcompiler;
	
	d3d_interface11             d3d;
	d3d_device_interface11	device;
};

//============================================================
//  PROTOTYPES
//============================================================

d3d_base *drawd3d9_init(void);
d3d11_base *drawd3d11_init(void);


#endif
