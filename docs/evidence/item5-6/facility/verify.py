"""Run via UE 5.4 -run=pythonscript -script=<absolute path> -NullRHI -EnablePlugins=PythonScriptPlugin."""
import json
import unreal as u

level = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/TestEnvironment')
actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
health, rates, lights, signals = [], {}, {}, {}
for actor in actors:
    label, cls = actor.get_actor_label(), actor.get_class().get_name()
    if cls == 'BP_Collectible_Health_C':
        health.append(actor.get_actor_location().to_tuple())
    if cls.startswith('BP_Monster_Lv'):
        tier = int(cls.split('Lv')[1][0]); interval = actor.get_editor_property('FireSpeed')
        assert abs(interval - [1.00, .52, .42, .34, .28][tier-1]) < .00001
        rates.setdefault(tier, []).append(interval)
    if label.startswith('Facility_'):
        c = actor.point_light_component
        lights[label] = [c.intensity, c.attenuation_radius]
        assert lights[label] == [3600, 540]
    if label.startswith('Signal_'):
        c = actor.audio_component; at = c.attenuation_overrides
        signals[label] = [c.sound.get_name(), c.sound.get_editor_property('looping'), c.get_editor_property('auto_activate'), c.get_editor_property('override_attenuation'), at.get_editor_property('falloff_distance')]
        assert signals[label] == [label.split('_')[1], True, True, True, 420]
assert len(set(health)) == len(health) == 17
assert [len(rates[k]) for k in range(1,6)] == [1,2,2,1,2]
assert len(lights) == 4 and len(signals) == 2
print('FACILITY_PASS '+json.dumps(dict(health=sorted(health), rates=rates, lights=lights, signals=signals),sort_keys=True))
