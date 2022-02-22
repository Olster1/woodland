static ID3D11Buffer* global_vertexBuffer_quad;
static void* global_testTexture;

typedef struct {

	//NOTE: Blob used by CreateInputLayout to get the layout of the semantics implicitly. Can release this once we've created all layouts for our shaders
	ID3DBlob* vsBlob;
	ID3D11VertexShader* vertexShader;

	ID3D11PixelShader* pixelShader;
} d3d_shader_program;


d3d_shader_program sdfFontShader;
d3d_shader_program textureShader;
d3d_shader_program rectOutlineShader;


//NOTE: Example of 16byte aligned struct 
// typedef struct {
//     float2 pos;
//     float2 paddingUnused; // color (below) needs to be 16-byte aligned! 
//     float4 color;
// } d3d_Constants;

typedef struct {
    float16 orthoMatrix;
} d3d_Constants;


typedef struct {
	ID3D11Device1* d3d11Device;
	ID3D11DeviceContext1* d3d11DeviceContext;
	IDXGISwapChain1* d3d11SwapChain;

	ID3D11RenderTargetView* default_d3d11FrameBufferView;

	// ID3D11InputLayout* default_immutable_model_layout; //NOTE: vertex layout for all immutable mesh data like vertex position in model space, texture coordinates, normals?

	ID3D11Buffer* instancing_vertex_buffer;
	ID3D11InputLayout* glyph_inputLayout;

	ID3D11SamplerState* samplerState_linearTexture;

	ID3D11BlendState *m_blendMode;

	ID3D11Buffer* constantBuffer; //NOTE: Used across all shaders to set the different 'uniform' matrices like MVP matrix and orthographic matrix

	HWND window_hwnd;

	d3d_Constants constants;

	UINT glyph_instance_buffer_stride;
	UINT quad_stride;
	UINT quad_numVerts;

	ID3D11ShaderResourceView* testTexture;

		


} BackendRenderer;



static ID3D11ShaderResourceView* d3d_loadFromFileToGPU(ID3D11Device1 *d3d11Device, char *image_to_load_utf8) {
	// Load Image
	int texWidth;
	int texHeight;
	unsigned char *testTextureBytes = (unsigned char *)stbi_load(image_to_load_utf8, &texWidth, &texHeight, 0, STBI_rgb_alpha);
	int texBytesPerRow = 4 * texWidth;

	assert(testTextureBytes);

	// Create Texture
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width              = texWidth;
	textureDesc.Height             = texHeight;
	textureDesc.MipLevels          = 1;
	textureDesc.ArraySize          = 1;
	textureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureDesc.SampleDesc.Count   = 1;
	textureDesc.Usage              = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA textureSubresourceData = {};
	textureSubresourceData.pSysMem = testTextureBytes;
	textureSubresourceData.SysMemPitch = texBytesPerRow;

	ID3D11Texture2D* texture;
	d3d11Device->CreateTexture2D(&textureDesc, &textureSubresourceData, &texture);

	ID3D11ShaderResourceView* textureView;
	d3d11Device->CreateShaderResourceView(texture, nullptr, &textureView);

	free(testTextureBytes);

	return textureView;
}

static ID3D11ShaderResourceView* d3d_loadFromFileToGPU_array(ID3D11Device1 *d3d11Device, char *image_to_load_utf8_array, int arrayCount) {
	// Load Image
	int texWidth;
	int texHeight;
	unsigned char *testTextureBytes = (unsigned char *)stbi_load("..\\src\\green.png", &texWidth, &texHeight, 0, STBI_rgb_alpha);
	unsigned char *testTextureBytes1 = (unsigned char *)stbi_load("..\\src\\blue.png", &texWidth, &texHeight, 0, STBI_rgb_alpha);
	unsigned char *testTextureBytes2 = (unsigned char *)stbi_load("..\\src\\orange.png", &texWidth, &texHeight, 0, STBI_rgb_alpha);
	int texBytesPerRow = 4 * texWidth;

	assert(testTextureBytes);

	// Create Texture
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width              = texWidth;
	textureDesc.Height             = texHeight;
	textureDesc.MipLevels          = 1;
	textureDesc.ArraySize          = arrayCount;
	textureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureDesc.SampleDesc.Count   = 1;
	textureDesc.Usage              = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA textureSubresourceDatas[3] = {};
	textureSubresourceDatas[0].pSysMem = testTextureBytes;
	textureSubresourceDatas[0].SysMemPitch = texBytesPerRow;

	textureSubresourceDatas[1].pSysMem = testTextureBytes1;
	textureSubresourceDatas[1].SysMemPitch = texBytesPerRow;

	textureSubresourceDatas[2].pSysMem = testTextureBytes2;
	textureSubresourceDatas[2].SysMemPitch = texBytesPerRow;

	ID3D11Texture2D* texture;
	d3d11Device->CreateTexture2D(&textureDesc, textureSubresourceDatas, &texture);

	ID3D11ShaderResourceView* textureView;
	d3d11Device->CreateShaderResourceView(texture, nullptr, &textureView);

	free(testTextureBytes);

	return textureView;
}

static void d3d_createShaderProgram_vs_ps(ID3D11Device1 *d3d11Device, LPCWSTR vs_src, LPCWSTR ps_src, d3d_shader_program *programToFillOut) {
	// Create Vertex Shader
	ID3DBlob* vsBlob;
	ID3D11VertexShader* vertexShader;
	{
	    ID3DBlob* shaderCompileErrorsBlob;
	    HRESULT hResult = D3DCompileFromFile(vs_src, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &vsBlob, &shaderCompileErrorsBlob);
	    if(FAILED(hResult))
	    {
	        const char* errorString = NULL;
	        if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	            errorString = "Could not compile shader; file not found";
	        else if(shaderCompileErrorsBlob){
	            errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
	            shaderCompileErrorsBlob->Release();
	        }
	        MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
	    }

	    hResult = d3d11Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
	    assert(SUCCEEDED(hResult));
	}


	// Create Pixel Shader
	ID3D11PixelShader* pixelShader;
	{
	    ID3DBlob* psBlob;
	    ID3DBlob* shaderCompileErrorsBlob;
	    HRESULT hResult = D3DCompileFromFile(ps_src, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &psBlob, &shaderCompileErrorsBlob);
	    if(FAILED(hResult))
	    {
	        const char* errorString = NULL;
	        if(hResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	            errorString = "Could not compile shader; file not found";
	        else if(shaderCompileErrorsBlob){
	            errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
	            shaderCompileErrorsBlob->Release();
	        }
	        MessageBoxA(0, errorString, "Shader Compiler Error", MB_ICONERROR | MB_OK);
	    }

	    hResult = d3d11Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
	    assert(SUCCEEDED(hResult));
	    psBlob->Release();
	}

	programToFillOut->vsBlob = vsBlob;
	programToFillOut->vertexShader = vertexShader;
	programToFillOut->pixelShader = pixelShader;

}

static void d3d_create_shader_from_RC_file(ID3D11Device1 *d3d11Device, int id, d3d_shader_program *shader)
{
    HRSRC res = FindResource(GetModuleHandle(0), MAKEINTRESOURCE(id), RT_RCDATA);
    HGLOBAL handle = LoadResource(0, res);
    void* data = LockResource(handle);
    DWORD size = SizeofResource(0, res);

    LPCWSTR str = (LPCWSTR)data;

    d3d_createShaderProgram_vs_ps(d3d11Device, str, str, shader);

}

static void getDefaultFrameBuffer_fromSwapChain(BackendRenderer *r) {
	IDXGISwapChain1* d3d11SwapChain = r->d3d11SwapChain;
	// Create Framebuffer Render Target for the swapchain (the default one that represents the screen)
	ID3D11RenderTargetView* default_d3d11FrameBufferView;
	{
	    ID3D11Texture2D* d3d11FrameBuffer;
	    HRESULT hResult = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
	    assert(SUCCEEDED(hResult));

	    hResult = r->d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, &default_d3d11FrameBufferView);
	    assert(SUCCEEDED(hResult));
	    d3d11FrameBuffer->Release();
	}
	r->default_d3d11FrameBufferView = default_d3d11FrameBufferView;
}


static void d3d_release_and_resize_default_frame_buffer(BackendRenderer *backendRenderer) {
    backendRenderer->d3d11DeviceContext->OMSetRenderTargets(0, 0, 0);
    backendRenderer->default_d3d11FrameBufferView->Release();
    // depthBufferView->Release();

    HRESULT res = backendRenderer->d3d11SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    assert(SUCCEEDED(res));

    getDefaultFrameBuffer_fromSwapChain(backendRenderer);
    
}

static UINT backendRender_init(BackendRenderer *r, HWND hwnd) {

	r->window_hwnd = hwnd;
	// Create D3D11 Device and Context
	{
	    ID3D11Device* baseDevice;
	    ID3D11DeviceContext* baseDeviceContext;
	    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 }; //we just want d3d 11 features, not below
	    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; 
	    #if defined(DEBUG_BUILD)
	    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	    #endif

	    HRESULT hResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, //hardware rendering instead of software rendering
	                                        0, creationFlags, 
	                                        featureLevels, ARRAYSIZE(featureLevels),  //feature levels: we want direct11 features - don't want any below
	                                        D3D11_SDK_VERSION, &baseDevice, 
	                                        0, &baseDeviceContext);
	    if(FAILED(hResult)){
	        MessageBoxA(0, "D3D11CreateDevice() failed", "Fatal Error", MB_OK);
	        return GetLastError();
	    }
	    
	    // Get 1.1 interface of D3D11 Device and Context
	    hResult = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&r->d3d11Device);
	    assert(SUCCEEDED(hResult));
	    baseDevice->Release();

	    hResult = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&r->d3d11DeviceContext);
	    assert(SUCCEEDED(hResult));
	    baseDeviceContext->Release();
	}

	ID3D11Device1* d3d11Device = r->d3d11Device;
	ID3D11DeviceContext1* d3d11DeviceContext = r->d3d11DeviceContext;
	

	#ifdef DEBUG_BUILD
	    // Set up debug layer to break on D3D11 errors
	    ID3D11Debug *d3dDebug = nullptr;
	    d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
	    if (d3dDebug)
	    {
	        ID3D11InfoQueue *d3dInfoQueue = nullptr;
	        if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
	        {
	            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
	            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
	            d3dInfoQueue->Release();
	        }
	        d3dDebug->Release();
	    }
	#endif



	// Create Swap Chain
	{ 
	    // Get DXGI Factory (needed to create Swap Chain)
	    IDXGIFactory2* dxgiFactory;
	    {
	        IDXGIDevice1* dxgiDevice;
	        HRESULT hResult = d3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);
	        assert(SUCCEEDED(hResult));

	        IDXGIAdapter* dxgiAdapter;
	        hResult = dxgiDevice->GetAdapter(&dxgiAdapter);
	        assert(SUCCEEDED(hResult));
	        dxgiDevice->Release();

	        DXGI_ADAPTER_DESC adapterDesc;
	        dxgiAdapter->GetDesc(&adapterDesc);

	        OutputDebugStringA("Graphics Device: \n");
	        OutputDebugStringW(adapterDesc.Description);

	        hResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
	        assert(SUCCEEDED(hResult));
	        dxgiAdapter->Release();
	    }

	    DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};
	    d3d11SwapChainDesc.Width = 0; // use window width
	    d3d11SwapChainDesc.Height = 0; // use window height
	    d3d11SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //_SRGB we dont do srgb anymore, otherwise we have to square all our colors
	    d3d11SwapChainDesc.SampleDesc.Count = 1;
	    d3d11SwapChainDesc.SampleDesc.Quality = 0;
	    d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	    d3d11SwapChainDesc.BufferCount = 2;
	    d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	    d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	    d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	    d3d11SwapChainDesc.Flags = 0;

	    HRESULT hResult = dxgiFactory->CreateSwapChainForHwnd(d3d11Device, hwnd, &d3d11SwapChainDesc, 0, 0, &r->d3d11SwapChain);
	    assert(SUCCEEDED(hResult));

	    dxgiFactory->Release();
	}



	getDefaultFrameBuffer_fromSwapChain(r);

	{ //NOTE: Create all shader programs

#if DEBUG_BUILD
		
		d3d_createShaderProgram_vs_ps(d3d11Device, L"..\\src\\sdf_font.hlsl", L"..\\src\\sdf_font.hlsl", &sdfFontShader);
		d3d_createShaderProgram_vs_ps(d3d11Device, L"..\\src\\texture.hlsl", L"..\\src\\texture.hlsl", &textureShader);
		d3d_createShaderProgram_vs_ps(d3d11Device, L"..\\src\\rect_outline.hlsl", L"..\\src\\rect_outline.hlsl", &rectOutlineShader);
		
#else 

		d3d_createShaderProgram_vs_ps(d3d11Device, L".\\shaders\\sdf_font.hlsl", L".\\shaders\\sdf_font.hlsl", &sdfFontShader);
		d3d_createShaderProgram_vs_ps(d3d11Device, L".\\shaders\\texture.hlsl", L".\\shaders\\texture.hlsl", &textureShader);
		d3d_createShaderProgram_vs_ps(d3d11Device, L".\\shaders\\rect_outline.hlsl", L".\\shaders\\rect_outline.hlsl", &rectOutlineShader);
	
#endif
	
	}
		
	//NOTE: Create default vertex buffer shapes like Quad, Cube, Sphere
	{
		// Create Vertex Buffer
		{
		    float vertexData[] = { // x, y, z, u, v
		        -0.5f,  0.5f, 0.f, 0.f, 0.f,
		        0.5f, -0.5f, 0.f, 1.f, 1.f,
		        -0.5f, -0.5f, 0.f, 0.f, 1.f,
		        -0.5f,  0.5f, 0.f, 0.f, 0.f,
		        0.5f,  0.5f, 0.f, 1.f, 0.f,
		        0.5f, -0.5f, 0.f, 1.f, 1.f
		    };
		    r->quad_stride = 5 * sizeof(float);
		    r->quad_numVerts = sizeof(vertexData) / r->quad_stride;

		    D3D11_BUFFER_DESC vertexBufferDesc = {};
		    vertexBufferDesc.ByteWidth = sizeof(vertexData);
		    vertexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;
		    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		    D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

		    HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &global_vertexBuffer_quad);
		    assert(SUCCEEDED(hResult));
		}
	}

	// Create Input Layout


	{ //NOTE: Create the input layout for the Glyph elements
	    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	    {
	    	{ "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	    	{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	        { "POS_INSTANCE", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	        { "SCALE_INSTANCE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	        { "COLOR_INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, //NOTE: 1 at the end to say advance every instance, the reason this could be more than 1 is that the instance data might be for every 4 instances like each side of a face if each side represents the an instance, than if we wanted it to be the same color for all faces.  
	        { "TEXCOORD_INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	        { "TEX_ARRAY_INDEX", 0, DXGI_FORMAT_R32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
	    };

	    HRESULT hResult = d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), sdfFontShader.vsBlob->GetBufferPointer(), sdfFontShader.vsBlob->GetBufferSize(), &r->glyph_inputLayout);
	    assert(SUCCEEDED(hResult));
	    

	    /*
		https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createinputlayout
	    Once an input-layout object is created from a shader signature, the input-layout object can be reused with any other shader that has an identical input signature (semantics included). This can simplify the creation of input-layout objects when you are working with many shaders with identical inputs.

	    If a data type in the input-layout declaration does not match the data type in a shader-input signature, CreateInputLayout will generate a warning during compilation. The warning is simply to call attention to the fact that the data may be reinterpreted when read from a register. You may either disregard this warning (if reinterpretation is intentional) or make the data types match in both declarations to eliminate the warning.

	    */
	}
	
	{ //NOTE: create the instancing_vertex_buffer
	    D3D11_BUFFER_DESC vertexBufferDesc = {};
	    vertexBufferDesc.ByteWidth = (UINT)GLYPH_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES;
	    vertexBufferDesc.Usage     = D3D11_USAGE_DYNAMIC;
	    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	    r->glyph_instance_buffer_stride = SIZE_OF_GLYPH_INSTANCE_IN_BYTES;

	    HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, 0, &r->instancing_vertex_buffer);
	    assert(SUCCEEDED(hResult));
	}

	//TODO: Look up whether the semantics names have to be a certain thing or can be anything

	{ // Create Sampler State for texture sampling
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 1.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		d3d11Device->CreateSamplerState(&samplerDesc, &r->samplerState_linearTexture);
	}

	{

		D3D11_BLEND_DESC blendDesc;
		ZeroMemory(&blendDesc, sizeof(blendDesc));

		D3D11_RENDER_TARGET_BLEND_DESC rtbd;
		ZeroMemory(&rtbd, sizeof(rtbd));

		rtbd.BlendEnable = true;
		rtbd.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rtbd.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rtbd.BlendOp = D3D11_BLEND_OP_ADD;

		rtbd.SrcBlendAlpha = D3D11_BLEND_ONE;
		rtbd.DestBlendAlpha = D3D11_BLEND_ZERO;
		rtbd.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		
		rtbd.RenderTargetWriteMask = 0x0f;
		blendDesc.RenderTarget[0] = rtbd;

		d3d11Device->CreateBlendState(&blendDesc, &r->m_blendMode);

		float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		UINT sampleMask   = 0xffffffff;

		d3d11DeviceContext->OMSetBlendState(r->m_blendMode, blendFactor, sampleMask);
	}

	//NOTE: Rasterizer state
	{
	   ID3D11RasterizerState1 *raster_state;

	   D3D11_RASTERIZER_DESC1 rasterizerState;
	   rasterizerState.FillMode = D3D11_FILL_SOLID;
	   rasterizerState.CullMode = D3D11_CULL_FRONT;
	   rasterizerState.FrontCounterClockwise = true;
	   rasterizerState.DepthBias = false;
	   rasterizerState.DepthBiasClamp = 0;
	   rasterizerState.SlopeScaledDepthBias = 0;
	   rasterizerState.DepthClipEnable = true;
	   rasterizerState.ScissorEnable = true;
	   rasterizerState.MultisampleEnable = false;
	   rasterizerState.AntialiasedLineEnable = false;
	   rasterizerState.ForcedSampleCount = 0;
	   d3d11Device->CreateRasterizerState1( &rasterizerState, &raster_state );	

	   d3d11DeviceContext->RSSetState(raster_state);
	}

	{
		//NOTE: Create the constant buffer
		{
		    D3D11_BUFFER_DESC constantBufferDesc = {};

		    // ByteWidth must be a multiple of 16, per the docs
		    constantBufferDesc.ByteWidth      = sizeof(d3d_Constants) + 0xf & 0xfffffff0;
		    constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
		    constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		    HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &r->constantBuffer);
		    assert(SUCCEEDED(hResult));
		}

	}

	// r->testTexture = d3d_loadFromFileToGPU_array(d3d11Device, "..\\src\\testTexture.png", 3);
	// global_testTexture = r->testTexture = d3d_loadFromFileToGPU(d3d11Device, "..\\src\\testTexture.png");

#if DEBUG_BUILD
	if(!global_white_texture) {
		global_white_texture = (void *)d3d_loadFromFileToGPU(d3d11Device, "..\\src\\white_texture.png");
	}
#else 
	if(!global_white_texture) {
		global_white_texture = (void *)d3d_loadFromFileToGPU(d3d11Device, ".\\white_texture.png");
	}
#endif


	return 0;
}

static void d3d_setGlobalConstantBuffer(BackendRenderer *r) {
	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	r->d3d11DeviceContext->Map(r->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	d3d_Constants* constants = (d3d_Constants*)(mappedSubresource.pData);

	*constants = r->constants;

	r->d3d11DeviceContext->Unmap(r->constantBuffer, 0);


	r->d3d11DeviceContext->VSSetConstantBuffers(0, 1, &r->constantBuffer);

}

static void backendRender_processCommandBuffer(Renderer *r, BackendRenderer *backend_r) {

	ID3D11DeviceContext *d3d11DeviceContext = backend_r->d3d11DeviceContext;

#if DEBUG_BUILD
	global_debug_stats.draw_call_count = 0;
    global_debug_stats.render_command_count = r->commandCount;
#endif

	for(int i = 0; i < r->commandCount; ++i) {
		RenderCommand *c = r->commands + i;

		switch(c->type) {
			case RENDER_NULL: {
				assert(false);
			} break;
			case RENDER_SET_VIEWPORT: {
				RECT winRect;
				GetClientRect(backend_r->window_hwnd, &winRect);
				D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(winRect.right - winRect.left), (FLOAT)(winRect.bottom - winRect.top), 0.0f, 1.0f };
				d3d11DeviceContext->RSSetViewports(1, &viewport);

			} break;
			case RENDER_CLEAR_COLOR_BUFFER: {
				FLOAT backgroundColor[4] = { c->color.x, c->color.y, c->color.z, c->color.w };
				d3d11DeviceContext->ClearRenderTargetView(backend_r->default_d3d11FrameBufferView, backgroundColor);
			} break;
			case RENDER_MATRIX: {
				backend_r->constants.orthoMatrix = c->matrix;
				
			} break;
			case RENDER_SET_SHADER: {
				d3d_shader_program *program = (d3d_shader_program *)c->shader;

				d3d11DeviceContext->VSSetShader(program->vertexShader, nullptr, 0);
				d3d11DeviceContext->PSSetShader(program->pixelShader, nullptr, 0);
			} break;
			case RENDER_SET_SCISSORS: {
				D3D11_RECT rect = { (LONG)c->scissors_bounds.minX, (LONG)c->scissors_bounds.minY, (LONG)c->scissors_bounds.maxX, (LONG)c->scissors_bounds.maxY };

				d3d11DeviceContext->RSSetScissorRects(1, &rect);
			} break;
			case RENDER_GLYPH: {

				u8 *data = r->glyphInstanceData + c->offset_in_bytes;
				int sizeInBytes = c->size_in_bytes;

				d3d_setGlobalConstantBuffer(backend_r);

				//NOTE: Update the vertex buffer
				D3D11_MAPPED_SUBRESOURCE mappedSubresource;
				d3d11DeviceContext->Map(backend_r->instancing_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
				u8* data_on_gpu = (u8*)(mappedSubresource.pData);
				
				memcpy(data_on_gpu, data, sizeInBytes);

				d3d11DeviceContext->Unmap(backend_r->instancing_vertex_buffer, 0);
				//////////
				

				d3d11DeviceContext->OMSetRenderTargets(1, &backend_r->default_d3d11FrameBufferView, nullptr);

				d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3d11DeviceContext->IASetInputLayout(backend_r->glyph_inputLayout);

				

				// EditorState *editorState = (EditorState *)global_platform.permanent_storage;
				
				// GlyphInfo glyph = easyFont_getGlyph(&editorState->font, (u32)'A');

				// ID3D11ShaderResourceView* fontTextureView = (ID3D11ShaderResourceView *)glyph.handle;
				// d3d11DeviceContext->PSSetShaderResources(0, 1, &fontTextureView);

				// ID3D11ShaderResourceView* textureView

				//NOTE: Set the texture array
				assert(c->textureHandle_count > 0);
				// c->texture_handles;

				// d3d11DeviceContext->PSSetShaderResources(0, 1, &backend_r->testTexture);

				//NOTE: We just use the first texture handle
				ID3D11ShaderResourceView *texture = (ID3D11ShaderResourceView *)c->texture_handles[0];

				d3d11DeviceContext->PSSetShaderResources(0, 1, &texture);
				d3d11DeviceContext->PSSetSamplers(0, 1, &backend_r->samplerState_linearTexture);

				ID3D11Buffer *vertex_buffers[] = {global_vertexBuffer_quad, backend_r->instancing_vertex_buffer};
				UINT buffer_strides[] = {backend_r->quad_stride, backend_r->glyph_instance_buffer_stride};
				UINT offsets[] = {0, 0};

				d3d11DeviceContext->IASetVertexBuffers(0, 2, vertex_buffers, buffer_strides, offsets);

				d3d11DeviceContext->DrawInstanced(backend_r->quad_numVerts, c->instanceCount, 0, 0);


				#if DEBUG_BUILD
				    global_debug_stats.draw_call_count++;
				#endif

			} break;
			case RENDER_TEXTURE: {

				u8 *data = r->textureInstanceData + c->offset_in_bytes;
				int sizeInBytes = c->size_in_bytes;

				d3d_setGlobalConstantBuffer(backend_r);

				//NOTE: Update the vertex buffer
				D3D11_MAPPED_SUBRESOURCE mappedSubresource;
				d3d11DeviceContext->Map(backend_r->instancing_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
				u8* data_on_gpu = (u8*)(mappedSubresource.pData);
				
				memcpy(data_on_gpu, data, sizeInBytes);

				d3d11DeviceContext->Unmap(backend_r->instancing_vertex_buffer, 0);
				//////////
				

				d3d11DeviceContext->OMSetRenderTargets(1, &backend_r->default_d3d11FrameBufferView, nullptr);

				d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d3d11DeviceContext->IASetInputLayout(backend_r->glyph_inputLayout);

				// float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				// UINT sampleMask   = 0xffffffff;

				// d3d11DeviceContext->OMSetBlendState(backend_r->m_blendMode, blendFactor, sampleMask);

				// EditorState *editorState = (EditorState *)global_platform.permanent_storage;
				
				// GlyphInfo glyph = easyFont_getGlyph(&editorState->font, (u32)'A');

				// ID3D11ShaderResourceView* fontTextureView = (ID3D11ShaderResourceView *)glyph.handle;
				// d3d11DeviceContext->PSSetShaderResources(0, 1, &fontTextureView);

				// ID3D11ShaderResourceView* textureView

				//NOTE: Set the texture array
				assert(c->textureHandle_count > 0);
				// c->texture_handles;

				// d3d11DeviceContext->PSSetShaderResources(0, 1, &backend_r->testTexture);

				//NOTE: We just use the first texture handle
				ID3D11ShaderResourceView *texture = (ID3D11ShaderResourceView *)c->texture_handles[0];

				d3d11DeviceContext->PSSetShaderResources(0, 1, &texture);
				d3d11DeviceContext->PSSetSamplers(0, 1, &backend_r->samplerState_linearTexture);

				ID3D11Buffer *vertex_buffers[] = {global_vertexBuffer_quad, backend_r->instancing_vertex_buffer};
				UINT buffer_strides[] = {backend_r->quad_stride, backend_r->glyph_instance_buffer_stride};
				UINT offsets[] = {0, 0};

				d3d11DeviceContext->IASetVertexBuffers(0, 2, vertex_buffers, buffer_strides, offsets);

				d3d11DeviceContext->DrawInstanced(backend_r->quad_numVerts, c->instanceCount, 0, 0);


				#if DEBUG_BUILD
				    global_debug_stats.draw_call_count++;
				#endif



			} break;
			default: {

			}
		}
	}
}

static void backendRender_presentFrame(BackendRenderer *r) {
	r->d3d11SwapChain->Present(1, 0);
}