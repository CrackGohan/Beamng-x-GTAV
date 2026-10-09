// BeamNG x Los Santos: draws the BeamNG car (captured from BeamNG's window) into GTA's frame.
// For each GTA pixel: cast the view ray from GTA's camera, intersect the BeamNG car's box, depth-test that
// against GTA's depth buffer, move the hit point to where the car was when BeamNG drew the picture, project
// it into BeamNG's camera and sample the picture there; BeamNG's key-colour background is transparent.
// The add-on in BeamLS.asi sets every uniform below each frame. Approach adapted from universal-modder's
// Minecraft x GTA V shader (MIT).
#include "ReShade.fxh"

texture BeamNGTex : BEAMNG;
sampler sBeamNG { Texture = BeamNGTex; AddressU = CLAMP; AddressV = CLAMP; MinFilter = LINEAR; MagFilter = LINEAR; };

uniform bool BeamLSActive = false;

// GTA camera for the presented picture: position, rotation rows (columns = right, forward, up), tan(vfov/2), aspect.
uniform float3 GtaCamPos = float3(0, 0, 0);
uniform float3 GtaCamR0 = float3(1, 0, 0);
uniform float3 GtaCamR1 = float3(0, 1, 0);
uniform float3 GtaCamR2 = float3(0, 0, 1);
uniform float2 GtaCamTan = float2(0.4663, 1.7778);
uniform float GtaNear = 0.15;

// The BeamNG car's box now (centre, rotation rows, half extents: right, forward, up).
uniform float3 BoxCentre = float3(0, 0, 0);
uniform float3 BoxR0 = float3(1, 0, 0);
uniform float3 BoxR1 = float3(0, 1, 0);
uniform float3 BoxR2 = float3(0, 0, 1);
uniform float3 BoxHalf = float3(1, 2.3, 0.8);

// Where a point on the car now was when BeamNG drew the picture: p_then = Map * p_now + MapT.
uniform float3 MapR0 = float3(1, 0, 0);
uniform float3 MapR1 = float3(0, 1, 0);
uniform float3 MapR2 = float3(0, 0, 1);
uniform float3 MapT = float3(0, 0, 0);

// BeamNG's camera for the captured picture.
uniform float3 BngCamPos = float3(0, 0, 0);
uniform float3 BngCamR0 = float3(1, 0, 0);
uniform float3 BngCamR1 = float3(0, 1, 0);
uniform float3 BngCamR2 = float3(0, 0, 1);
uniform float2 BngCamTan = float2(0.4663, 1.7778);

uniform float3 KeyColor = float3(1.0, 0.0, 1.0);
uniform float KeyTolerance = 0.18;

uniform float HostFar < ui_type = "drag"; ui_min = 100.0; ui_max = 100000.0; ui_label = "GTA far plane (m)"; > = 10000.0;
uniform float DepthBias < ui_type = "drag"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01; ui_label = "Depth bias (m)";
	ui_tooltip = "How far behind GTA's surface the car may still show (hides z-fighting at the road)."; > = 0.15;
uniform float EdgeSoftness < ui_type = "drag"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01; ui_label = "Key edge softness"; > = 0.6;
uniform int DebugView < ui_type = "combo"; ui_items = "Composite\0Car box\0GTA depth (1 m bands)\0BeamNG picture\0"; > = 0;

float hostLinear(float d)
{
	// GTA V: reversed Z (ReShade.ini: RESHADE_DEPTH_INPUT_IS_REVERSED=1).
	const float n = GtaNear, f = HostFar;
	return d > 0.0 ? n * f / (n + d * (f - n)) : 1e9;
}

float3x3 rowsToMatrix(float3 r0, float3 r1, float3 r2) { return float3x3(r0, r1, r2); }

// Ray (origin o, direction d, both in box space) against the box [-h, h]: entry distance, or -1.
float boxEntry(float3 o, float3 d, float3 h)
{
	const float3 inv = 1.0 / (abs(d) > 1e-6 ? d : (d >= 0 ? 1e-6 : -1e-6));
	const float3 t0 = (-h - o) * inv, t1 = (h - o) * inv;
	const float3 tmin = min(t0, t1), tmax = max(t0, t1);
	const float tn = max(max(tmin.x, tmin.y), tmin.z), tf = min(min(tmax.x, tmax.y), tmax.z);
	if (tf < max(tn, 0.0))
		return -1.0;
	return max(tn, 0.0);
}

float keyAlpha(float3 c)
{
	const float d = distance(c, KeyColor);
	return smoothstep(KeyTolerance, KeyTolerance * (1.0 + 2.0 * EdgeSoftness) + 1e-4, d);
}

float3 despill(float3 c)
{
	// Magenta spill shows as red and blue above green on the car's edges.
	const float limit = c.g + 0.5 * abs(c.r - c.b);
	return float3(min(c.r, max(limit, c.g)), c.g, min(c.b, max(limit, c.g)));
}

float4 PS_BeamLS(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
	const float3 host = tex2D(ReShade::BackBuffer, uv).rgb;
	if (!BeamLSActive)
		return float4(host, 1.0);

	// View ray in GTA's camera space: right, forward (= 1), up. Its parameter t is the view depth.
	const float3 rayCam = float3((uv.x * 2.0 - 1.0) * GtaCamTan.x * GtaCamTan.y, 1.0, (1.0 - uv.y * 2.0) * GtaCamTan.x);
	const float3x3 camRot = rowsToMatrix(GtaCamR0, GtaCamR1, GtaCamR2);
	const float3 rayWorld = mul(camRot, rayCam);

	const float3x3 boxRot = rowsToMatrix(BoxR0, BoxR1, BoxR2);
	const float3 o = mul(transpose(boxRot), GtaCamPos - BoxCentre);
	const float3 d = mul(transpose(boxRot), rayWorld);
	const float t = boxEntry(o, d, BoxHalf);

	const float zHost = hostLinear(tex2Dlod(ReShade::DepthBuffer, float4(uv, 0, 0)).x);
	if (DebugView == 2)
		return float4(lerp(float3(0.1, 0.1, 0.1), float3(1.0, 0.85, 0.3), step(0.5, frac(zHost))), 1.0);
	if (DebugView == 3)
		return float4(tex2D(sBeamNG, uv).rgb, 1.0);
	if (t < 0.0)
		return float4(host, 1.0);
	if (DebugView == 1)
		return float4(lerp(host, float3(0.0, 1.0, 0.3), 0.35), 1.0);
	if (zHost + DepthBias < t)
		return float4(host, 1.0); // something in GTA is in front of the car's box

	// The box point now -> where it was in BeamNG's picture -> BeamNG's camera -> picture coordinates.
	const float3 pNow = GtaCamPos + rayWorld * t;
	const float3 pThen = mul(rowsToMatrix(MapR0, MapR1, MapR2), pNow) + MapT;
	const float3 local = mul(transpose(rowsToMatrix(BngCamR0, BngCamR1, BngCamR2)), pThen - BngCamPos);
	if (local.y <= 1e-3)
		return float4(host, 1.0);
	const float2 ndc = float2(local.x / (local.y * BngCamTan.x * BngCamTan.y), local.z / (local.y * BngCamTan.x));
	if (any(abs(ndc) > 1.0))
		return float4(host, 1.0);
	const float2 buv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	const float3 car = tex2Dlod(sBeamNG, float4(buv, 0, 0)).rgb;
	const float a = keyAlpha(car);
	return float4(lerp(host, despill(car), a), 1.0);
}

technique BeamLS < ui_label = "BeamNG x Los Santos"; ui_tooltip = "Draws the BeamNG car into GTA V. Set by BeamLS.asi; leave on."; >
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = PS_BeamLS;
	}
}
