#ifndef _D3DRENDER_H
#define _D3DRENDER_H

#include <d3d9.h>
#include "unistr.h"

class CD3DFont;
class CD3DRender;

#define FCR_NONE	0x0
#define FCR_BOLD 	0x1
#define FCR_ITALICS 0x2

#define FT_NONE		0x0
#define FT_CENTER	0x1
#define FT_BORDER	0x2
#define FT_VCENTER	0x4
#define FT_SINGLELINE 0x8

//FVF Macros
#define D3DFVF_BITMAPFONT	(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)
#define D3DFVF_PRIMITIVES	(D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

//Vertex Types
struct d3dfont_s { float x,y,z,rhw; DWORD colour; float tu,tv; };
typedef struct { float x,y,z,rhw; DWORD colour; } d3dprimitives_s;

//Initialization Macros
#undef SAFE_RELEASE
#define SAFE_RELEASE( pInterface ) if( pInterface ) { pInterface->Release(); pInterface = 0; }

class CD3DBaseRender
{
public:
	CD3DBaseRender();
	~CD3DBaseRender();

	static HRESULT BeginRender();
	static HRESULT EndRender();

protected:
	static HRESULT Initialize(IDirect3DDevice9 *pD3Ddev);
	static HRESULT Invalidate();

	static HRESULT CreateStates();
	static HRESULT DeleteStates();

	static IDirect3DDevice9		*m_pD3Ddev;
	static IDirect3DStateBlock9	*m_pD3DstateDraw;
	static IDirect3DStateBlock9	*m_pD3DstateNorm;

	static int	m_renderCount;
	static int	m_numShared;
	static bool m_statesOK;
};

class CD3DRender : public CD3DBaseRender
{
public:
	CD3DRender(int numVertices);
	~CD3DRender();

	HRESULT Initialize(IDirect3DDevice9 *pD3Ddev);
	HRESULT Invalidate();

	HRESULT Begin(D3DPRIMITIVETYPE primType);
	HRESULT End();

	inline HRESULT D3DColour(DWORD colour);
	inline HRESULT D3DVertex2f(float x, float y);

	HRESULT D3DAddQuad( int x, int y, int w, int h, DWORD dwColor );
	
private:
	D3DPRIMITIVETYPE		m_primType;
	IDirect3DVertexBuffer9	*m_pD3Dbuf;
	d3dprimitives_s			*m_pVertex;

	DWORD					m_colour;
	int						m_maxVertex;
	int						m_curVertex;

	bool					m_canRender;
};

class CD3DFont : public CD3DBaseRender
{
public:
	CD3DFont( uichar *szFontName, int fontHeight, DWORD dwCreateFlags = 0 );
	~CD3DFont();

	HRESULT Initialize(IDirect3DDevice9 *pD3Ddev);
	HRESULT Invalidate();

	HRESULT Print( float x, float y, DWORD colour, const uichar *szText, DWORD dwFlags = 0 );

	int DrawLength( const uichar* );
	int DrawHeight() const { return static_cast<int>( m_fChrHeight ); };

private:	
	HRESULT CreateVertexBuffers();
	HRESULT DeleteVertexBuffers();

	uichar 	m_szFontName[31+1];
	int  	m_fontHeight;
	DWORD 	m_dwCreateFlags;

	bool 	m_isReady;

	IDirect3DTexture9 		*m_pD3Dtex;
	IDirect3DVertexBuffer9 	*m_pD3Dbuf;
	CD3DRender				*m_pRender;

	DWORD 	m_maxTriangles;

	int 	m_texWidth, m_texHeight;
	int 	m_chrSpacing;
	float 	m_fTexCoords[256][4];
	float 	m_fChrHeight;		
};

#endif