# Run once after building the plugin; save only the shared enemy Blueprint.
import unreal as u
bp=u.EditorAssetLibrary.load_asset('/Game/Enemy/Turret/BP_Turret')
parent=u.load_class(None,'/Script/SectorNineEnemyAI.SectorNineSentinel')
assert parent
u.BlueprintEditorLibrary.reparent_blueprint(bp,parent)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.generated_class().get_super_class()==parent if hasattr(bp.generated_class(),'get_super_class') else isinstance(u.get_default_object(bp.generated_class()),u.SectorNineSentinel)
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
