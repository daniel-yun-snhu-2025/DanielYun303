"""UE editor -ExecutePythonScript=<this file>. Uses transient PIE actors, saves no maps."""
import unreal as u, time, pathlib, json, traceback
root=pathlib.Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
out=root/'docs/evidence/item2/enemy-ai'; rows=[]; evidence={'passed':False,'variants':rows}
editor=u.get_editor_subsystem(u.LevelEditorSubsystem); worlds=u.get_editor_subsystem(u.UnrealEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True);editor.editor_play_simulate(); phase='start'; level=1; started=time.monotonic(); deadline=started+120

def gameclass(path): return u.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C')

def spawn(w,cls,pos):
    before=set(u.GameplayStatics.get_all_actors_of_class(w,cls));u.SystemLibrary.execute_console_command(w,'summon '+cls.get_path_name());a,=set(u.GameplayStatics.get_all_actors_of_class(w,cls))-before;a.set_actor_location(pos,False,True);return a

def finish():
    (out/'pie.json').write_text(json.dumps(evidence,indent=2));u.unregister_slate_post_tick_callback(handle);editor.editor_request_end_play();u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()

def step(_):
    global phase, level, deadline, enemy, box, row, player, wait, clsproj
    try:
        if time.monotonic()>deadline: raise RuntimeError('Timeout '+phase)
        w=worlds.get_game_world()
        if not w:return
        p=u.GameplayStatics.get_player_pawn(w,0)
        if phase!='start': p=player
        elif not p or p.get_class().get_name()!='BP_PlayerChar_C':
            pc=u.GameplayStatics.get_player_controller(w,0)
            if not pc:return
            tr=u.Transform(location=u.Vector(0,0,500));cls=gameclass('/Game/Player/BP_PlayerChar')
            p=spawn(w,cls,u.Vector(0,0,500));pc.set_view_target_with_blend(p)
        if phase=='start':
            player=p; player.character_movement.set_movement_mode(u.MovementMode.MOVE_FLYING)
            cls=gameclass('/Game/Enemy/Turret/BP_Monster_Lv'+str(level));clsproj=gameclass('/Game/Enemy/Turret/BP_TurretProjectile')
            pos=u.Vector(0,level*10000,5000);player.set_actor_location(pos+u.Vector(4500,0,0),False,True)
            enemy=spawn(w,cls,pos)
            box=next(c for c in enemy.get_components_by_class(u.BoxComponent) if c.get_name()=='Box');row={'level':level,'states':[]};rows.append(row);phase='patrol'; deadline=time.monotonic()+45;return
        state=str(enemy.get_editor_property('enemy_state')).split('.')[-1].split(':')[0].upper();
        if not row['states'] or row['states'][-1]!=state: row['states'].append(state)
        if phase=='patrol' and 'PATROL' in state:
            phase='alert'
        if phase in ('alert','attack','shot'):
            pos=box.get_world_location()+u.Vector(min(280,box.get_scaled_box_extent().x*.6),0,0);player.set_actor_location(pos,False,True)
            row['diagnostic']=[str(box.get_world_location()),str(player.get_actor_location()),box.is_overlapping_actor(player),enemy.is_actor_tick_enabled(),str(player.get_class().get_name()),str(box.get_collision_enabled()),str(enemy.get_actor_location())]
            if 'ALERT' in state:phase='attack'
            if phase=='attack' and 'ATTACK' in state:phase='shot'
            if phase=='shot':
                shots=[a for a in u.GameplayStatics.get_all_actors_of_class(w,clsproj) if (a.get_actor_location()-enemy.get_actor_location()).length()<1200]
                if shots:
                    row['projectile_observed']=True
                    if level==1:
                        pc=u.GameplayStatics.get_player_controller(w,0);pc.set_control_rotation(u.MathLibrary.find_look_at_rotation(player.get_actor_location()+u.Vector(0,0,50),enemy.get_actor_location()));u.SystemLibrary.execute_console_command(w,'Shot')
                    phase='leave'
        elif phase=='leave':
            player.set_actor_location(box.get_world_location()+u.Vector(5000,0,0),False,True);phase='search'
        elif phase=='search' and 'SEARCH' in state:phase='reset'
        elif phase=='reset' and 'PATROL' in state:
            if level==5:evidence['passed']=True;finish();return
            level+=1;phase='start'
    except Exception:
        evidence['error']=traceback.format_exc();finish()
handle=u.register_slate_post_tick_callback(step)
