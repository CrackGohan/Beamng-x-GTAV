-- BeamNG x Los Santos: runs when BeamNG mounts this mod. Loads only the idle listener; the mashup itself
-- starts when GTA V's BeamLS.asi says hello on 127.0.0.1.
load('beamls/listener')
setExtensionUnloadMode('beamls/listener', 'manual')
