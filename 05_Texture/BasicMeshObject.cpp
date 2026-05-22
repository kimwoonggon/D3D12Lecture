#include "pch.h"
#include "typedef.h"
#include <d3dcompiler.h>
#include <d3dx12.h>
#include "../D3D_Util/D3DUtil.h"
#include "D3D12ResourceManager.h"
#include "D3D12Renderer.h"
#include "BasicMeshObject.h"


ID3D12RootSignature* CBasicMeshObject::m_pRootSignature = nullptr;
ID3D12PipelineState* CBasicMeshObject::m_pPipelineState = nullptr;
DWORD CBasicMeshObject::m_dwInitRefCount = 0;

CBasicMeshObject::CBasicMeshObject()
{
}

BOOL CBasicMeshObject::Initialize(CD3D12Renderer* pRenderer)
{
	m_pRenderer = pRenderer;

	BOOL bResult = InitCommonResources();
	return bResult;
}
BOOL CBasicMeshObject::InitCommonResources()
{
	if (m_dwInitRefCount)
		goto lb_true;

	InitRootSinagture();
	InitPipelineState();

lb_true:
	m_dwInitRefCount++;
	return m_dwInitRefCount;
}
BOOL CBasicMeshObject::InitRootSinagture()
{
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	/*
	* 이번에 셰이더로 넘길 자원은 SRV(텍스처) 종류이고, 개수는 1개이며, 셰이더의 t0 레지스터부터 시작해라"라는 상세 내역 지정. 
	만약 텍스처를 3개 연속으로 넘기고 싶다면 개수를 3으로 지정하여 t0, t1, t2를 묶을 수도 있다
	*/
	CD3DX12_DESCRIPTOR_RANGE ranges[1] = {}; // 연속된 범위
	
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);	// t0 : texture
	
	/*
    * range로 자원의 종류와 레지스터 번호를 상세히 정의 후 이를 rootparameters에 담아서 큰
	* Descriptor Table 매개변수로 묶어서 셰이더에 전달.
	*/
	CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
	/*
	* 만약 Pixel Shader에서 100% 쓰이는 것이 확실하면 D3D12_SHADER_VISIBILITY_PIXEL로 지정하여 Pixel Shader에서만
	* 접근 가능하도록 한다.
	* InitAsConstants(), InitAsConstantBufferView()가 있다. 이 경우 heap에 자원을 올려서 Descriptor Table로 넘기는 방식이 아니어도 된다.
	*/
	rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);

	// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	SetDefaultSamplerDesc(&sampler, 0);
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	// Allow input layout and deny uneccessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Create an root signature.
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	//rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 2. GPU 제조사(NVIDIA/AMD)가 즉시 읽을 수 있는 "압축 바코드 파일"로 변환한 뒤,
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError)))
	{
		__debugbreak();
	}
	// 3. 그 바코드를 찍어서 GPU 내부에 진짜 통로 장치를 생산합니다.
	if (FAILED(pD3DDeivce->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))))
	{
		__debugbreak();
	}
	if (pSignature)
	{
		pSignature->Release();
		pSignature = nullptr;
	}
	if (pError)
	{
		pError->Release();
		pError = nullptr;
	}
	return TRUE;
}
BOOL CBasicMeshObject::InitPipelineState()
{
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();

	ID3DBlob* pVertexShader = nullptr;
	ID3DBlob* pPixelShader = nullptr;


#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	if (FAILED(D3DCompileFromFile(L"./Shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &pVertexShader, nullptr)))
	{
		__debugbreak();
	}
	if (FAILED(D3DCompileFromFile(L"./Shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pPixelShader, nullptr)))
	{
		__debugbreak();
	}


	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	0, 28,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};


	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_pRootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShader->GetBufferPointer(), pVertexShader->GetBufferSize());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShader->GetBufferPointer(), pPixelShader->GetBufferSize());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	if (FAILED(pD3DDeivce->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState))))
	{
		__debugbreak();
	}

	if (pVertexShader)
	{
		pVertexShader->Release();
		pVertexShader = nullptr;
	}
	if (pPixelShader)
	{
		pPixelShader->Release();
		pPixelShader = nullptr;
	}
	return TRUE;
}

BOOL CBasicMeshObject::CreateDescriptorTable()
{

	BOOL bResult = FALSE;
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();

	// 렌더링시 Descriptor Table로 사용할 Descriptor Heap - 
	// Descriptor Table
	// | TEX
	m_srvDescriptorSize = pD3DDeivce->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// create descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = DESCRIPTOR_COUNT_FOR_DRAW;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(pD3DDeivce->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pDescritorHeap))))
	{
		__debugbreak();
		goto lb_return;
	}
	bResult = TRUE;
lb_return:
	return bResult;

}
BOOL CBasicMeshObject::CreateMesh()
{
	BOOL bResult = FALSE;
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
	CD3D12ResourceManager*	pResourceManager = m_pRenderer->INL_GetResourceManager();

	// Create the vertex buffer.
	// Define the geometry for a triangle.
	BasicVertex Vertices[] =
	{
		{ { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
		{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
	};

	const UINT VertexBufferSize = sizeof(Vertices);

	if (FAILED(pResourceManager->CreateVertexBuffer(sizeof(BasicVertex), (DWORD)_countof(Vertices), &m_VertexBufferView, &m_pVertexBuffer, Vertices)))
	{
		__debugbreak();
		goto lb_return;
	}
	// Create Texture
	const UINT TexWidth = 16;
	const UINT TexHeight = 16;;
	DXGI_FORMAT TexFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	BYTE* pImage = (BYTE*)malloc(TexWidth * TexHeight * 4);
	memset(pImage, 0, TexWidth * TexHeight * 4);

	BOOL bFirstColorIsWhite = TRUE;
	
	for (UINT y = 0; y < TexHeight; y++)
	{
		for (UINT x = 0; x < TexWidth; x++)
		{
			
			RGBA* pDest = (RGBA*)(pImage + (x + y * TexWidth) * 4);

			pDest->r = rand() % 255;
			pDest->g = rand() % 255;
			pDest->b = rand() % 255;

			if ((bFirstColorIsWhite + x) % 2)
			{
				pDest->r = 255;
				pDest->g = 255;
				pDest->b = 255;
			}
			else
			{
				pDest->r = 0;
				pDest->g = 0;
				pDest->b = 0;
			}
			pDest->a = 255;
		}
		bFirstColorIsWhite++;
		bFirstColorIsWhite %= 2;
	}
	pResourceManager->CreateTexture(&m_pTexResource, TexWidth, TexHeight, TexFormat, pImage);

	free(pImage);

	CreateDescriptorTable();

	// Create ShaderResource View from TextureResource
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = TexFormat;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		CD3DX12_CPU_DESCRIPTOR_HANDLE srv(m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart(), BASIC_MESH_DESCRIPTOR_INDEX_TEX, m_srvDescriptorSize);
		pD3DDeivce->CreateShaderResourceView(m_pTexResource, &SRVDesc, srv);
	}

	// dispatch할때 사용할 common heap에 카피
	//CD3DX12_CPU_DESCRIPTOR_HANDLE	commonHeapDest(m_pCommonHeap->GetCPUDescriptorHandleForHeapStart(), COMMON_DESCRIPTOR_INDEX_SRV_COLORMAP, m_srvDescriptorSize);
	//pDevice->CopyDescriptorsSimple(1, commonHeapDest, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	bResult = TRUE;

lb_return:
	return bResult;
}
void CBasicMeshObject::Draw(ID3D12GraphicsCommandList* pCommandList)
{

	/*
	* // 프레임 시작: 무거운 보관함 장착은 "딱 한 번만" 합니다.
		pCommandList->SetDescriptorHeaps(1, &m_pGlobalDescriptorHeap);

		// -------------------------------------------------------------
		// 캐릭터 1 그리기 (가벼운 주소 전달만)
		CD3DX12_GPU_DESCRIPTOR_HANDLE char1Handle = ...; // 힙의 0번 칸 (캐릭터1 텍스처)
		pCommandList->SetGraphicsRootDescriptorTable(0, char1Handle); 
		pCommandList->DrawInstanced(...);

		// 캐릭터 2 그리기 (보관함은 그대로 두고, 가볍게 주소만 슥 바꿈)
		CD3DX12_GPU_DESCRIPTOR_HANDLE char2Handle = ...; // 힙의 50번 칸 (캐릭터2 텍스처)
		pCommandList->SetGraphicsRootDescriptorTable(0, char2Handle); 
		pCommandList->DrawInstanced(...);

		// 배경 그리기 (보관함은 그대로 두고, 또 주소만 바꿈)
		CD3DX12_GPU_DESCRIPTOR_HANDLE bgHandle = ...;    // 힙의 120번 칸 (배경 텍스처)
		pCommandList->SetGraphicsRootDescriptorTable(0, bgHandle); 
		pCommandList->DrawInstanced(...);
	*/
	// set RootSignature
	pCommandList->SetGraphicsRootSignature(m_pRootSignature);

	pCommandList->SetDescriptorHeaps(1, &m_pDescritorHeap);

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable(m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart());
	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->SetPipelineState(m_pPipelineState);
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	pCommandList->DrawInstanced(3, 1, 0, 0);
	

}

void CBasicMeshObject::Cleanup()
{
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	if (m_pDescritorHeap)
	{
		m_pDescritorHeap->Release();
		m_pDescritorHeap = nullptr;
	}
	if (m_pTexResource)
	{
		m_pTexResource->Release();
		m_pTexResource = nullptr;
	}
	CleanupSharedResources();
}
void CBasicMeshObject::CleanupSharedResources()
{
	if (!m_dwInitRefCount)
		return;

	DWORD ref_count = --m_dwInitRefCount;
	if (!ref_count)
	{
		if (m_pRootSignature)
		{
			m_pRootSignature->Release();
			m_pRootSignature = nullptr;
		}
		if (m_pPipelineState)
		{
			m_pPipelineState->Release();
			m_pPipelineState = nullptr;
		}
	}
}
CBasicMeshObject::~CBasicMeshObject()
{
	Cleanup();
}