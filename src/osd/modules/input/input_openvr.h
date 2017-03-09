#ifndef INPUT_OPENVR_H_
#define INPUT_OPENVR_H_

#include "input_common.h"
#include "winutil.h"

class openvr_helper
{
public:
	openvr_helper();
	virtual ~openvr_helper();
	void initVirtualDevice(running_machine &machine, wininput_module &module);
};

class openvr_device : public device_info
{
public:
	openvr_device(running_machine &machine, const char *name, input_module &module);
	virtual ~openvr_device();

	void initItems();

protected:
	void poll() override;
	void reset() override;
};

#endif