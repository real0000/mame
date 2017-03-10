#pragma once

#ifndef __VR_INTERFACE_H__
#define __VR_INTERFACE_H__

void vr_interface_send_message(int a_ArgCount, ...);
void* vr_interface_get_handle_state(int a_Handle);

// for input module
enum VRInputDefine : int64_t
{
    TRIGGER,
    MENU,
    GRIP,
    CUSTOM_1,
    CUSTOM_2,
    CUSTOM_3,
    CUSTOM_4,
    CUSTOM_5,

    NUM_INPUT
};
int vr_interface_button_get_state(void *device_internal, void *item_internal);

#endif