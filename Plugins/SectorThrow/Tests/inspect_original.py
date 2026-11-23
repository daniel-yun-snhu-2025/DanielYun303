import unreal
for path in ['/Game/Player/BP_PlayerChar','/Game/Interactables/BP_PhysicsPickup','/Game/Interactables/AnimatedInteractables/BP_Button']:
    cls=unreal.EditorAssetLibrary.load_blueprint_class(path)
    unreal.SystemLibrary.execute_console_command(None,'DISASMSCRIPT '+cls.get_name())
ctx=unreal.load_asset('/Game/Inputs/Mapping_Player')
for mapping in ctx.get_editor_property('mappings'):
    unreal.log('SectorThrow mapping: '+mapping.action.get_name()+' '+str(mapping.key.get_editor_property('key_name')))
