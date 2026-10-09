// Send GTA's final rendered camera to BeamNG every frame. (sheet systems: gta_camera_sync)
#include "game.hpp"
#include "../core/platform.hpp"
#include "../gen/natives.hpp"

namespace beamls::game::camera
{
	void update(Game &g)
	{
		const Vector3 p = CAMERA::GET_FINAL_RENDERED_CAM_COORD();
		const Vector3 r = CAMERA::GET_FINAL_RENDERED_CAM_ROT(2);
		CamSample c;
		c.seq = ++g.camSeq;
		c.pos = V3(p.x, p.y, p.z);
		c.rot = fromGtaRot(r.x, r.y, r.z);
		c.fov = CAMERA::GET_FINAL_RENDERED_CAM_FOV();
		c.nearClip = CAMERA::GET_FINAL_RENDERED_CAM_NEAR_CLIP();
		platform::gtaClientSize(c.w, c.h);
		c.time = g.now;
		g.cam = c;
		// The cinematic idle camera would cut in after 30 s without input (universal-modder gotcha 15).
		CAMERA::INVALIDATE_IDLE_CAM();

		msg::Cam m;
		m.seq = c.seq;
		m.px = c.pos.x;
		m.py = c.pos.y;
		m.pz = c.pos.z;
		m.qx = c.rot.x;
		m.qy = c.rot.y;
		m.qz = c.rot.z;
		m.qw = c.rot.w;
		m.fov = c.fov;
		m.near = c.nearClip;
		m.w = c.w;
		m.h = c.h;
		g.bridge.send(m);
	}
}
