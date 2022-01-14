ID3D11Buffer* global_vertexBuffer_quad;

typedef struct {

	//NOTE: Blob used by CreateInputLayout to get the layout of the semantics implicitly. Can release this once we've created all layouts for our shaders
	ID3DBlob* vsBlob;
	ID3D11VertexShader* vertexShader;

	ID3D11PixelShader* pixelShader;
} d3d_shader_program;


typedef struct {
	ID3D11Device1* d3d11Device;
	ID3D11DeviceContext1* d3d11DeviceContext;
	IDXGISwapChain1* d3d11SwapChain;

	ID3D11RenderTargetView* default_d3d11FrameBufferView;

	ID3D11InputLayout* default_immutable_model_layout; //NOTE: vertex layout for all immutable mesh data like vertex position in model space, texture coordinates, normals?

	D3D11Buffer* instancing_vertex_buffer;
	D3D11InputLayout* glyph_inputLayout;

	ID3D11SamplerState* samplerState_linearTexture;

	d3d_shader_program sdfFontShader;

} BackendRenderer;

static void d3d_createShaderProgram_vs_ps(char *vs_src, char *ps_src, d3d_shader_program *programToFillOut) {
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
	        return 1;
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
	        return 1;
	    }

	    hResult = d3d11Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
	    assert(SUCCEEDED(hResult));
	    psBlob->Release();
	}

	programToFillOut->vsBlob = vsBlob;
	programToFillOut->vertexShader = vertexShader;
	programToFillOut->pixelShader = pixelShader;

}

static void backendRender_init(BackendRenderer *r) {

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
	    d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
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

	IDXGISwapChain1* d3d11SwapChain = r->d3d11SwapChain;


	// Create Framebuffer Render Target for the swapchain (the default one that represents the screen)
	ID3D11RenderTargetView* default_d3d11FrameBufferView;
	{
	    ID3D11Texture2D* d3d11FrameBuffer;
	    HRESULT hResult = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
	    assert(SUCCEEDED(hResult));

	    hResult = d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, &default_d3d11FrameBufferView);
	    assert(SUCCEEDED(hResult));
	    d3d11FrameBuffer->Release();
	}
	r->default_d3d11FrameBufferView = default_d3d11FrameBufferView;

	{ //NOTE: Create all shader programs

	d3d_createShaderProgram_vs_ps(L"..\\src\\sdf_font.hlsl", L"..\\src\\sdf_font.hlsl", &r->sdfFontShader);
	
	}
		
	//NOTE: Create default vertex buffer shapes like Quad, Cube, Sphere
	{
		// Create Vertex Buffer
		UINT numVerts;
		UINT stride;
		UINT offset;
		{
		    float vertexData[] = { // x, y, u, v
		        -0.5f,  0.5f, 0.f, 0.f, 0.f,
		        0.5f, -0.5f, 0.f, 1.f, 1.f,
		        -0.5f, -0.5f, 0.f, 0.f, 1.f,
		        -0.5f,  0.5f, 0.f, 0.f, 0.f,
		        0.5f,  0.5f, 0.f, 1.f, 0.f,
		        0.5f, -0.5f, 0.f, 1.f, 1.f
		    };
		    stride = 4 * sizeof(float);
		    numVerts = sizeof(vertexData) / stride;
		    offset = 0;

		    D3D11_BUFFER_DESC vertexBufferDesc = {};
		    vertexBufferDesc.ByteWidth = sizeof(vertexData);
		    vertexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;
		    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		    D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

		    HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &global_vertexBuffer_quad);
		    assert(SUCCEEDED(hResult));
		}


		// Create Input Layout
		
		{
			// x, y, z, u, v
		    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
		    {
		        { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		        { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		    };

		    HRESULT hResult = d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), r->sdfFontShader.vsBlob->GetBufferPointer(), r->sdfFontShader.vsBlob->GetBufferSize(), &r->default_immutable_model_layout);
		    assert(SUCCEEDED(hResult));
		    // vsBlob->Release();
		}
	}


	{ //NOTE: create the instancing_vertex_buffer
	    D3D11_BUFFER_DESC vertexBufferDesc = {};
	    vertexBufferDesc.ByteWidth = 0;
	    vertexBufferDesc.Usage     = D3D11_USAGE_DYNAMIC;
	    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	    HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, 0, &r->instancing_vertex_buffer);
	    assert(SUCCEEDED(hResult));
	}

	//TODO: Look up whether the semantics names have to be a certain thing or can be anything

	{ //NOTE: Create the input layout for the Glyph elements
	    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	    {
	        { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
	        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
	        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 0 }
	    };

	    HRESULT hResult = d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), r->sdfFontShader.vsBlob->GetBufferPointer(), r->sdfFontShader.vsBlob->GetBufferSize(), &r->glyph_inputLayout);
	    assert(SUCCEEDED(hResult));
	    

	    /*
		https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createinputlayout
	    Once an input-layout object is created from a shader signature, the input-layout object can be reused with any other shader that has an identical input signature (semantics included). This can simplify the creation of input-layout objects when you are working with many shaders with identical inputs.

	    If a data type in the input-layout declaration does not match the data type in a shader-input signature, CreateInputLayout will generate a warning during compilation. The warning is simply to call attention to the fact that the data may be reinterpreted when read from a register. You may either disregard this warning (if reinterpretation is intentional) or make the data types match in both declarations to eliminate the warning.

	    */
	}

	{ // Create Sampler State
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
}

static void backendRender_processCommandBuffer(Renderer *r, BackendRenderer *backend_r) {

	for(int i = 0; i < r->commandCount; ++i) {
		RenderCommand *c = r->commands + i;

		switch(c->type) {
			case RENDER_NULL: {
				assert(false);
			} break;
			case RENDER_GLYPH: {

				float *data = r->glyphInstanceData + c->offset;
				int sizeInBytes = c->size;
				


				// d3d11DeviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);
				// d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
				// d3d11DeviceContext->Draw(numVerts, 0);
				
			} break;
			case RENDER_MATRIX: {

			} break;
		}
	}
}

static void backendRender_presentFrame(BackendRenderer *r) {
	r->d3d11SwapChain->Present(1, 0);
}