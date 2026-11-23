"""Transient in-level E/R test; run with -ExecutePythonScript, no asset saves."""
import unreal as u, time, pathlib, json, traceback
root=pathlib.Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()));out=root/'Saved/SectorThrow';out.mkdir(exist_ok=True)
e=u.get_editor_subsystem(u.LevelEditorSubsystem);worlds=u.get_editor_subsystem(u.UnrealEditorSubsystem);u.EditorPythonScripting.set_keep_python_script_alive(True);e.editor_play_simulate()
phase='begin';end=time.monotonic()+120;row={};cnt=0

def finish():
    (out/'pie.json').write_text(json.dumps(row,indent=2));u.unregister_slate_post_tick_callback(handle);e.editor_request_end_play();u.SystemLibrary.quit_editor()

busy=False
def tick(_):
    global busy
    if busy:return
    busy=True
    global phase,cnt,cube,mesh,wait,pc,p,button,door
    try:
        if time.monotonic()>end:raise RuntimeError('timeout '+phase)
        w=worlds.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0) if w else None
        if w and (not p or p.get_class().get_name()!='BP_PlayerChar_C'):
            pc=u.GameplayStatics.get_player_controller(w,0)
            if not pc:return
            cls=u.load_class(None,'/Game/Player/BP_PlayerChar.BP_PlayerChar_C');tr=u.Transform(location=u.Vector(0,0,500));u.SystemLibrary.execute_console_command(w,'summon /Game/Player/BP_PlayerChar.BP_PlayerChar_C',pc);p=u.GameplayStatics.get_all_actors_of_class(w,cls)[0];pc.possess(p)
        if not p:return
        pc=u.GameplayStatics.get_player_controller(w,0);cmd=lambda s:u.SystemLibrary.execute_console_command(w,s,pc)
        def actors(path):return u.GameplayStatics.get_all_actors_of_class(w,u.load_class(None,path+'.'+path.rsplit('/',1)[-1]+'_C'))
        if phase=='begin':
            buttons=actors('/Game/Interactables/AnimatedInteractables/BP_Button');cubes=actors('/Game/Interactables/BP_PhysicsPickup');row['pawn']=p.get_class().get_name();row['buttons']=[[a.get_name(),str(a.get_actor_location()),str(a.get_editor_property('PickupColor'))] for a in buttons];row['cubes']=[[a.get_name(),str(a.get_actor_location()),str(a.get_editor_property('PickupColor'))] for a in cubes];cube=cubes[0];mesh=cube.get_component_by_class(u.StaticMeshComponent);button=next(b for b in buttons if b.get_editor_property('PickupColor')==cube.get_editor_property('PickupColor'));door=button.get_editor_property('TargetDoor');row['initial_unlock']=door.get_editor_property('isUnlocked');p.character_movement.set_movement_mode(u.MovementMode.MOVE_FLYING);dest=mesh.get_world_location()-u.Vector(140,0,0);p.set_actor_location(dest,False,True);phase='aim';wait=0;return
        wait+=1
        if phase=='aim':
            pc.set_control_rotation(u.MathLibrary.find_look_at_rotation(pc.player_camera_manager.get_camera_location(),mesh.get_world_location()));
            if wait>8:
                cube.call_method('INT_Interact',(p,));phase='interact';wait=0
        elif phase=='interact' and wait>3:cmd('Input.-key E');phase='held';wait=0
        elif phase=='held' and wait>40:
            row['parent']=str(mesh.get_attach_parent());row['attached']=mesh.get_attach_parent() is not None and mesh.get_attach_parent().get_owner()==p;row['sim_held']=mesh.is_simulating_physics();assert row['attached'];phase='target';wait=0
            # Align with the original button from a short ballistic approach.
            target=button.get_component_by_class(u.BoxComponent).get_world_location();p.set_actor_location(target-u.Vector(440,0,-40),False,True);pc.set_control_rotation(u.Rotator(0,0,0))
        elif phase=='target' and wait>12:
            cmd('Shot');row['held_position']=str(mesh.get_world_location());phase='release';wait=0
        elif phase=='release' and wait>10:cmd('Input.+key R 1');phase='flight';wait=0
        elif phase=='flight':
            if wait==3:cmd('Input.-key R')
            v=mesh.get_physics_linear_velocity();row.setdefault('flight',[]).append([wait,str(mesh.get_world_location()),str(v),door.get_editor_property('isUnlocked')]);
            if wait>=90:row['released']=cube.get_attach_parent_actor()!=p and mesh.is_simulating_physics();row['ccd']=mesh.get_editor_property('body_instance').get_editor_property('use_ccd');finish()
    except Exception:row['error']=traceback.format_exc();finish()
    finally:busy=False
handle=u.register_slate_post_tick_callback(tick)
