bl_info = {
    "name": "Haven Area Importer",
    "author": "Haven Tools",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Import > Haven Area (.havenarea)",
    "description": "Import Dragon Age: Origins level areas exported from Haven Tools",
    "category": "Import-Export",
}

import bpy
import json
import os
from mathutils import Vector, Quaternion
from bpy_extras.io_utils import ImportHelper
from bpy.props import StringProperty, BoolProperty


class IMPORT_OT_havenarea(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.havenarea"
    bl_label = "Import Haven Area"
    bl_options = {'REGISTER', 'UNDO'}

    filename_ext = ".havenarea"
    filter_glob: StringProperty(default="*.havenarea", options={'HIDDEN'})

    import_terrain: BoolProperty(name="Import Terrain", default=True)
    import_props: BoolProperty(name="Import Props", default=True)
    import_trees: BoolProperty(name="Import Trees", default=True)

    def execute(self, context):
        return import_havenarea(context, self.filepath,
                                self.import_terrain,
                                self.import_props,
                                self.import_trees)


def setup_materials(objects):
    for obj in objects:
        if obj.type != 'MESH':
            continue
        for slot in obj.material_slots:
            mat = slot.material
            if mat is None:
                continue
            mat.use_backface_culling = False
            if not mat.use_nodes:
                mat.use_nodes = True

            principled = None
            for node in mat.node_tree.nodes:
                if node.type == 'BSDF_PRINCIPLED':
                    principled = node
                    break
            if principled is None:
                continue

            spec_input = principled.inputs.get('Specular IOR Level') or principled.inputs.get('Specular')
            if spec_input:
                spec_input.default_value = 0.0

            has_alpha_input = False
            if 'Alpha' in principled.inputs:
                for link in mat.node_tree.links:
                    if link.to_socket == principled.inputs['Alpha']:
                        has_alpha_input = True
                        break

            is_alpha_clip = False
            if hasattr(mat, 'blend_method'):
                is_alpha_clip = (mat.blend_method == 'CLIP')
            if has_alpha_input:
                is_alpha_clip = True

            if is_alpha_clip:
                try: mat.blend_method = 'CLIP'
                except: pass
                try: mat.alpha_threshold = 0.5
                except: pass
                try: mat.shadow_method = 'CLIP'
                except: pass
                try: mat.surface_render_method = 'DITHERED'
                except: pass


def build_terrain_shader(bl_mat, info, base_dir):
    pal_dim = info['palDim']
    pal_param = info['palParam']
    uv_scales = info['uvScales']
    total_cells = info.get('totalCells', int(pal_dim[2]) * int(pal_dim[3]))
    if total_cells < 1:
        total_cells = 1
    if total_cells > 8:
        total_cells = 8

    cols = int(pal_dim[2])
    rows = int(pal_dim[3])
    cell_w, cell_h = pal_dim[0], pal_dim[1]
    pad_x, pad_y = pal_param[0], pal_param[1]
    usable_w, usable_h = pal_param[2], pal_param[3]

    bl_mat.use_backface_culling = False
    bl_mat.use_nodes = True
    tree = bl_mat.node_tree
    nodes = tree.nodes
    links = tree.links
    nodes.clear()

    def add(node_type, x, y, label=''):
        n = nodes.new(node_type)
        n.location = (x, y)
        if label:
            n.label = label
        return n

    X_UV = -1800
    X_MASKS = -1400
    X_SEP = -1100
    X_TILE = -800
    X_PAL = -400
    X_BLEND = 0
    X_OUT = 400

    output_node = add('ShaderNodeOutputMaterial', X_OUT, 0)
    principled = add('ShaderNodeBsdfPrincipled', X_BLEND + 200, 0)
    spec = principled.inputs.get('Specular IOR Level') or principled.inputs.get('Specular')
    if spec:
        spec.default_value = 0.0
    principled.inputs['Metallic'].default_value = 0.0
    principled.inputs['Roughness'].default_value = 0.5
    links.new(principled.outputs['BSDF'], output_node.inputs['Surface'])

    uv_node = add('ShaderNodeTexCoord', X_UV, 0)
    sep_uv = add('ShaderNodeSeparateXYZ', X_UV + 200, 0, 'Separate UV')
    links.new(uv_node.outputs['UV'], sep_uv.inputs['Vector'])

    palette_path = os.path.join(base_dir, info['palette'])
    palette_img = bpy.data.images.load(palette_path)
    palette_img.colorspace_settings.name = 'sRGB'

    mask_a_path = os.path.join(base_dir, info['maskA'])
    mask_a_img = bpy.data.images.load(mask_a_path)
    mask_a_img.colorspace_settings.name = 'Non-Color'

    mask_a2_img = None
    if info.get('maskA2') and total_cells > 4:
        mask_a2_path = os.path.join(base_dir, info['maskA2'])
        mask_a2_img = bpy.data.images.load(mask_a2_path)
        mask_a2_img.colorspace_settings.name = 'Non-Color'

    mask_a_tex = add('ShaderNodeTexImage', X_MASKS, 300, 'MaskA')
    mask_a_tex.image = mask_a_img
    mask_a_tex.interpolation = 'Linear'
    links.new(uv_node.outputs['UV'], mask_a_tex.inputs['Vector'])

    sep_a = add('ShaderNodeSeparateColor', X_SEP, 300, 'Weights 0-2')
    links.new(mask_a_tex.outputs['Color'], sep_a.inputs['Color'])

    weight_outs = []
    weight_outs.append(sep_a.outputs['Red'])
    weight_outs.append(sep_a.outputs['Green'])
    weight_outs.append(sep_a.outputs['Blue'])
    weight_outs.append(mask_a_tex.outputs['Alpha'])

    if mask_a2_img and total_cells > 4:
        mask_a2_tex = add('ShaderNodeTexImage', X_MASKS, -200, 'MaskA2')
        mask_a2_tex.image = mask_a2_img
        mask_a2_tex.interpolation = 'Linear'
        links.new(uv_node.outputs['UV'], mask_a2_tex.inputs['Vector'])

        sep_a2 = add('ShaderNodeSeparateColor', X_SEP, -200, 'Weights 4-6')
        links.new(mask_a2_tex.outputs['Color'], sep_a2.inputs['Color'])

        weight_outs.append(sep_a2.outputs['Red'])
        weight_outs.append(sep_a2.outputs['Green'])
        weight_outs.append(sep_a2.outputs['Blue'])
        weight_outs.append(mask_a2_tex.outputs['Alpha'])

    cell_color_outs = []
    cell_y_start = 600

    for ci in range(total_cells):
        cy = cell_y_start - ci * 280
        col = ci // rows
        row = ci % rows
        origin_x = col * cell_w + pad_x
        origin_y = 1.0 - (row * cell_h + pad_y) - usable_h
        scale = uv_scales[ci] if ci < len(uv_scales) else 1.0

        mul_u = add('ShaderNodeMath', X_TILE - 300, cy, f'Cell{ci} U*s')
        mul_u.operation = 'MULTIPLY'
        mul_u.inputs[1].default_value = scale
        links.new(sep_uv.outputs['X'], mul_u.inputs[0])

        frac_u = add('ShaderNodeMath', X_TILE - 150, cy, f'Cell{ci} fracU')
        frac_u.operation = 'FRACT'
        links.new(mul_u.outputs[0], frac_u.inputs[0])

        pal_u = add('ShaderNodeMath', X_TILE, cy, f'Cell{ci} palU')
        pal_u.operation = 'MULTIPLY_ADD'
        pal_u.inputs[1].default_value = usable_w
        pal_u.inputs[2].default_value = origin_x
        links.new(frac_u.outputs[0], pal_u.inputs[0])

        mul_v = add('ShaderNodeMath', X_TILE - 300, cy - 140, f'Cell{ci} V*s')
        mul_v.operation = 'MULTIPLY'
        mul_v.inputs[1].default_value = scale
        links.new(sep_uv.outputs['Y'], mul_v.inputs[0])

        frac_v = add('ShaderNodeMath', X_TILE - 150, cy - 140, f'Cell{ci} fracV')
        frac_v.operation = 'FRACT'
        links.new(mul_v.outputs[0], frac_v.inputs[0])

        pal_v = add('ShaderNodeMath', X_TILE, cy - 140, f'Cell{ci} palV')
        pal_v.operation = 'MULTIPLY_ADD'
        pal_v.inputs[1].default_value = usable_h
        pal_v.inputs[2].default_value = origin_y
        links.new(frac_v.outputs[0], pal_v.inputs[0])

        combine = add('ShaderNodeCombineXYZ', X_PAL - 150, cy - 70, f'Cell{ci} UV')
        links.new(pal_u.outputs[0], combine.inputs['X'])
        links.new(pal_v.outputs[0], combine.inputs['Y'])

        pal_tex = add('ShaderNodeTexImage', X_PAL, cy - 70, f'Palette Cell{ci}')
        pal_tex.image = palette_img
        pal_tex.interpolation = 'Linear'
        pal_tex.extension = 'EXTEND'
        links.new(combine.outputs['Vector'], pal_tex.inputs['Vector'])

        cell_color_outs.append(pal_tex.outputs['Color'])


    if total_cells == 1:
        links.new(cell_color_outs[0], principled.inputs['Base Color'])
    else:
        bx = X_BLEND
        prev_accum = add('ShaderNodeVectorMath', bx, cell_y_start, 'Weighted 0')
        prev_accum.operation = 'SCALE'
        links.new(cell_color_outs[0], prev_accum.inputs[0])
        links.new(weight_outs[0], prev_accum.inputs['Scale'])

        prev_total = weight_outs[0]

        for ci in range(1, total_cells):
            cy = cell_y_start - ci * 200

            scaled = add('ShaderNodeVectorMath', bx, cy, f'Weighted {ci}')
            scaled.operation = 'SCALE'
            links.new(cell_color_outs[ci], scaled.inputs[0])
            if ci < len(weight_outs):
                links.new(weight_outs[ci], scaled.inputs['Scale'])

            accum = add('ShaderNodeVectorMath', bx + 150, cy + 50, f'Accum {ci}')
            accum.operation = 'ADD'
            links.new(prev_accum.outputs['Vector'], accum.inputs[0])
            links.new(scaled.outputs['Vector'], accum.inputs[1])
            prev_accum = accum

            if ci < len(weight_outs):
                total_add = add('ShaderNodeMath', bx - 150, cy - 60, f'TotalW +{ci}')
                total_add.operation = 'ADD'
                links.new(prev_total, total_add.inputs[0])
                links.new(weight_outs[ci], total_add.inputs[1])
                prev_total = total_add.outputs[0]

        inv_w = add('ShaderNodeMath', bx + 300, 0, 'Inv Total W')
        inv_w.operation = 'DIVIDE'
        inv_w.inputs[0].default_value = 1.0
        inv_w.use_clamp = True
        links.new(prev_total, inv_w.inputs[1])

        final_scale = add('ShaderNodeVectorMath', bx + 450, 0, 'Final Color')
        final_scale.operation = 'SCALE'
        links.new(prev_accum.outputs['Vector'], final_scale.inputs[0])
        links.new(inv_w.outputs[0], final_scale.inputs['Scale'])

        links.new(final_scale.outputs['Vector'], principled.inputs['Base Color'])


def import_model(filepath, collection):
    if not os.path.exists(filepath):
        return None
    ext = os.path.splitext(filepath)[1].lower()
    before = set(bpy.data.objects)
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=filepath)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=filepath)
    else:
        return None
    after = set(bpy.data.objects)
    new_objs = after - before
    if not new_objs:
        return None
    root = None
    for obj in new_objs:
        if obj.parent is None or obj.parent not in new_objs:
            root = obj
            break
    if root is None:
        root = list(new_objs)[0]
    for obj in new_objs:
        for col in list(obj.users_collection):
            col.objects.unlink(obj)
        collection.objects.link(obj)
    setup_materials(new_objs)
    return root


def deep_copy_object(source_obj, collection):
    if source_obj.data:
        new_obj = bpy.data.objects.new(source_obj.name, source_obj.data)
    else:
        new_obj = bpy.data.objects.new(source_obj.name, None)
    collection.objects.link(new_obj)
    new_obj.matrix_local = source_obj.matrix_local.copy()
    for child_src in source_obj.children:
        child = deep_copy_object(child_src, collection)
        child.parent = new_obj
        child.matrix_parent_inverse = child_src.matrix_parent_inverse.copy()
    return new_obj


def place_instance(template_obj, position, rotation, scale, collection):
    instance = deep_copy_object(template_obj, collection)
    instance.location = Vector(position)
    instance.rotation_mode = 'QUATERNION'
    game_quat = Quaternion((rotation[3], rotation[0], rotation[1], rotation[2]))
    template_quat = template_obj.matrix_local.to_quaternion()
    instance.rotation_quaternion = game_quat @ template_quat
    instance.scale = Vector((scale, scale, scale))
    return instance


def import_havenarea(context, filepath, do_terrain, do_props, do_trees):
    with open(filepath, 'r') as f:
        data = json.load(f)

    base_dir = os.path.dirname(filepath)
    level_name = data.get("level", "Unknown Level")

    master_collection = bpy.data.collections.new(level_name)
    context.scene.collection.children.link(master_collection)

    terrain_mat_lookup = {}

    if do_terrain and "terrain" in data:
        terrain_col = bpy.data.collections.new("Terrain")
        master_collection.children.link(terrain_col)

        terrain_mats = data["terrain"].get("materials", [])
        for info in terrain_mats:
            name = info.get("name", "")
            if name:
                terrain_mat_lookup[name.lower()] = info

        patches = data["terrain"].get("patches", {})
        templates_col = bpy.data.collections.new("_TerrainTemplates")
        terrain_col.children.link(templates_col)

        for patch_name, patch_data in patches.items():
            model_file = os.path.join(base_dir, patch_data["file"])
            template = import_model(model_file, templates_col)
            if template is None:
                continue
            template.hide_set(True)
            template.hide_render = True
            for child in template.children_recursive:
                child.hide_set(True)
                child.hide_render = True

            for inst in patch_data.get("instances", []):
                place_instance(template, inst["position"], inst["rotation"],
                               inst.get("scale", 1.0), terrain_col)

    if do_props and "props" in data:
        props_col = bpy.data.collections.new("Props")
        master_collection.children.link(props_col)
        templates_col = bpy.data.collections.new("_PropTemplates")
        props_col.children.link(templates_col)

        for model_name, prop_data in data["props"].items():
            model_file = os.path.join(base_dir, prop_data["file"])
            template = import_model(model_file, templates_col)
            if template is None:
                continue
            template.hide_set(True)
            template.hide_render = True
            for child in template.children_recursive:
                child.hide_set(True)
                child.hide_render = True

            for inst in prop_data.get("instances", []):
                place_instance(template, inst["position"], inst["rotation"],
                               inst.get("scale", 1.0), props_col)

    if do_trees and "trees" in data:
        trees_col = bpy.data.collections.new("Trees")
        master_collection.children.link(trees_col)
        tree_templates_col = bpy.data.collections.new("_TreeTemplates")
        trees_col.children.link(tree_templates_col)

        for tree_name, tree_data in data["trees"].items():
            model_file = os.path.join(base_dir, tree_data["file"])
            template = import_model(model_file, tree_templates_col)
            if template is None:
                continue
            template.hide_set(True)
            template.hide_render = True
            for child in template.children_recursive:
                child.hide_set(True)
                child.hide_render = True

            for inst in tree_data.get("instances", []):
                place_instance(template, inst["position"], inst["rotation"],
                               inst.get("scale", 1.0), trees_col)

    if terrain_mat_lookup:
        matched = 0
        for bl_mat in bpy.data.materials:
            key = bl_mat.name.lower()
            info = terrain_mat_lookup.get(key)
            if not info:
                dot = key.rfind('.')
                if dot > 0:
                    info = terrain_mat_lookup.get(key[:dot])
            if info:
                try:
                    build_terrain_shader(bl_mat, info, base_dir)
                    matched += 1
                except Exception as e:
                    print(f"[HavenArea] Failed to build terrain shader for '{bl_mat.name}': {e}")
    context.view_layer.update()
    return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator(IMPORT_OT_havenarea.bl_idname, text="Haven Area (.havenarea)")


def register():
    bpy.utils.register_class(IMPORT_OT_havenarea)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(IMPORT_OT_havenarea)


if __name__ == "__main__":
    register()