// ReShade add-on part of BeamLS.asi: captures BeamNG's window and feeds BeamLS.fx, which draws the BeamNG
// car into GTA's frame. (sheet systems: gta_compositor, gta_capture)
#pragma once

namespace beamls::compositor
{
	bool tryRegister(void *module); // true once registered with ReShade (ReShade may load after us)
	void unregister(void *module);
	void setNotice(const char *text); // shown in ReShade's log
}
