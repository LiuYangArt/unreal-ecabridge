"""
ECABridge Stress Test — Stage Performance scenario build script

Reference procedure the autonomous build agent follows. NOT meant to be run
directly — the agent reads this for build order + verify pattern, then issues
MCP calls + writes findings entries per the schema at ../findings-schema.json.

Findings file path:  findings/<engine>-<YYYYMMDD>.json
Findings entries:    append one per MCP call (authoring AND verification reads)
Stop on FAIL:        no — log and continue. The point is to surface all gaps.

ENGINE-DETECT:
  Before starting, call any MCP command (e.g. get_project_overview) and record
  the engine version in every findings entry's `engine` field.

CONTENT ROOT for all generated assets: /Game/StressTest/
"""

# ---------------------------------------------------------------------------
# PHASE 0 — Pre-flight
# ---------------------------------------------------------------------------
# 1. curl http://127.0.0.1:3000/health → expect {bridge_ready: true, commands: N>500}
# 2. list_categories → save the full category list for the help() spot-check
# 3. get_project_overview → record engine version into all findings entries
# 4. create_folder("/Game/StressTest") → idempotent
# 5. open_level("/Game/StressTest/StressTest") → if missing, create_level first
#
# LOG: one findings entry per call in this phase. Pre-flight findings tag: ["phase-0"]

# ---------------------------------------------------------------------------
# PHASE 1 — Character (skeletal + physics + materials)
# Target: ~50 MCP calls
# ---------------------------------------------------------------------------
# 1.1  Find a starter SkeletalMesh in the engine content
#      find_assets(type="SkeletalMesh", path="/Engine/", limit=5)
#      VERIFY: pick the one with the most bones via dump (we want >20 bones to test joint coverage)
#
# 1.2  Duplicate the chosen mesh into /Game/StressTest/Characters/SKM_TestChar
#      duplicate_asset(...) → assert path exists post-call
#
# 1.3  Create physics asset from mesh (Batch X)
#      create_physics_asset_from_mesh(mesh_path, dest_path, assign_to_mesh=true)
#      VERIFY: dump_physics_asset(...) → body_count > 0, constraint_count > 0
#
# 1.4  Inspect bodies (Batch X)
#      get_body_names(physics_asset_path) → capture list
#      For 3 representative bones (root, spine, hand): get_body_shapes(...)
#
# 1.5  Add a custom sphere shape on the spine (Batch X)
#      set_body_sphere(bone_name="spine_01", shape_name="custom_collision", center={x:0,y:0,z:5}, radius=15)
#      VERIFY: get_body_shapes(spine_01) contains shape with the right radius
#      EXPECTED FINDING: response should echo bone_name (per design predictions)
#
# 1.6  Set physics mode on a bone (Batch X)
#      set_body_physics_mode(bone_name="head", mode="Kinematic")
#      VERIFY: get_body_physics_mode(head) returns "Kinematic"
#
# 1.7  Set mass scale (Batch X)
#      set_body_mass_scale(bone_name="root", mass_scale=2.5)
#      VERIFY: get_body_mass_scale(root) returns 2.5
#
# 1.8  Add a constraint (Batch X)
#      add_constraint(bone1_name="hand_r", bone2_name="lowerarm_r")
#      VERIFY: get_constraints contains the new pair
#      set_constraint_limits(swing1_motion="Limited", swing1_limit_degrees=45.0, ...)
#      VERIFY: get_constraints reflects new limits
#
# 1.9  Physical material (ours-only Physics CRUD)
#      create_physical_material(path="/Game/StressTest/Physics/", name="PM_Skin",
#                                friction=0.4, restitution=0.1, density=1.05)
#      VERIFY: dump_physical_material(...)
#
# 1.10 Material with parameter collection
#      create_material_parameter_collection(path="/Game/StressTest/Materials/MPC_StageState")
#      add_material_parameter_collection_scalar("MPC_StageState", "Intensity", 1.0)
#      VERIFY: dump_material_parameter_collection(MPC_StageState)
#
# 1.11 MetaHuman head + groom (ours-only)
#      OPTIONAL — only if MetaHumanCharacter plugin is enabled. Skip with finding if not.
#      create_metahuman(...) → use the simplest available preset
#      VERIFY: dump_metahuman(...)
#
# 1.12 Mutable customization (ours-only)
#      OPTIONAL — only if Mutable plugin enabled.
#      find_assets(type="CustomizableObject") → if any exists, dump one
#      Skip authoring if no test asset; log finding ["mutable-skipped-no-asset"]
#
# 1.13 Niagara trail (Batch V + ours-only)
#      Build a minimal sprite-trail system.
#      find_assets(class_path="/Script/Niagara.NiagaraSystem", path="/Engine/")
#      create_niagara_system(name="NS_Trail", path="/Game/StressTest/FX/", template_system=<basic-sprite>)
#      VERIFY: get_niagara_system_info(NS_Trail)
#      get_niagara_system_schema() — capture schema, log if any property names look like typo magnets
#      get_niagara_system_topology(NS_Trail) — capture emitter list
#      add_niagara_user_variables(NS_Trail, [{"name":"Velocity","type":"vec3","default":"0,0,100"}])
#      VERIFY: get_niagara_user_variables(NS_Trail) contains Velocity
#      set_niagara_module_enabled(...) → toggle one module
#      VERIFY: get_niagara_module_topology shows enabled:false
#      get_niagara_system_compile_state(NS_Trail) — assert status, log per-script events
#      get_niagara_stack_issues(NS_Trail) — log any issues observed
#      dump_niagara_system(NS_Trail) — full rosetta dump
#
# 1.14 Compose: spawn an actor that uses the skeletal mesh + physics asset + Niagara
#      spawn_actor(class="SkeletalMeshActor", location={0,0,100}, label="HeroCharacter")
#      set_uproperty_checked(HeroCharacter, "SkeletalMeshComponent.SkeletalMeshAsset", "/Game/StressTest/Characters/SKM_TestChar")
#      attach_niagara_to_actor(actor=HeroCharacter, system=NS_Trail, socket_name="hand_r")
#      VERIFY: dump_actor(HeroCharacter) shows mesh + niagara component

# ---------------------------------------------------------------------------
# PHASE 2 — HUD (UMG + MVVM + Enhanced Input)
# Target: ~30 MCP calls. Heavy on Batch W.
# ---------------------------------------------------------------------------
# 2.1  Create widget blueprint
#      create_umg_widget_blueprint("/Game/StressTest/UI/WBP_HUD", parent_class="UserWidget")
#
# 2.2  Polymorphic add_widget (Batch W) — root panel
#      add_widget(WBP_HUD, widget_class="/Script/UMG.CanvasPanel", widget_name="Root")
#      VERIFY: dump_widget_tree shows Root as root widget
#
# 2.3  Add child widgets via polymorphic add_widget (Batch W)
#      add_widget(WBP_HUD, widget_class="/Script/UMG.VerticalBox", widget_name="Stack", parent_widget_name="Root")
#      add_widget(WBP_HUD, widget_class="/Script/UMG.TextBlock", widget_name="HealthLabel", parent_widget_name="Stack")
#      add_widget(WBP_HUD, widget_class="/Script/UMG.ProgressBar", widget_name="HealthBar", parent_widget_name="Stack")
#      VERIFY: dump_widget_tree shows expected hierarchy
#
# 2.4  Type-specific add (ours-only) — compare polymorphic vs type-specific
#      add_text_block_to_widget(WBP_HUD, parent="Stack", name="DebugText", text="Stress Test")
#      EXPECTED FINDING: behavioral parity with add_widget(class=TextBlock)? Log differences.
#
# 2.5  Named-slot operations (Batch W)
#      Find a widget class with a named slot (e.g. CommonBorder if available, or skip)
#      set_named_slot_content(...) → if successful, verify with get_named_slots
#      EXPECTED FINDING: if no native UMG class has named slots, log ["umg-named-slots-need-test-asset"]
#
# 2.6  Tree mutation (Batch W)
#      move_widget(HealthLabel to a new parent VerticalBox)
#      VERIFY: dump_widget_tree
#      rename_widget(HealthLabel → "PlayerHealthLabel")
#      VERIFY: old name gone, new name present
#      remove_widget(DebugText)
#      VERIFY: gone from tree
#
# 2.7  Flag op (Batch W)
#      set_widget_as_variable(PlayerHealthLabel, true)
#      VERIFY: dump_widget_tree shows is_variable=true
#
# 2.8  MVVM (ours-only)
#      create_mvvm_view_model(...) — bind a property to PlayerHealthLabel's Text
#      VERIFY: dump_widget_tree shows binding
#
# 2.9  Compile widget blueprint (Batch W)
#      compile_widget_blueprint(WBP_HUD)
#      VERIFY: assert compiled=true, errors=[]
#
# 2.10 Enhanced Input
#      create_input_action("/Game/StressTest/Input/IA_TogglePause")
#      create_input_mapping_context("/Game/StressTest/Input/IMC_Default")
#      add_input_action_to_mapping_context(...)
#      VERIFY: dump on each

# ---------------------------------------------------------------------------
# PHASE 3 — GAS + GameplayTags + StateTree (Batches Y + Z)
# Target: ~25 MCP calls
# ---------------------------------------------------------------------------
# 3.1  GameplayTags (Batch Y)
#      add_gameplay_tag("StressTest.Damage.Physical")
#      add_gameplay_tag("StressTest.Effect.Burning")
#      add_cue_tag("GameplayCue.StressTest.Hit")
#      VERIFY: get_gameplay_tag_info on each, list_gameplay_cues parent="GameplayCue.StressTest"
#      find_referencers_by_tag — should return empty for newly added (good — proves the registry consults)
#
# 3.2  GameplayCue notify (Batch Y)
#      create_cue_notify_asset("GameplayCue.StressTest.Hit", path="/Game/StressTest/FX", name="GCN_Hit", is_actor=false)
#      VERIFY: find_cue_notify_assets parent="GameplayCue.StressTest" returns the new notify
#
# 3.3  GAS inspector (Batch Y) — requires PIE actor with ASC
#      Spawn an actor with ASC, start PIE (start_play_in_editor)
#      Wait ~3 sec, get_selected_actors → ensure actor selected
#      get_actor_attribute_values(actor_name=<spawned>) — may return error if no AttributeSet bound
#      Log result; this is a PIE-dependent surface — finding ["gas-pie-dependent"] applies
#      stop_play_in_editor
#
# 3.4  GAS AttributeSet discovery (Batch Y)
#      find_attribute_set_classes() — log count, expect 0 in a fresh project
#      list_attribute_set_attributes("UAbilitySystemTestAttributeSet") if any standard one available
#
# 3.5  GameFeatures (Batch Y)
#      list_game_features() — log returned list (may be empty)
#      OPTIONAL: create_game_feature_plugin (skip — pollutes filesystem)
#
# 3.6  StateTree (Batch Z)
#      find_assets(class_path="/Script/StateTreeModule.StateTree", limit=5)
#      If none found, log ["statetree-no-test-asset"] and skip rest of phase 3.6
#      Otherwise: get_state_tree_editor_data(<found>)
#      list_state_tree_root_states(<found>) — capture child counts
#      For each root: list_state_tree_state_children, verify count matches
#      For a state with tasks > 0: list_state_tree_state_tasks
#      describe_state_tree_node(node_kind="task", state_path=..., index=0)
#      list_state_tree_global_tasks, list_state_tree_evaluators
#
# 3.7  WorldConditions (Batch Z)
#      Likely no test asset with embedded query. Log ["worldconditions-skipped"] and move on.

# ---------------------------------------------------------------------------
# PHASE 4 — Stage / Virtual Production
# Target: ~25 MCP calls
# ---------------------------------------------------------------------------
# 4.1  World Partition introspection
#      get_world_partition_info(current_level)
#      list_world_partition_cells(...) — log count
#      dump_world_partition_grid(...)
#
# 4.2  World Partition mutation
#      Pick an actor to pin: pin_wp_actors([HeroCharacter])
#      VERIFY: subsequent introspection reflects the pin
#      force_load_wp_region(...) — pick a region with content
#
# 4.3  nDisplay (Batch L)
#      get_ndisplay_status() — log result
#      list_ndisplay_root_actors() — likely empty in this scene; log finding
#
# 4.4  DMX (Batch L+)
#      list_dmx_libraries() — log
#      create_dmx_library(path="/Game/StressTest/DMX/", name="DMX_Stage")
#      add_dmx_fixture(...) → if successful, dump_dmx_patch
#      set_dmx_universe(...)
#      send_dmx_values(...) — runtime-y, may need an active nDisplay or PIE
#
# 4.5  XR (Batch L+)
#      get_xr_runtime_info() — log
#      dump_xr_input_state() — may be empty without HMD; finding ["xr-no-hmd"]
#
# 4.6  Stage actors
#      spawn_light_card(location=..., size=..., color=...)
#      VERIFY: dump_actor(LightCard)
#
# 4.7  LiveLink (Batch L)
#      list_livelink_subjects() — likely empty
#      Apply a cinematic preset if any presets are reachable

# ---------------------------------------------------------------------------
# PHASE 5 — Lighting + Rendering
# Target: ~20 MCP calls
# ---------------------------------------------------------------------------
# 5.1  Sky atmosphere
#      spawn_actor(class="SkyAtmosphere") → name "Sky"
#      spawn_actor(class="DirectionalLight") → name "Sun"
#      set_sun_position(actor="Sun", time_of_day=15.0)
#      VERIFY: dump_actor("Sun") shows updated rotation
#
# 5.2  PostProcess volume
#      spawn_post_process_volume(unbound=true) → name "PPV"
#      set_post_process_setting(actor="PPV", property="AutoExposure.MinBrightness", value=0.5)
#      set_post_process_setting(actor="PPV", property="Bloom.Intensity", value=0.8)
#      VERIFY: dump_post_process_volume("PPV")
#
# 5.3  Color grading + LUT
#      set_color_grading(actor="PPV", saturation={x:1.1,y:1.0,z:1.0,w:1.0})
#      VERIFY: dump_post_process_volume shows the new tint
#
# 5.4  Path Tracer
#      enable_path_tracer()
#      configure_path_tracer(samples=64, max_bounces=4)
#      take_path_tracer_screenshot() — wait for completion; capture the inline base64 PNG
#      LOG: file size of the returned image; if too small (< 50KB), finding ["pathtracer-low-fidelity"]
#
# 5.5  Frame capture / Insights
#      start_insights_trace()
#      enable_stat_group("Engine")
#      capture_frame() — single-frame capture
#      Wait a few seconds, then stop_insights_trace
#      dump_insights_summary

# ---------------------------------------------------------------------------
# PHASE 6 — Sequencer / Cinematic / MetaSound
# Target: ~25 MCP calls
# ---------------------------------------------------------------------------
# 6.1  Sequencer
#      create_level_sequence(path="/Game/StressTest/Cinematics/LS_Stage")
#      add_actor_to_sequence(LS_Stage, HeroCharacter)
#      add_camera_to_sequence(LS_Stage, name="CineCam_01")
#      add_keyframe(LS_Stage, target="CineCam_01.Location", time=0.0, value={0,-500,200})
#      add_keyframe(LS_Stage, target="CineCam_01.Location", time=5.0, value={0,500,200})
#      VERIFY: dump_level_sequence(LS_Stage)
#
# 6.2  LiveLink cinematic preset
#      list_livelink_presets() — likely empty
#      build_livelink_preset(...) — if reachable
#
# 6.3  Movie Render Queue
#      create_movie_render_queue_job(sequence=LS_Stage, output_dir="/Game/StressTest/Renders/")
#      VERIFY: dump_movie_render_job
#      (Don't actually render — too slow for stress test)
#
# 6.4  Movie Render Graph (5.8 only)
#      ENGINE-CHECK: if engine == "5.8", proceed; else skip with finding ["mrg-5.7-not-supported"]
#      create_movie_render_graph(name="MRG_Stress", path="/Game/StressTest/MRG/")
#      add_movie_render_graph_node(...)
#      VERIFY: dump_movie_render_graph(MRG_Stress)
#
# 6.5  MetaSound
#      Find a MetaSound asset in engine content; if none, create one
#      create_metasound(path="/Game/StressTest/Audio/", name="MS_Hum")
#      add_metasound_node(...)
#      VERIFY: dump_metasound
#
# 6.6  SoundCue + Attenuation
#      create_sound_cue(path="/Game/StressTest/Audio/", name="SC_Ambient")
#      set_attenuation(SC_Ambient, falloff_distance=2000.0, ...)
#      VERIFY: dump_sound_cue

# ---------------------------------------------------------------------------
# PHASE 7 — Blueprint authoring (the BP graph editing surface)
# Target: ~35 MCP calls — heavy use of batch_edit + the new spellrot commands
# ---------------------------------------------------------------------------
# 7.1  Create character BP
#      create_blueprint(path="/Game/StressTest/Blueprints/", name="BP_StressChar",
#                       parent_class="Character")
#
# 7.2  Add components
#      add_blueprint_component(BP_StressChar, class="StaticMeshComponent", name="Visuals")
#      add_blueprint_component(BP_StressChar, class="AbilitySystemComponent", name="ASC")
#      VERIFY: dump_blueprint_components
#
# 7.3  Add variables
#      add_blueprint_variable(BP_StressChar, name="MaxHealth", type="float", default="100.0")
#      add_blueprint_variable(BP_StressChar, name="CurrentHealth", type="float", default="100.0")
#      add_blueprint_variable(BP_StressChar, name="DamageTags", type="GameplayTagContainer")
#
# 7.4  Build a function graph using batch_edit_blueprint_nodes
#      add_blueprint_function_graph(BP_StressChar, function_name="ApplyDamage")
#      batch_edit_blueprint_nodes(BP_StressChar, graph_name="ApplyDamage", nodes=[
#        {"temp_id":"entry","type":"function_entry"},
#        {"temp_id":"sub","type":"math","operation":"Subtract","input_pin_types":["float","float"]},
#        {"temp_id":"set","type":"variable_set","variable_name":"CurrentHealth"},
#        {"temp_id":"exit","type":"function_result"}
#      ], connections=[
#        {"from":"entry.then","to":"sub.exec"},
#        {"from":"entry.Damage","to":"sub.B"},  # this might surface a finding about pin name resolution
#        ...
#      ])
#      VERIFY: dump_blueprint_graph(BP_StressChar, "ApplyDamage")
#
# 7.5  New spellrot commands (just-merged)
#      set_blueprint_node_position(BP_StressChar, "ApplyDamage", node_id=<sub>, x=400, y=200)
#      VERIFY: dump position
#      add_blueprint_comment_node(BP_StressChar, "ApplyDamage",
#                                  text="Damage Calculation",
#                                  wrap_node_ids=[<sub>, <set>],
#                                  margin=20)
#      VERIFY: dump shows comment node + correct framing
#
# 7.6  auto_layout
#      auto_layout_blueprint_graph(BP_StressChar, "ApplyDamage")
#      VERIFY: dump shows non-overlapping positions
#
# 7.7  Cross-BP variable_class (the other-session feature)
#      Create BP_Target; add a variable on BP_Target
#      In BP_StressChar's graph, add a variable_get with variable_class="BP_Target_C"
#      VERIFY: dump shows the typed-self pin
#
# 7.8  Compile + dump
#      compile_blueprint(BP_StressChar)
#      VERIFY: assert success
#      dump_blueprint_graph (full, all graphs)

# ---------------------------------------------------------------------------
# PHASE 8 — DataTable / CurveTable / PCG / Refactoring
# Target: ~15 MCP calls
# ---------------------------------------------------------------------------
# 8.1  DataTable
#      create_data_table(...) → if API doesn't exist, log finding
#      add_data_table_row(...) for 3 damage entries
#      VERIFY: dump_data_table
#
# 8.2  CurveTable
#      create_curve_table(...) → if API doesn't exist, log finding ["curve-table-missing"]
#
# 8.3  PCG
#      Find a PCG graph in engine content (PCGGraph type)
#      If found: dump_pcg_graph, list_pcg_nodes, dump_pcg_settings
#      Otherwise: create_pcg_graph(...) → if API exists; else finding
#
# 8.4  Refactoring smoke
#      replace_asset_references(old=<picked-actor>, new=<other-actor>, dry_run=true)
#      VERIFY: dry_run output shows the references
#      bulk_rename_assets(pattern="^PM_", replacement="PhysMat_", dry_run=true)

# ---------------------------------------------------------------------------
# PHASE 9 — Editor UX + Source Control + Validation
# Target: ~15 MCP calls
# ---------------------------------------------------------------------------
# 9.1  CVar profile
#      save_cvar_profile(name="stress-test-profile", cvars={"r.Lumen.HardwareRayTracing":"1"})
#      load_cvar_profile("stress-test-profile")
#      list_cvar_profiles()
#
# 9.2  Viewport bookmark
#      save_viewport_bookmark(slot=1)
#      load_viewport_bookmark(slot=1)
#      list_viewport_bookmarks()
#
# 9.3  Workspace layout
#      save_workspace_layout(name="StressLayout")
#      list_workspace_layouts()
#
# 9.4  Source control (if P4 reachable; otherwise log finding)
#      check_out_asset(BP_StressChar)
#      validate_before_submit([BP_StressChar])
#      diff_asset_against_depot(BP_StressChar)
#
# 9.5  Asset validation
#      run_asset_validator(BP_StressChar)
#      list_validation_rules()
#      dump_validation_report

# ---------------------------------------------------------------------------
# PHASE 10 — Rosetta Stone full dump pass
# Target: ~20 MCP calls
# ---------------------------------------------------------------------------
# Run every dump_* command against the assets created in this run:
#   dump_blueprint_graph (full)
#   dump_level
#   dump_level_sequence
#   dump_niagara_system
#   dump_widget_tree
#   dump_material_graph (any material we created)
#   dump_physics_asset
#   dump_animation
#   dump_metasound
#   dump_data_table
#   dump_pcg_graph (if reachable)
#   dump_control_rig (if any present)
#   dump_landscape (n/a here, log if there's no landscape)
#
# For each dump: log the response size in bytes. Anything > 100KB triggers a
# finding suggesting continuation-token usage. Anything < 50 bytes suggests the
# dump is broken / missing data.

# ---------------------------------------------------------------------------
# PHASE 11 — help() spot-check (post-QoL batch)
# Target: 3 MCP calls
# ---------------------------------------------------------------------------
# 11.1 help() → assert categories array
# 11.2 help("Niagara") → assert commands array contains the V batch additions
# 11.3 help("PhysicsAsset") → assert commands array contains all 17 from X
# Findings: if a command we just successfully used isn't in help(), that's a fail

# ---------------------------------------------------------------------------
# PHASE 12 — Final state capture
# ---------------------------------------------------------------------------
# 12.1 Save the level (save_level)
# 12.2 Take a Path Tracer screenshot of the finished scene
#      (already done in 5.4, but a final composed shot here)
# 12.3 Capture a diagnostic bundle (capture_diagnostic_bundle)
# 12.4 Summarize the findings file: tally counts of ok/warn/fail per category
# 12.5 Write the summary as findings/<engine>-<date>-summary.md

# ---------------------------------------------------------------------------
# EXIT
# ---------------------------------------------------------------------------
# The agent stops here. Does NOT fix any of the findings. Does NOT commit
# the generated Content/ (gitignored anyway). The findings file is the
# deliverable; everything else is incidental scaffolding.
