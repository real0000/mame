#include "emu.h"

#include <set>

#include "modelfile.h"
#include "pnchmn.h"
#include "pnchmn_event.def"
#include "glm/gtx/vector_angle.hpp"

#define PAD_IDENTIFY "Pad"
#define PUNCH_IDENTIFY "Punch"
#define PAD_IO_MIN 0
#define PAD_IO_MAX 5

//#include "modules/render/d3d/d3dhlsl.h"
// copy from modules/render/d3d/d3dhlsl.h
typedef enum
{
    UT_VEC4,
    UT_VEC3,
    UT_VEC2,
    UT_FLOAT,
    UT_INT,
    UT_BOOL,
    UT_MATRIX,
    UT_SAMPLER
} uniform_type;

vr_device_pnchmn::vr_device_pnchmn()
    : vr_device_interface()
    , m_pRefMachine(nullptr)
    , m_pDefMaterial(nullptr)
    , m_bAutoFireFlag(false), m_bAutoFirState(false)
    , m_PadDeadZone(glm::pi<float>() / 4.0f), m_PunchFix(0.0f)
    , m_PunchWeight(200.0f), m_DriverSpeed(8.0f)
{
    memset(m_PunchNode, 0, sizeof(m_PunchNode));
    memset(m_Pads, 0, sizeof(m_Pads));
}

vr_device_pnchmn::~vr_device_pnchmn()
{
    getPhysxScene()->lockWrite();
    getPhysxScene()->setSimulationEventCallback(nullptr);
    getPhysxScene()->fetchResults(true);
    for( unsigned int i=0 ; i<PNCHMN_NUMPAD ; ++i )
    {
        PHYSX_SAFE_RELEASE(m_Pads[i].m_Joints[0]);
        PHYSX_SAFE_RELEASE(m_Pads[i].m_Joints[1]);
        PHYSX_SAFE_RELEASE(m_Pads[i].m_pBone);
    }
    for( unsigned int i=0 ; i<PNCHMN_NUMPUNCH ; ++i )
    {
        PHYSX_SAFE_RELEASE(m_PunchNode[i].m_pActor);
    }
    PHYSX_SAFE_RELEASE(m_pDefMaterial);

    getPhysxScene()->unlockWrite();
}

void vr_device_pnchmn::initMachine(vr_option &a_Config, std::vector<vr_machine::machine_model *> &a_Container, vr_machine::machine_node* a_pRoot, std::vector<vr_machine::machine_fx *> &a_Fx)
{
    m_PunchWeight = a_Config.getParamValue("machine", "punch_weight", 200.0f);
    m_DriverSpeed = a_Config.getParamValue("machine", "motor_drive", 8.0f);
    m_PadDeadZone = glm::radians(a_Config.getParamValue("machine", "pad_dead_zone", 45.0f));
    m_PunchFix = a_Config.getParamValue("machine", "punch_pos_fix", 0.0f);
    
    std::map<std::string, int> l_FxMap;
    {// init all fx
        const char *c_pFxNames[] = {"ambient", "noLight", "pad"};
        const unsigned int c_NumFx = sizeof(c_pFxNames) / sizeof(const char *);

        for( unsigned int i=0 ; i<c_NumFx ; ++i )
        {
            vr_machine::machine_fx *l_pNewFx = new vr_machine::machine_fx();
            l_pNewFx->m_FxName = c_pFxNames[i];
            l_FxMap[l_pNewFx->m_FxName] = a_Fx.size();
            a_Fx.push_back(l_pNewFx);
        
            vr_machine::machine_fx_item l_World;
            l_World.m_UnitformName = "u_World";
            l_World.m_Size = sizeof(glm::mat4x4);
            l_World.m_UniformType = UT_MATRIX;
            l_pNewFx->m_UniformMap.push_back(l_World);

            vr_machine::machine_fx_item l_ViewProjection;
            l_ViewProjection.m_UnitformName = "u_VP";
            l_ViewProjection.m_Size = sizeof(glm::mat4x4);
            l_ViewProjection.m_UniformType = UT_MATRIX;
            l_pNewFx->m_UniformMap.push_back(l_ViewProjection);
        }

        vr_machine::machine_fx *l_pLightFx = a_Fx[l_FxMap["pad"]];
        {
            vr_machine::machine_fx_item l_LightSwitch;
            l_LightSwitch.m_UnitformName = "u_LightOn";
            l_LightSwitch.m_Size = sizeof(int);
            l_LightSwitch.m_UniformType = UT_INT;
            l_pLightFx->m_UniformMap.push_back(l_LightSwitch);
        }
    }

    m_pRefMachine = a_pRoot->find("Frame");

    const std::map<std::string, std::string> c_DrawTarget = {
        std::make_pair("Frame", "ambient"),
        std::make_pair("PunchLeft", "ambient"),
        std::make_pair("PunchRight", "ambient"),
        std::make_pair("GameScreen", "noLight"),
        std::make_pair("PadLeftTop", "pad"),
        std::make_pair("PadLeftMiddle", "pad"),
        std::make_pair("PadLeftBottom", "pad"),
        std::make_pair("PadRightBottom", "pad"),
        std::make_pair("PadRightMiddle", "pad"),
        std::make_pair("PadRightTop", "pad")};
    const std::map<std::string, unsigned int> c_PadIdxMap = {
        std::make_pair("PadLeftTop", 1),
        std::make_pair("PadLeftMiddle", 2),
        std::make_pair("PadLeftBottom", 3),
        std::make_pair("PadRightBottom", 6),
        std::make_pair("PadRightMiddle", 5),
        std::make_pair("PadRightTop", 4)};
    const model_file *l_pSrcFile = vr_machine::singleton().getRefModelFile();
    const std::vector<model_file::model_meshes *> &l_SrcMeshList = l_pSrcFile->m_Meshes;
    for( unsigned int i=0 ; i<l_SrcMeshList.size() ; ++i )
    {
        const model_file::model_meshes *l_pSrcMesh = l_SrcMeshList[i];
        vr_machine::machine_model *l_pNewModel = nullptr;
        for( unsigned int j=0 ; j<l_pSrcMesh->m_RefNode.size() ; ++j ) 
        {
            model_file::model_node *l_pRefNodeInst = l_pSrcFile->m_ModelNodes[l_pSrcMesh->m_RefNode[j]];
            std::string l_Nodename(l_pRefNodeInst->m_NodeName);

            auto it = c_DrawTarget.find(l_Nodename);
            if( c_DrawTarget.end() == it ) continue;

            if( nullptr == l_pNewModel ) 
            {
                l_pNewModel = new vr_machine::machine_model();
                l_pNewModel->m_model_index = i;
                l_pNewModel->m_FxIndex = l_FxMap[it->second];
                a_Container.push_back(l_pNewModel);
            }

            vr_machine::model_instance *l_pNewInstance = new vr_machine::model_instance();
            l_pNewInstance->m_RefNodeName = l_pRefNodeInst->m_NodeName;
            auto l_TargetNode = a_pRoot->find(l_Nodename);
            assert(nullptr != l_TargetNode);
            l_pNewInstance->m_pRefWorld = &(l_TargetNode->m_Tranform);
            l_pNewInstance->m_Uniform.push_back(l_pNewInstance->m_pRefWorld);
            l_pNewInstance->m_Uniform.push_back(&vr_machine::singleton().getViewProject());
            l_pNewModel->m_pRefWorld.push_back(l_pNewInstance);
            
            // check light param needed
            if( it->second == "pad" )
            {
                unsigned int l_Idx = c_PadIdxMap.find(it->first)->second - 1;
                l_pNewInstance->m_Uniform.push_back(&(m_Pads[l_Idx].m_LightOn));
            }
        }
    }

    physx::PxScene *l_pRefPxScene = getPhysxScene();
    l_pRefPxScene->lockWrite();

    const char *c_PadTarget[PNCHMN_NUMPAD] = {
        "PadLeftTop",
        "PadLeftMiddle",
        "PadLeftBottom",
        "PadRightTop",
        "PadRightMiddle",
        "PadRightBottom"};
    auto l_pModelFile = vr_machine::singleton().getRefModelFile();
    physx::PxPhysics *l_pRefPxInst = getPhysXInst();
    m_pDefMaterial = l_pRefPxInst->createMaterial(0.5f, 0.5f, 1.0f);
    physx::PxQuat l_LeftRot, l_RightRot;
    {
        l_LeftRot = physx::PxQuat(glm::radians(90.0f), physx::PxVec3(0.0f, 0.0f, 1.0f));

        glm::quat l_TempQuat;
        l_TempQuat = glm::rotate(l_TempQuat, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        l_TempQuat = glm::rotate(l_TempQuat, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        l_RightRot = physx::PxQuat(l_TempQuat.x, l_TempQuat.y, l_TempQuat.z, l_TempQuat.w);
    }
    for( unsigned int i=0 ; i<PNCHMN_NUMPAD ; ++i )
    {
        m_Pads[i].m_pRefPadNode = a_pRoot->find(c_PadTarget[i]);
        unsigned int l_RefMeshIdx = l_pModelFile->find(c_PadTarget[i])->m_RefMesh.front();
        auto l_pRefMesh = l_pModelFile->m_Meshes[l_RefMeshIdx];

        physx::PxTransform l_Matrix;
        vr_machine::machine_node *l_pArmNode = m_Pads[i].m_pRefPadNode->m_pRefParent;
        l_Matrix.p = physx::PxVec3(l_pArmNode->m_Tranform[0][3], l_pArmNode->m_Tranform[1][3], l_pArmNode->m_Tranform[2][3]);
        l_Matrix.q = physx::PxQuat(0.0f, 0.0f, 0.0f, 1.0f);
        physx::PxRigidStatic *l_pNewActor = l_pRefPxInst->createRigidStatic(l_Matrix);

        physx::PxSphereGeometry l_EmptyBall;
        l_EmptyBall.radius = 1.0f;
        physx::PxShape *l_pBall = l_pRefPxInst->createShape(l_EmptyBall, *m_pDefMaterial, false, physx::PxShapeFlag::eVISUALIZATION);
        l_pNewActor->attachShape(*l_pBall);
        
        l_pNewActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
        l_pRefPxScene->addActor(*l_pNewActor);
        m_Pads[i].m_Joints[0] = l_pNewActor;

        physx::PxTransform l_PadMatrix;
        l_PadMatrix.p = physx::PxVec3(m_Pads[i].m_pRefPadNode->m_Tranform[0][3], m_Pads[i].m_pRefPadNode->m_Tranform[1][3], m_Pads[i].m_pRefPadNode->m_Tranform[2][3]);
        l_PadMatrix.q = physx::PxQuat(0.0f, 0.0f, 0.0f, 1.0f);
        physx::PxRigidDynamic *l_pJointActor = l_pRefPxInst->createRigidDynamic(l_PadMatrix);

        physx::PxBoxGeometry l_BoxData = {};
        l_BoxData.halfExtents = physx::PxVec3(l_pRefMesh->m_BoxSize.x, l_pRefMesh->m_BoxSize.y, l_pRefMesh->m_BoxSize.z);
        physx::PxShape *l_Box = l_pRefPxInst->createShape(l_BoxData, *m_pDefMaterial);
        l_Box->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        l_Box->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
        l_Box->setName(PAD_IDENTIFY);
        l_Box->userData = (void *)&(m_Pads[i]);
        l_pJointActor->attachShape(*l_Box);

        l_pJointActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
        l_pRefPxScene->addActor(*l_pJointActor);
        m_Pads[i].m_Joints[1] = l_pJointActor;
        l_pJointActor->setWakeCounter(PX_MAX_F32);

        l_PadMatrix.p = physx::PxVec3(0.0f, l_PadMatrix.p.x - l_Matrix.p.x, 0.0f);
        l_Matrix.p = physx::PxVec3(0.0f, 0.0f, 0.0f);
        l_Matrix.q = i > 2 ? l_RightRot : l_LeftRot;
        
        m_Pads[i].m_pBone = physx::PxRevoluteJointCreate(*l_pRefPxInst, m_Pads[i].m_Joints[0], l_Matrix, m_Pads[i].m_Joints[1], l_PadMatrix);
        m_Pads[i].m_pBone->setLimit(physx::PxJointAngularLimitPair(0.0f, physx::PxPi * 0.5f));
        m_Pads[i].m_pBone->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, true);

        m_Pads[i].m_OriginVec = glm::vec2(m_Pads[i].m_pRefPadNode->m_Tranform[0][3], m_Pads[i].m_pRefPadNode->m_Tranform[2][3]) -
                                glm::vec2(l_pArmNode->m_Tranform[0][3], l_pArmNode->m_Tranform[2][3]);
        m_Pads[i].m_OriginVec = glm::normalize(m_Pads[i].m_OriginVec);
        m_Pads[i].m_pBone->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eDRIVE_ENABLED, true);
        
        m_Pads[i].m_pBone->setDriveVelocity(m_DriverSpeed);
        m_Pads[i].m_bToBack = true;
        m_Pads[i].m_bDelaySignal = false;
        
#ifdef MAME_DEBUG
        m_Pads[i].m_pBone->setConstraintFlag(physx::PxConstraintFlag::eVISUALIZATION, true);
#endif
    }

    const char *c_pPunchTarget[PNCHMN_NUMPUNCH] = {
        "PunchLeft",
        "PunchRight"};
    for( unsigned int i=0 ; i<PNCHMN_NUMPUNCH ; ++i )
    {
        m_PunchNode[i].m_pRefNode = a_pRoot->find(c_pPunchTarget[i]);
        unsigned int l_RefMeshIdx = l_pModelFile->find(c_pPunchTarget[i])->m_RefMesh.front();
        auto l_pRefMesh = l_pModelFile->m_Meshes[l_RefMeshIdx];
        
        physx::PxTransform l_Transform;
        l_Transform.p = physx::PxVec3(m_PunchNode[i].m_pRefNode->m_Tranform[0][3], m_PunchNode[i].m_pRefNode->m_Tranform[1][3], m_PunchNode[i].m_pRefNode->m_Tranform[2][3]);
        l_Transform.q = physx::PxQuat(0.0f, 0.0f, 0.0f, 1.0f);
        physx::PxRigidDynamic *l_pPunchActor = l_pRefPxInst->createRigidDynamic(l_Transform);
        l_pPunchActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
        
        physx::PxBoxGeometry l_BoxData = {};
        l_BoxData.halfExtents = physx::PxVec3(l_pRefMesh->m_BoxSize.x, l_pRefMesh->m_BoxSize.y, l_pRefMesh->m_BoxSize.z);
        
        physx::PxShape *l_Box = l_pRefPxInst->createShape(l_BoxData, *m_pDefMaterial);
        l_Box->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        l_Box->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
        l_Box->userData = (void *)&(m_PunchNode[i]);
        l_Box->setName(PUNCH_IDENTIFY);
        l_pPunchActor->attachShape(*l_Box);

        l_pRefPxScene->addActor(*l_pPunchActor);
        m_PunchNode[i].m_pActor = l_pPunchActor;
    }
    
    getPhysxScene()->setSimulationEventCallback(this);
    l_pRefPxScene->unlockWrite();
}

void vr_device_pnchmn::update(const int a_Time)
{
    if( m_bAutoFireFlag ) m_bAutoFirState = !m_bAutoFirState;

    getPhysxScene()->lockWrite();
    for( auto it=m_PunchMap.begin() ; it!=m_PunchMap.end() ; ++it )
    {
        glm::quat l_Rot = glm::quat_cast(m_PunchNode[it->second].m_pRefNode->m_Tranform);
        m_PunchNode[it->second].m_pRefNode->m_Tranform[1][3] += m_PunchFix;

        physx::PxTransform l_Tansform;
        l_Tansform.p = physx::PxVec3(m_PunchNode[it->second].m_pRefNode->m_Tranform[0][3], m_PunchNode[it->second].m_pRefNode->m_Tranform[1][3], m_PunchNode[it->second].m_pRefNode->m_Tranform[2][3]);
        l_Tansform.q = physx::PxQuat(l_Rot.x, l_Rot.y, l_Rot.z, l_Rot.w);
        m_PunchNode[it->second].m_pActor->setGlobalPose(l_Tansform);
    }
    getPhysxScene()->simulate(a_Time * 1.0f / 60.0f);
    
    getPhysxScene()->lockRead();
    getPhysxScene()->fetchResults(true);
    
    for( unsigned int i=0 ; i<PNCHMN_NUMPAD ; ++i )
    {
        physx::PxVec3 l_ActorVec = m_Pads[i].m_Joints[1]->getGlobalPose().p - m_Pads[i].m_Joints[0]->getGlobalPose().p;
        m_Pads[i].m_CurrAngle = glm::angle(m_Pads[i].m_OriginVec, glm::vec2(l_ActorVec.x, l_ActorVec.z)) * 0.5f;
        m_Pads[i].m_Rotation = glm::rotate(glm::mat4x4(), m_Pads[i].m_CurrAngle, glm::vec3(0.0f, 0.0f, i<3 ? -1.0f : 1.0f));

        m_Pads[i].m_pRefPadNode->m_Tranform = m_Pads[i].m_pRefPadNode->m_Origin * m_Pads[i].m_Rotation * m_Pads[i].m_pRefPadNode->m_pRefParent->m_Tranform;
        m_Pads[i].m_pRefPadNode->update();

        physx::PxRigidDynamic *l_pBody = (physx::PxRigidDynamic *)m_Pads[i].m_Joints[1];
        if( l_pBody->isSleeping() ) l_pBody->setWakeCounter(PX_MAX_F32);
    }
    getPhysxScene()->unlockRead();
    getPhysxScene()->unlockWrite();
}


void vr_device_pnchmn::handlePosition(unsigned int a_DeviceIdx, vr::TrackedDevicePose_t &a_TrackedDevice, vr::ETrackedDeviceClass a_Role)
{
    int l_TargetIdx = -1;
    switch( a_Role )
    {
        case vr::TrackedDeviceClass_Controller:{
            auto l_IdxIt = m_PunchMap.find(a_DeviceIdx);

            if( m_PunchMap.end() != l_IdxIt ) l_TargetIdx = l_IdxIt->second;
            else if( m_PunchMap.size() < 2 )
            {
                l_TargetIdx = m_PunchMap.empty() ? 0 : 1;
                m_PunchMap.insert(std::make_pair(a_DeviceIdx, l_TargetIdx));
                m_PunchNode[l_TargetIdx].m_DeviceIdx = a_DeviceIdx;
            }
            }break;
        
        default:break;
    }

    if( -1 != l_TargetIdx )
    {
        m_PunchNode[l_TargetIdx].m_pRefNode->m_Origin = getMatrix(a_TrackedDevice);
        m_PunchNode[l_TargetIdx].m_Velocy = physx::PxVec3(a_TrackedDevice.vVelocity.v[0], a_TrackedDevice.vVelocity.v[1], a_TrackedDevice.vVelocity.v[2]);
    }
}

void vr_device_pnchmn::handleInput(vr::VREvent_t a_VrEvent)
{
    switch( a_VrEvent.eventType )
    {
        case vr::VREvent_TrackedDeviceActivated:{
            }break;

        case vr::VREvent_TrackedDeviceDeactivated:{
            }break;

        case vr::VREvent_TrackedDeviceUpdated:{
            }break;

        case vr::VREvent_ButtonPress:{
            switch( a_VrEvent.data.controller.button )
            {
                case vr::k_EButton_SteamVR_Touchpad:{
                    glm::vec2 l_Pos(vr_machine::singleton().getTouchPadPos(a_VrEvent.trackedDeviceIndex));
                    vr_machine::singleton().sendInput(l_Pos.x <= 0.0f ? VRInputDefine::CUSTOM_1 : VRInputDefine::CUSTOM_2, true);
                    }break;

                case vr::k_EButton_Grip:
                    m_bAutoFireFlag = true;
                    break;
            }
            }break;

        case vr::VREvent_ButtonUnpress:{
            switch( a_VrEvent.data.controller.button )
            {
                case vr::k_EButton_SteamVR_Touchpad:{
                    vr_machine::singleton().sendInput(VRInputDefine::CUSTOM_1, false);
                    vr_machine::singleton().sendInput(VRInputDefine::CUSTOM_2, false);
                    }break;

                case vr::k_EButton_Grip:
                    m_bAutoFireFlag = false;
                    break;
            }
            }break;

        default:break;
    }
}

void vr_device_pnchmn::sendMessage(int a_ArgCount, va_list a_ArgList)
{
    for( unsigned int i=0 ; i<a_ArgCount ; ++i )
    {
        int l_EventID = va_arg(a_ArgList, int);
        switch( l_EventID )
        {
            case LIGHT_SWITCH_LT:
            case LIGHT_SWITCH_LM:
            case LIGHT_SWITCH_LB:
            case LIGHT_SWITCH_RT:
            case LIGHT_SWITCH_RM:
            case LIGHT_SWITCH_RB:{
                int l_State = va_arg(a_ArgList, int);
                int l_PadIdx = l_EventID - LIGHT_SWITCH_LT;
                m_Pads[l_PadIdx].m_LightOn = 0 != l_State ? 1 : 0;
                if( 0 == m_Pads[l_PadIdx].m_LightOn && m_Pads[l_PadIdx].m_bDelaySignal )
                {
                    m_Pads[l_PadIdx].m_bDelaySignal = false;
                    m_Pads[l_PadIdx].m_pBone->setDriveVelocity(m_DriverSpeed);
                }
                }break;

            case MOTOR_LT:
            case MOTOR_LM:
            case MOTOR_LB:
            case MOTOR_RT:
            case MOTOR_RM:
            case MOTOR_RB:{
                int l_PadIdx = l_EventID - MOTOR_LT;
                m_Pads[l_PadIdx].m_bToBack = va_arg(a_ArgList, bool);
                if( m_Pads[l_PadIdx].m_bToBack && 0 != m_Pads[l_PadIdx].m_LightOn )
                {
                    m_Pads[l_PadIdx].m_bDelaySignal = true;
                }
                else
                {
                    m_Pads[l_PadIdx].m_bDelaySignal = false;
                    physx::PxRigidDynamic *l_pBody = (physx::PxRigidDynamic *)m_Pads[l_PadIdx].m_Joints[1];
                    m_Pads[l_PadIdx].m_pBone->setDriveVelocity(m_Pads[l_PadIdx].m_bToBack ? m_DriverSpeed : -m_DriverSpeed);
                }
                }break;

            default:break;
        }
    }
}

void* vr_device_pnchmn::getHandleState(int a_Handle)
{
    switch( a_Handle )
    {
        case MOTOR_LT:
        case MOTOR_LM:
        case MOTOR_LB:
        case MOTOR_RT:
        case MOTOR_RM:
        case MOTOR_RB:{
            int64_t l_Result = PAD_IO_MAX;
            if( m_bAutoFireFlag )
            {
                int64_t l_Res = m_bAutoFirState ? PAD_IO_MAX : PAD_IO_MIN;
                return (void *)l_Result;
            }
            else
            {
                unsigned int l_PadIdx = a_Handle - MOTOR_LT; 
                l_Result = m_Pads[l_PadIdx].m_CurrAngle <= m_PadDeadZone ? PAD_IO_MIN : PAD_IO_MAX;
                return (void *)l_Result;
            }
            }

        default:break;
    }

    return nullptr;
}

void vr_device_pnchmn::onTrigger(physx::PxTriggerPair *a_pPairs, physx::PxU32 a_Count)
{
    for( unsigned int i=0 ; i<a_Count ; ++i )
    {
        if( nullptr == a_pPairs[i].triggerShape->getName() || nullptr == a_pPairs[i].otherShape->getName() ||
            0 == strcmp(PUNCH_IDENTIFY, a_pPairs[i].triggerShape->getName()) ||
            physx::PxPairFlag::eNOTIFY_TOUCH_FOUND != a_pPairs[i].status ) continue;
        
        PunchData *l_pPunch = (PunchData *)a_pPairs[i].otherShape->userData;
        if( l_pPunch->m_Velocy.z < 0.0f ) continue;

        PadData *l_pPad = (PadData *)a_pPairs[i].triggerShape->userData;
        physx::PxRigidDynamic *l_pBody = (physx::PxRigidDynamic *)l_pPad->m_Joints[1];
        l_pBody->addForce(l_pPunch->m_Velocy * vr_machine::singleton().getScaler() * m_PunchWeight);

        vr_machine::singleton().doVibrate(l_pPunch->m_DeviceIdx, 500);
    }
}