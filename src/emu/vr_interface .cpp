// license:BSD-3-Clause
// copyright-holders:Ian Wu
/***************************************************************************

    vr.cpp

    case vr function call to vr_machine instance.

***************************************************************************/

#include "emu.h"
#include "vr.h"

#include "vr_interface.h"

void vr_interface_send_message(int a_ArgCount, ...)
{
	va_list l_ArgList;
	va_start(l_ArgList, a_ArgCount);
	vr_machine::singleton().sendMessage(a_ArgCount, l_ArgList);
	va_end(l_ArgList);
}

void* vr_interface_get_handle_state(int a_Handle)
{
	return vr_machine::singleton().getHandleState(a_Handle);
}

int vr_interface_button_get_state(void *device_internal, void *item_internal)
{
	VRInputDefine l_Btn = (VRInputDefine)(int64_t)item_internal;
	return vr_machine::singleton().getInputPressed(l_Btn) ? 1 : 0;
}