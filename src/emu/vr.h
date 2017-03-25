// license:BSD-3-Clause
// copyright-holders:Ian Wu
/***************************************************************************

    vr.h

    Optional vr machine for mame.

****************************************************************************/

#pragma once

#ifndef __VR_H__
#define __VR_H__

#include "emu.h"
#include "openvr.h"
#include <map>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include "PxPhysicsAPI.h"
#include "extensions/PxExtensionsAPI.h"
#include "vr_interface.h"

//**************************************************************************
//  CONSTANTS
//**************************************************************************


//**************************************************************************
//  MACROS
//**************************************************************************
#define PHYSX_SAFE_RELEASE(p) if( nullptr != p ) p->release(); p = nullptr

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// forward definitions
struct buffer11;
struct texture11;
struct texture11_view;
class vr_device_interface;
class model_file;

// ======================> vr_option
class vr_option// : public core_options 
{
public:
    typedef std::map<std::string, std::string> param_collect;

public:
    vr_option();
    virtual ~vr_option();

    bool parseIni(std::string a_Filename);
    param_collect& getParams(std::string a_SectionName);
    template<typename T>
    T getParamValue(std::string a_SectionName, std::string a_ParamName, T a_Default, std::function<T(std::string)> a_ConvertFunc)
    {
        auto l_SecIt = m_Sections.find(a_SectionName);
        if( m_Sections.end() == l_SecIt ) return a_Default;

        auto l_ParamIt = l_SecIt->second.find(a_ParamName);
        if( l_SecIt->second.end() == l_ParamIt ) return a_Default;

        return a_ConvertFunc(l_ParamIt->second);
    }
    int getParamValue(std::string a_SectionName, std::string a_ParamName, int a_Default);
    float getParamValue(std::string a_SectionName, std::string a_ParamName, float a_Default);
    std::string getParamValue(std::string a_SectionName, std::string a_ParamName, std::string a_Default);

private:
    std::map<std::string, param_collect> m_Sections;
};

// ======================> vr_machine

// vr_machine is a class manage virtual machine render and input/output
class vr_machine
{
public:
    struct model_instance
    {
        model_instance();
        ~model_instance();

        std::string m_RefNodeName;
        glm::mat4x4 *m_pRefWorld;
        std::vector<void *> m_Uniform;//data
    };
    struct machine_model
    {
        machine_model();
        ~machine_model();

        unsigned int m_FxIndex;
        std::vector< std::pair<texture11 *, texture11_view *> > m_Textures;
        buffer11 *m_pVtxBuffer;
        buffer11 *m_pIndexuffer;
        unsigned int m_model_index;
        std::vector<model_instance *> m_pRefWorld;
    };
    struct machine_fx_item
    {
        machine_fx_item();
        ~machine_fx_item();

        std::string m_UnitformName;
        int m_UniformType; // uniform::uniform_type
        int m_Size;
        void *m_pRefTarget;
    };
    struct machine_fx
    {
        machine_fx();
        ~machine_fx();

        std::string m_FxName;
        std::vector<machine_fx_item> m_UniformMap;
    };
    struct machine_node
    {
        machine_node();
        ~machine_node();
        
        void update(glm::mat4x4 &a_World);
        void update();
        machine_node* find(std::string a_Name);

        std::string m_Name;
        machine_node *m_pRefParent;
        glm::mat4x4 m_Origin, m_Tranform;
        std::vector<machine_node *> m_Children;
    };

public:
    static vr_machine& singleton(){ static vr_machine s_Inst; return s_Inst; }

    void init(running_machine *a_pCurrMachine);
    void uninit();

    const model_file* getRefModelFile(){ return m_pModelData; }
    const std::vector<machine_model *>& getModels(){ return m_machine_model; }// model name : weak model pointer
    const std::vector<machine_fx *>& getFx(){ return m_machine_fx; }// model name : weak model pointer
    glm::uvec2 getEyeTextureSize(){ return m_EyeTextureSize; }

    // render
    void update(const int a_Time);
    void commit(void *a_pLeftEyeTexture, void *a_pRightEyeTexture, vr::ETextureType a_Api);
    void updateFx(unsigned int a_ModelIdx, unsigned int a_InstIdx);

    // event handler
    void handleInput();
    void sendMessage(int a_ArgCount, va_list a_ArgList);
    void* getHandleState(int a_Handle);

    // input handler
    void sendInput(VRInputDefine a_Btn, bool a_bPressed);
    bool getInputPressed(VRInputDefine a_Btn);
    void doVibrate(unsigned int a_DeviceIdx, unsigned short a_MicroSec);

    // machine state
    bool isValid(){ return nullptr != m_pHMD; }
    std::string getDirPath(){ return m_DirPath; }
    glm::mat4x4& getViewProject(){ return m_ViewProject; }// output 
    glm::mat4x4& getLeftEye(){ return m_LeftVP; }
    glm::mat4x4& getRightEye(){ return m_RightVP; }
    void setCurrEye(bool a_bLeft){ m_ViewProject = a_bLeft ? m_LeftVP : m_RightVP; }
    float getScaler(){ return m_Scaler; }
    glm::vec2 getTouchPadPos(int a_DeviceIdx);

private:
    // construction/destruction
    vr_machine();
    virtual ~vr_machine();
    void initHMDProjection(vr::Hmd_Eye a_Eye);
    void initEyeToHeadMatrix(vr::Hmd_Eye a_Eye);

    glm::uvec2 m_EyeTextureSize;
    vr::IVRSystem *m_pHMD;
    vr::IVRRenderModels *m_pRenderModel;
    std::vector<machine_model *> m_machine_model;// model name : weak model pointer
    std::vector<machine_fx *> m_machine_fx;

    vr_device_interface *m_pInterface;
    running_machine *m_pRefMachine;
    model_file *m_pModelData;
    machine_node *m_pRoot;

    vr::TrackedDevicePose_t m_DevicePos[vr::k_unMaxTrackedDeviceCount];
    std::string m_DirPath;
    glm::mat4x4 m_EyeProjection[2], m_HeadToEye[2];
    glm::mat4x4 m_ViewProject, m_LeftVP, m_RightVP;
    float m_Scaler;

    bool m_InputMap[VRInputDefine::NUM_INPUT];
};

class vr_device_interface
{
public:
    vr_device_interface();
    virtual ~vr_device_interface();
    
    void init(vr_option &a_Config, std::vector<vr_machine::machine_model *> &a_Container, vr_machine::machine_node* &a_Root, std::vector<vr_machine::machine_fx *> &a_Fx);
    virtual void update(const int a_Time) = 0;
    virtual void handleHmdPosition(vr::TrackedDevicePose_t &a_TrackedDevice){}
    virtual void handlePosition(unsigned int a_DeviceIdx, vr::TrackedDevicePose_t &a_TrackedDevice, vr::ETrackedDeviceClass a_Role) = 0;
    virtual void handleInput(vr::VREvent_t a_VrEvent) = 0;
    virtual void sendMessage(int a_ArgCount, va_list a_ArgList) = 0;
    virtual void* getHandleState(int a_Handle) = 0;

protected:
    void initPhysX();
    void initMachineNode(vr_machine::machine_node* &a_pRoot);
    virtual void initMachine(vr_option &a_Config, std::vector<vr_machine::machine_model *> &a_Container, vr_machine::machine_node* a_pRoot, std::vector<vr_machine::machine_fx *> &a_Fx) = 0;
    virtual void initPhysXTolerance(physx::PxTolerancesScale &a_Tolerance){}
    virtual void initPhysXScene(physx::PxSceneDesc &a_SceneDesc){}

    glm::mat4x4 getMatrix(vr::TrackedDevicePose_t &a_TrackedDevice);
    physx::PxFoundation* getPhysXBase(){ return m_pPxBase; }
    physx::PxPhysics* getPhysXInst(){ return m_pPhysics; }
    physx::PxScene* getPhysxScene(){ return m_pPxScene; }

private:
    physx::PxFoundation *m_pPxBase;
    physx::PxProfileZoneManager *m_pProfileZoneMgr;
    physx::PxPhysics *m_pPhysics;
    physx::PxDefaultAllocator m_PxAllocator;
    physx::PxDefaultErrorCallback m_PxErrorHandle;

    physx::PxDefaultCpuDispatcher *m_pCpuDispatcher;
    physx::PxScene *m_pPxScene;

    physx::PxVisualDebuggerConnection *m_pPxDebugger;
};
#endif