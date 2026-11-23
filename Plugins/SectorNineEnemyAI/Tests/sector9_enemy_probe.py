import unreal as u, json, pathlib
root=pathlib.Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
rows=[]
for name in ['BP_Turret']+['BP_Monster_Lv'+str(i) for i in range(1,6)]:
    cls=u.EditorAssetLibrary.load_blueprint_class('/Game/Enemy/Turret/'+name)
    c=u.get_default_object(cls); actor=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(cls,u.Vector(0,0,99999),transient=True)
    row={'name':name,'props':{},'components':{}}
    for key in ('Health','Base Damage','Base_Damage','FireSpeed','Agility'):
        try: row['props'][key]=c.get_editor_property(key)
        except Exception: pass
    for comp in actor.get_components_by_class(u.SceneComponent):
        row['components'][comp.get_name()]=str(comp.get_scaled_box_extent() if isinstance(comp,u.BoxComponent) else comp.get_relative_transform().translation)
    rows.append(row);u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(actor)
(root/'docs/evidence/item2/enemy-ai/baseline.json').write_text(json.dumps(rows,indent=2))
