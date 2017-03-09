// license:BSD-3-Clause
// copyright-holders:Ian Wu
/***************************************************************************

    vr.cpp

    handle vr render and input/output.

***************************************************************************/

#include "emu.h"
#include "render.h"
#include "emuopts.h"

#include "vr.h"
#include "vrdevice/modelfile.h"
#include "vrdevice/pnchmn.h"

static std::map<std::string, std::function<vr_device_interface*()> > s_FactoryMap;

template<typename T>
T* newFunction(){ return new T(); }
#define DEVICE_FACTORY(name) \
	s_FactoryMap[#name] = std::bind(&newFunction<vr_device_##name>);

#pragma region vr_option
//**************************************************************************
//  vr_option
//**************************************************************************
vr_option::vr_option()
{
}

vr_option::~vr_option()
{
	m_Sections.clear();
}

bool vr_option::parseIni(std::string a_Filename)
{
	FILE *fp = fopen(a_Filename.c_str(), "rt");
	if( nullptr == fp ) return false;

	bool l_Result = true;

	// loop over lines in the file
	char l_Buff[4096] = {0};
	std::string l_CurrSecName("Global");
	param_collect *l_pTargetCollection = &(m_Sections[l_CurrSecName] = param_collect());
	while( nullptr != fgets(l_Buff, ARRAY_LENGTH(l_Buff), fp) )
	{
		std::string l_Line(l_Buff);
		// will convert "XXXX OOOO" to "XXXXOOOO", maybe not a good solution....
		std::remove_if(l_Line.begin(), l_Line.end(), [](char a_Char){ return !isgraph((int)a_Char); });
		if( iscntrl((int)l_Line.back()) ) l_Line.pop_back();

		size_t l_TempPos = l_Line.find_first_of('#');
		if( std::string::npos != l_TempPos )
		{
			if( 0 == l_TempPos ) continue;
			l_Line.substr(0, l_TempPos);
		}

		// found section
		if( '[' == l_Line.front() )
		{
			size_t l_SectionEnd = l_Line.find_last_of(']');
			if( l_SectionEnd != l_Line.find_first_of(']') ||
			   l_Line.find_last_of('[') != 0 )
			{
				l_Result = false;
				break;
			}
			l_CurrSecName = l_Line.substr(1, l_SectionEnd - 1);
			l_pTargetCollection = &(m_Sections[l_CurrSecName] = param_collect());
			continue;
		}
		
		size_t l_EqualCharPos = l_Line.find_first_of('=');
		if( std::string::npos == l_EqualCharPos )
		{
			l_Result = false;
			break;
		}

		std::string l_Name(l_Line.substr(0, l_EqualCharPos));
		std::string l_Value(l_Line.substr(l_EqualCharPos + 1));
		l_pTargetCollection->insert(make_pair(l_Name, l_Value));
	}

	fclose(fp);
	return l_Result;
}

vr_option::param_collect& vr_option::getParams(std::string a_SectionName)
{
	auto l_SecIt = m_Sections.find(a_SectionName);
	assert( m_Sections.end() != l_SecIt );
	return l_SecIt->second;
}

int vr_option::getParamValue(std::string a_SectionName, std::string a_ParamName, int a_Default)
{
	auto l_Func = [](std::string a_Value){ return atoi(a_Value.c_str()); };
	return getParamValue<int>(a_SectionName, a_ParamName, a_Default, l_Func);
}

float vr_option::getParamValue(std::string a_SectionName, std::string a_ParamName, float a_Default)
{
	auto l_Func = [](std::string a_Value){ return atof(a_Value.c_str()); };
	return getParamValue<float>(a_SectionName, a_ParamName, a_Default, l_Func);
}

std::string vr_option::getParamValue(std::string a_SectionName, std::string a_ParamName, std::string a_Default)
{
	auto l_Func = [](std::string a_Value){ return a_Value; };
	return getParamValue<std::string>(a_SectionName, a_ParamName, a_Default, l_Func);
}
#pragma endregion

#pragma region vr_machine
//**************************************************************************
//  VR Manager
//**************************************************************************
#pragma region vr_machine::model_instance
vr_machine::model_instance::model_instance()
	: m_RefNodeName()
	, m_pRefWorld(nullptr)
{
}

vr_machine::model_instance::~model_instance()
{
}
#pragma endregion

#pragma region vr_machine::machine_model
vr_machine::machine_model::machine_model()
	: m_FxIndex(0)
	, m_pVtxBuffer(nullptr)
	, m_pIndexuffer(nullptr)
	, m_model_index(0)
{
}

vr_machine::machine_model::~machine_model()
{
}
#pragma endregion

#pragma region vr_machine::machine_fx_item
vr_machine::machine_fx_item::machine_fx_item()
	: m_UnitformName("")
	, m_UniformType(0)
	, m_Size(0)
	, m_pRefTarget(nullptr)
{
}

vr_machine::machine_fx_item::~machine_fx_item()
{
}
#pragma endregion

#pragma region vr_machine::machine_fx
vr_machine::machine_fx::machine_fx()
	: m_FxName("")
{
}

vr_machine::machine_fx::~machine_fx()
{
}
#pragma endregion

#pragma region vr_machine::machine_node
vr_machine::machine_node::machine_node()
	: m_Name("")
	, m_pRefParent(nullptr)
	, m_Origin(), m_Tranform()
{
}

vr_machine::machine_node::~machine_node()
{
	for( unsigned int i=0 ; i<m_Children.size() ; ++i ) delete m_Children[i];
}

void vr_machine::machine_node::update(glm::mat4x4 &a_World)
{
	m_Tranform = m_Origin * a_World;
	update();
}

void vr_machine::machine_node::update()
{
	for( unsigned int i=0 ; i<m_Children.size() ; ++i ) m_Children[i]->update(m_Tranform);
}

vr_machine::machine_node* vr_machine::machine_node::find(std::string a_Name)
{
	if( m_Name == a_Name ) return this;
	for( unsigned int i=0 ; i<m_Children.size() ; ++i )
	{
		vr_machine::machine_node *l_pRef = m_Children[i]->find(a_Name);
		if( nullptr != l_pRef ) return l_pRef;
	}
	return nullptr;
}
#pragma endregion

//-------------------------------------------------
//  vr_machine - constructor
//-------------------------------------------------
vr_machine::vr_machine()
	: m_EyeTextureSize(1080, 1200)
	, m_pHMD(nullptr)
	, m_pRenderModel(nullptr)
	, m_pInterface(nullptr)
	, m_pRefMachine(nullptr)
	, m_pModelData(nullptr)
	, m_pRoot(nullptr)
	, m_Scaler(1.0f)
{
	DEVICE_FACTORY(pnchmn)

	memset(m_InputMap, 0, sizeof(m_InputMap));
}

vr_machine::~vr_machine()
{
}

void vr_machine::init(running_machine *a_pCurrMachine)
{
	m_pRefMachine = a_pCurrMachine;
	std::string l_VRMachineName(a_pCurrMachine->config().m_vr_machine_name);
	if( l_VRMachineName.empty() ) return;

	m_DirPath = a_pCurrMachine->manager().options().vr_path() + std::string("/") + l_VRMachineName;

	vr_option l_MachineSetting;
	if( !l_MachineSetting.parseIni(m_DirPath + "/machine_setting.ini") ) return;

	vr::EVRInitError l_Error = vr::VRInitError_None;
	auto l_pHmd = vr::VR_Init( &l_Error, vr::VRApplication_Scene );

	if ( l_Error != vr::VRInitError_None ) return;

	m_pRenderModel = (vr::IVRRenderModels *)vr::VR_GetGenericInterface( vr::IVRRenderModels_Version, &l_Error );
	if( nullptr == m_pRenderModel ||
		!vr::VRCompositor() )
	{
		m_pHMD = nullptr;
		vr::VR_Shutdown();
		return;
	}

	m_pInterface = s_FactoryMap[l_VRMachineName]();
	std::string l_ModelName(l_MachineSetting.getParamValue("machine", "model", ""));
	std::transform(l_ModelName.begin(), l_ModelName.end(), l_ModelName.begin(), ::tolower);
	m_pModelData = new model_file(m_DirPath + "/" + l_ModelName);

	m_Scaler = l_MachineSetting.getParamValue("machine", "scaler", 1.0f);

	m_pInterface->init(l_MachineSetting, m_machine_model, m_pRoot, m_machine_fx);
	m_pHMD = l_pHmd;
	m_pHMD->GetRecommendedRenderTargetSize(&m_EyeTextureSize.x, &m_EyeTextureSize.y);

	initHMDProjection(vr::Eye_Left);
	initHMDProjection(vr::Eye_Right);
	initEyeToHeadMatrix(vr::Eye_Left);
	initEyeToHeadMatrix(vr::Eye_Right);
}

void vr_machine::uninit()
{
	if( nullptr != m_pRoot ) delete m_pRoot;
	m_pRoot = nullptr;
	if( nullptr != m_pModelData ) delete m_pModelData;
	m_pModelData = nullptr;
	if( nullptr != m_pInterface ) delete m_pInterface;
	m_pInterface = nullptr;

	if( nullptr == m_pHMD ) return;
	m_pHMD->ReleaseInputFocus();

	for( unsigned int i=0 ; i<m_machine_model.size() ; ++i )
	{
		for( unsigned int j=0 ; j<m_machine_model[i]->m_pRefWorld.size() ; ++j ) delete m_machine_model[i]->m_pRefWorld[j];
		m_machine_model[i]->m_pRefWorld.clear();
		delete m_machine_model[i];
	}
	m_machine_model.clear();

	for( unsigned int i=0 ; i<m_machine_fx.size() ; ++i )
	{
		auto &l_UniformVec = m_machine_fx[i]->m_UniformMap;
		delete m_machine_fx[i];
	}
	m_machine_fx.clear();

	m_pHMD = nullptr;
	vr::VR_Shutdown();

	memset(m_InputMap, 0, sizeof(m_InputMap));
}

void vr_machine::update(const int a_Time)
{
	if( nullptr == m_pHMD || nullptr == m_pInterface ) return;

	glm::mat4x4 l_Identity;
	m_pRoot->update(l_Identity);
	m_pInterface->update(a_Time);
}

void vr_machine::commit(void *a_pLeftEyeTexture, void *a_pRightEyeTexture, vr::ETextureType a_Api)
{
	vr::Texture_t l_LeftEyeTexture = {(void*)a_pLeftEyeTexture, a_Api, vr::ColorSpace_Gamma};
	vr::EVRCompositorError l_ErrorCode = vr::VRCompositor()->Submit(vr::Eye_Left, &l_LeftEyeTexture);
	vr::Texture_t l_RightEyeTexture = {(void*)a_pRightEyeTexture, a_Api, vr::ColorSpace_Gamma};
	l_ErrorCode = vr::VRCompositor()->Submit(vr::Eye_Right, &l_RightEyeTexture);
}

void vr_machine::updateFx(unsigned int a_ModelIdx, unsigned int a_InstIdx)
{
	machine_model *l_pModel = m_machine_model[a_ModelIdx];
	model_instance *l_pInstance = l_pModel->m_pRefWorld[a_InstIdx];
	machine_fx *l_pFx = m_machine_fx[l_pModel->m_FxIndex];
	for( unsigned int i=0 ; i<l_pFx->m_UniformMap.size() ; ++i )
	{
		memcpy(l_pFx->m_UniformMap[i].m_pRefTarget, l_pInstance->m_Uniform[i], l_pFx->m_UniformMap[i].m_Size);
	}
}

void vr_machine::handleInput()
{
	if( nullptr == m_pInterface ) return;

	vr::VRCompositor()->WaitGetPoses(m_DevicePos, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

	if( m_DevicePos[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
	{
		m_pInterface->handleHmdPosition(m_DevicePos[vr::k_unTrackedDeviceIndex_Hmd]);
	}
	for( unsigned int i=1 ; i<vr::k_unMaxTrackedDeviceCount ; ++i )
	{
		if ( !m_DevicePos[i].bPoseIsValid ) continue;

		vr::ETrackedDeviceClass l_Role = m_pHMD->GetTrackedDeviceClass(i);
		m_pInterface->handlePosition(i, m_DevicePos[i], l_Role);
	}

	auto l_ViewMat4x3 = m_DevicePos[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
	glm::mat4x4 l_View = glm::mat4x4(l_ViewMat4x3.m[0][0], l_ViewMat4x3.m[0][1], l_ViewMat4x3.m[0][2], l_ViewMat4x3.m[0][3] * m_Scaler
									,l_ViewMat4x3.m[1][0], l_ViewMat4x3.m[1][1], l_ViewMat4x3.m[1][2], l_ViewMat4x3.m[1][3] * m_Scaler
									,l_ViewMat4x3.m[2][0], l_ViewMat4x3.m[2][1], l_ViewMat4x3.m[2][2], l_ViewMat4x3.m[2][3] * m_Scaler
									,0.0f, 0.0f, 0.0f, 1.0f);
	l_View = glm::inverse(l_View);

	m_LeftVP = l_View * m_HeadToEye[vr::Eye_Left] * m_EyeProjection[vr::Eye_Left];
	m_RightVP = l_View * m_HeadToEye[vr::Eye_Right] * m_EyeProjection[vr::Eye_Right];

	vr::VREvent_t l_Event;
	while( m_pHMD->PollNextEvent( &l_Event, sizeof(vr::VREvent_t) ) )
	{
		switch( l_Event.eventType )
		{
			case vr::VREvent_ButtonPress:
			case vr::VREvent_ButtonUnpress:{
				bool l_bPress = l_Event.eventType == vr::VREvent_ButtonPress;
				switch( l_Event.data.controller.button )
				{
					case vr::k_EButton_SteamVR_Trigger: m_InputMap[VRInputDefine::TRIGGER]	= l_bPress;	break;
					case vr::k_EButton_ApplicationMenu:	m_InputMap[VRInputDefine::MENU]		= l_bPress;	break;
					case vr::k_EButton_Grip:			m_InputMap[VRInputDefine::GRIP]		= l_bPress;	break;
					default:break;
				}
				}break;

			default:break;
		}
		m_pInterface->handleInput(l_Event);
	}
}

void vr_machine::sendMessage(int a_ArgCount, va_list a_ArgList)
{
	if( nullptr != m_pInterface ) m_pInterface->sendMessage(a_ArgCount, a_ArgList);
}

void* vr_machine::getHandleState(int a_Handle)
{
	if( nullptr == m_pInterface ) return nullptr;
	return m_pInterface->getHandleState(a_Handle);
}

void vr_machine::sendInput(VRInputDefine a_Btn, bool a_bPressed)
{
	m_InputMap[a_Btn] = a_bPressed;
}

bool vr_machine::getInputPressed(VRInputDefine a_Btn)
{
	return m_InputMap[a_Btn];
}

void vr_machine::doVibrate(unsigned int a_DeviceIdx, unsigned short a_MicroSec)
{
	m_pHMD->TriggerHapticPulse(a_DeviceIdx, vr::k_eControllerAxis_None, a_MicroSec);
}

glm::vec2 vr_machine::getTouchPadPos(int a_DeviceIdx)
{
	glm::vec2 l_Res(0.0f, 0.0f);
	vr::VRControllerState_t l_State;
	if( m_pHMD->GetControllerState(a_DeviceIdx, &l_State, sizeof(l_State)) )
	{
		return glm::vec2(l_State.rAxis[0].x, l_State.rAxis[0].y);
	}
	return glm::vec2(0.0f, 0.0f);
}

void vr_machine::initHMDProjection(vr::Hmd_Eye a_Eye)
{
	auto l_Proj = m_pHMD->GetProjectionMatrix(a_Eye, 0.1f, 10000.0f);
	m_EyeProjection[(int)a_Eye] =
		glm::mat4x4(l_Proj.m[0][0], l_Proj.m[0][1], l_Proj.m[0][2], l_Proj.m[0][3],
					l_Proj.m[1][0], l_Proj.m[1][1], l_Proj.m[1][2], l_Proj.m[1][3], 
					l_Proj.m[2][0], l_Proj.m[2][1], l_Proj.m[2][2], l_Proj.m[2][3], 
					l_Proj.m[3][0], l_Proj.m[3][1], l_Proj.m[3][2], l_Proj.m[3][3]);
}

void vr_machine::initEyeToHeadMatrix(vr::Hmd_Eye a_Eye)
{
	vr::HmdMatrix34_t l_EyeTrans = m_pHMD->GetEyeToHeadTransform(a_Eye);
	m_HeadToEye[(int)a_Eye] =
		glm::inverse(glm::mat4x4(
			l_EyeTrans.m[0][0], l_EyeTrans.m[0][1], l_EyeTrans.m[0][2], l_EyeTrans.m[0][3], 
			l_EyeTrans.m[1][0], l_EyeTrans.m[1][1], l_EyeTrans.m[1][2], l_EyeTrans.m[1][3],
			l_EyeTrans.m[2][0], l_EyeTrans.m[2][1], l_EyeTrans.m[2][2], l_EyeTrans.m[2][3],
			0.0f, 0.0f, 0.0f, 1.0f));
}
#pragma endregion

#pragma region vr_device_interface
//
// vr_device_interface
//
vr_device_interface::vr_device_interface()
	: m_pPxBase(nullptr)
	, m_pProfileZoneMgr(nullptr)
	, m_pPhysics(nullptr)
	, m_pCpuDispatcher(nullptr)
	, m_pPxScene(nullptr)
	, m_pPxDebugger(nullptr)
{
}

vr_device_interface::~vr_device_interface()
{
	PHYSX_SAFE_RELEASE(m_pPxDebugger);
	PHYSX_SAFE_RELEASE(m_pPxScene);
	PHYSX_SAFE_RELEASE(m_pCpuDispatcher);

	PxCloseExtensions();
	PHYSX_SAFE_RELEASE(m_pPhysics);
	PHYSX_SAFE_RELEASE(m_pProfileZoneMgr);
	PHYSX_SAFE_RELEASE(m_pPxBase);
}

void vr_device_interface::init(vr_option &a_Config, std::vector<vr_machine::machine_model *> &a_Container, vr_machine::machine_node* &a_pRoot, std::vector<vr_machine::machine_fx *> &a_Fx)
{
	initPhysX();
	initMachineNode(a_pRoot);
	initMachine(a_Config, a_Container, a_pRoot, a_Fx);
}

void vr_device_interface::initPhysX()
{
	m_pPxBase = PxCreateFoundation(PX_PHYSICS_VERSION, m_PxAllocator, m_PxErrorHandle);
	assert(nullptr != m_pPxBase);

	//m_pProfileZoneMgr = &physx::PxProfileZoneManager::createProfileZoneManager(m_pPxBase);
	//assert(nullptr != m_pProfileZoneMgr);

	physx::PxTolerancesScale l_Toloerance;
	initPhysXTolerance(l_Toloerance);
	
	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pPxBase, l_Toloerance, true, nullptr);//m_pProfileZoneMgr);
	assert(nullptr != m_pPhysics);

	if( !PxInitExtensions(*m_pPhysics) ) assert(false && "physx extention initi failed");

	physx::PxSceneDesc l_SceneDesc(l_Toloerance);
	l_SceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	//l_SceneDesc.simulationOrder = physx::PxSimulationOrder::eSOLVE_COLLIDE;
	l_SceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVETRANSFORMS | physx::PxSceneFlag::eREQUIRE_RW_LOCK;
	initPhysXScene(l_SceneDesc);
	if( nullptr == l_SceneDesc.cpuDispatcher )
	{
		l_SceneDesc.cpuDispatcher = m_pCpuDispatcher = physx::PxDefaultCpuDispatcherCreate(1);
		assert(nullptr != l_SceneDesc.cpuDispatcher);
	}

	if( nullptr == l_SceneDesc.filterShader ) l_SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
	m_pPxScene = m_pPhysics->createScene(l_SceneDesc);
	assert(nullptr != m_pPxScene);

#ifdef MAME_DEBUG
	if( nullptr != m_pPhysics->getPvdConnectionManager() )
	{
		m_pPxScene->lockWrite();
		m_pPxScene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 1.0f);
		m_pPxScene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LIMITS, 1.0f);
		m_pPxScene->unlockWrite();

		const char *c_pLocalIP = "127.0.0.1";
		int l_Port = 5425;
		unsigned int l_Timeout = 100;
		physx::PxVisualDebuggerConnectionFlags l_ConnectionFlags = physx::PxVisualDebuggerExt::getAllConnectionFlags();
		m_pPxDebugger = physx::PxVisualDebuggerExt::createConnection(m_pPhysics->getPvdConnectionManager(), c_pLocalIP, l_Port, l_Timeout, l_ConnectionFlags);
	}
#endif
}

void vr_device_interface::initMachineNode(vr_machine::machine_node* &a_pRoot)
{
	const model_file *l_pSrcFile = vr_machine::singleton().getRefModelFile();
	auto l_NodeList = l_pSrcFile->m_ModelNodes;

	std::vector<vr_machine::machine_node *> l_NodeVec(l_NodeList.size(), nullptr);
	for( unsigned int i=0 ; i<l_NodeList.size() ; ++i )
	{
		vr_machine::machine_node *l_pNewNode = new vr_machine::machine_node();
		l_NodeVec[i] = l_pNewNode;
		l_pNewNode->m_Name = l_NodeList[i]->m_NodeName;
		l_pNewNode->m_Origin = l_NodeList[i]->m_Transform;
		l_pNewNode->m_Tranform = glm::mat4x4(1.0f);
	}

	for( unsigned int i=0 ; i<l_NodeList.size() ; ++i )
	{
		model_file::model_node *l_pSrcNode = l_NodeList[i];
		vr_machine::machine_node *l_pDstNode = l_NodeVec[i];
		if( l_pSrcNode->m_Children.empty() ) continue;

		l_pDstNode->m_Children.resize(l_pSrcNode->m_Children.size(), nullptr);
		for( unsigned int j=0 ; j<l_pSrcNode->m_Children.size() ; ++j )
		{
			l_pDstNode->m_Children[j] = l_NodeVec[l_pSrcNode->m_Children[j]];
			l_pDstNode->m_Children[j]->m_pRefParent = l_pDstNode;
		}
	}

	a_pRoot = l_NodeVec.front();
	/*a_pRoot = new vr_machine::machine_node();
	a_pRoot->m_Name = "Root";
	a_pRoot->m_Origin = a_pRoot->m_Tranform = glm::mat4x4(1.0f);
	a_pRoot->m_pRefParent = nullptr;
	for( unsigned int i=0 ; i<l_NodeVec.size() ; ++i )
	{
		if( nullptr != l_NodeVec[i]->m_pRefParent ) continue;

		l_NodeVec[i]->m_pRefParent = a_pRoot;
		a_pRoot->m_Children.push_back(l_NodeVec[i]);
	}*/

	glm::mat4x4 l_Identity;
	a_pRoot->update(l_Identity);
}

glm::mat4x4 vr_device_interface::getMatrix(vr::TrackedDevicePose_t &a_TrackedDevice)
{
	auto l_Matrix = a_TrackedDevice.mDeviceToAbsoluteTracking;
	float l_Scaler = vr_machine::singleton().getScaler();
	return glm::mat4x4(l_Matrix.m[0][0], l_Matrix.m[0][1], l_Matrix.m[0][2], l_Matrix.m[0][3] * l_Scaler
					 , l_Matrix.m[1][0], l_Matrix.m[1][1], l_Matrix.m[1][2], l_Matrix.m[1][3] * l_Scaler
					 , l_Matrix.m[2][0], l_Matrix.m[2][1], l_Matrix.m[2][2], l_Matrix.m[2][3] * l_Scaler
					 , 0.0f, 0.0f, 0.0f, 1.0f);
}
#pragma endregion