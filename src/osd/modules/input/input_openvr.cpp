// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Brad Hughes
//============================================================
//
//  input_openvr.cpp - open vr controller support
//
//============================================================


#include "input_module.h"
#include "modules/osdmodule.h"

#if defined(OSD_WINDOWS)

#include "emu.h"

#include "input_common.h"
#include "input_windows.h"
#include "input_openvr.h"

#include "vr_interface.h"

//============================================================
//  openvr_helper - helper for win hybrid
//============================================================
openvr_helper::openvr_helper()
{
}

openvr_helper::~openvr_helper()
{
}

void openvr_helper::initVirtualDevice(running_machine &machine, wininput_module &module)
{
    openvr_device *l_pDevice = module.devicelist()->create_device<openvr_device>(machine, "open vr controller", module);
    l_pDevice->initItems();
}

//============================================================
//  openvr_device - base openvr device
//============================================================
openvr_device::openvr_device(running_machine &machine, const char *name, input_module &module)
    : device_info(machine, name, DEVICE_CLASS_JOYSTICK, module)
{
    //
}

openvr_device::~openvr_device()
{
}

void openvr_device::poll()
{
}

void openvr_device::reset()
{
}

void openvr_device::initItems()
{
    device()->add_item("Trigger"    , ITEM_ID_BUTTON1, vr_interface_button_get_state, (void *)VRInputDefine::TRIGGER);
    device()->add_item("Menu"        , ITEM_ID_BUTTON2, vr_interface_button_get_state, (void *)VRInputDefine::MENU);
    device()->add_item("Grip"        , ITEM_ID_BUTTON3, vr_interface_button_get_state, (void *)VRInputDefine::GRIP);
    device()->add_item("Custom 1"    , ITEM_ID_BUTTON4, vr_interface_button_get_state, (void *)VRInputDefine::CUSTOM_1);
    device()->add_item("Custom 2"    , ITEM_ID_BUTTON5, vr_interface_button_get_state, (void *)VRInputDefine::CUSTOM_2);
    device()->add_item("Custom 3"    , ITEM_ID_BUTTON6, vr_interface_button_get_state, (void *)VRInputDefine::CUSTOM_3);
    device()->add_item("Custom 4"    , ITEM_ID_BUTTON7, vr_interface_button_get_state, (void *)VRInputDefine::CUSTOM_4);
    device()->add_item("Custom 5"    , ITEM_ID_BUTTON8, vr_interface_button_get_state, (void *)VRInputDefine::CUSTOM_5);
}

//============================================================
//  openvr_input_modeule
//============================================================
class openvr_input_module : public wininput_module
{
public:
    openvr_input_module() :
        wininput_module(OSD_JOYSTICKINPUT_PROVIDER, "openvr")
    {
    }

    int init_internal() override
    {
        return 0;
    }

    void exit() override
    {
        wininput_module::exit();
    }

    void input_init(running_machine &machine) override
    {
        openvr_device *l_pDevice = devicelist()->create_device<openvr_device>(machine, "open vr controller", *this);
        l_pDevice->initItems();
    }
};

#else
MODULE_NOT_SUPPORTED(keyboard_input_dinput, OSD_JOYSTICKINPUT_PROVIDER, "openvr")
#endif

MODULE_DEFINITION(JOYSTICKINPUT_OPENVR, openvr_input_module)