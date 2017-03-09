#pragma once

#ifndef __VR_PNCHMN_H__
#define __VR_PNCHMN_H__

#include "emu.h"
#include "vr.h"

#define PNCHMN_NUMPAD 6
#define PNCHMN_NUMPUNCH 2

class vr_device_pnchmn : public vr_device_interface, public physx::PxSimulationEventCallback
{
public:
	vr_device_pnchmn();
	virtual ~vr_device_pnchmn();
	
	virtual void update(const int a_Time);
	virtual void handlePosition(unsigned int a_DeviceIdx, vr::TrackedDevicePose_t &a_TrackedDevice, vr::ETrackedDeviceClass a_Role);
	virtual void handleInput(vr::VREvent_t a_VrEvent);
	virtual void sendMessage(int a_ArgCount, va_list a_ArgList);
	virtual void* getHandleState(int a_Handle);

	virtual void onContact(const physx::PxContactPairHeader&, const physx::PxContactPair*, physx::PxU32){}
	virtual void onTrigger(physx::PxTriggerPair *a_pPairs, physx::PxU32 a_Count);
	virtual void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32){}
	virtual void onWake(physx::PxActor**, physx::PxU32){}
	virtual void onSleep(physx::PxActor**, physx::PxU32){}

protected:
	virtual void initMachine(vr_option &a_Config, std::vector<vr_machine::machine_model *> &a_Container, vr_machine::machine_node* a_pRoot, std::vector<vr_machine::machine_fx *> &a_Fx);

private:
	struct PadData
	{
		int m_LightOn;
		glm::mat4x4 m_Rotation;
		vr_machine::machine_node *m_pRefPadNode;
		glm::vec2 m_OriginVec;
		float m_CurrAngle;
		bool m_bToBack;
		bool m_bDelaySignal;
		physx::PxRigidActor *m_Joints[2];
		physx::PxRevoluteJoint *m_pBone;
	};
	PadData m_Pads[PNCHMN_NUMPAD];//uniform LT, LM, LB, RT, RM, RB

	struct PunchData
	{
		vr_machine::machine_node *m_pRefNode;
		physx::PxRigidDynamic *m_pActor;
		physx::PxVec3 m_Velocy;
		unsigned int m_DeviceIdx;
	};
	PunchData m_PunchNode[PNCHMN_NUMPUNCH];
	
	vr_machine::machine_node *m_pRefMachine;
	std::map<unsigned int, unsigned int> m_PunchMap;
	physx::PxMaterial *m_pDefMaterial;

	// since it is hardly to punch over 100 hit, so....
	bool m_bAutoFireFlag, m_bAutoFirState;

	float m_PadDeadZone, m_PunchFix;
	float m_PunchWeight, m_DriverSpeed;
};

#endif
