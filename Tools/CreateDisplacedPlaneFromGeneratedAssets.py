import os
import re
import struct
import zlib

import unreal


ASSET_ROOT = "/Game/PlaneToPBR/Generated"
TEXTURE_SET_DIR = os.path.join(unreal.Paths.project_saved_dir(), "PlaneToPBR_textures")
MESH_OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "PlaneToPBR_meshes")
SUBDIVISIONS_X = 96
DISPLACEMENT_STRENGTH_CM = 25.0
PLANE_WIDTH_CM = 200.0
HEIGHT_CENTER = 0.5

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def log(message):
    print(f"PLANETOPBR_DISPLACE: {message}")


def sanitize_asset_name(value):
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_")
    return sanitized or "Generated"


def asset_exists(asset_path):
    return unreal.EditorAssetLibrary.does_asset_exist(asset_path)


def list_generated_run_dirs(asset_root=ASSET_ROOT):
    if not unreal.EditorAssetLibrary.does_directory_exist(asset_root):
        raise RuntimeError(f"Generated asset root does not exist: {asset_root}")

    child_dirs = unreal.EditorAssetLibrary.list_assets(asset_root, recursive=False, include_folder=True)
    run_dirs = []
    for child in child_dirs:
        if unreal.EditorAssetLibrary.does_directory_exist(child):
            run_dirs.append(child.rstrip("/"))

    if not run_dirs:
        raise RuntimeError(f"No generated PlaneToPBR run folders found under: {asset_root}")
    return run_dirs


def get_latest_generated_run_dir(asset_root=ASSET_ROOT):
    run_dirs = list_generated_run_dirs(asset_root)
    return sorted(run_dirs)[-1]


def get_run_name(run_dir):
    return run_dir.rstrip("/").split("/")[-1]


def find_generated_assets(run_dir):
    run_name = get_run_name(run_dir)
    material_path = f"{run_dir}/M_{run_name}_PlaneToPBR"
    depth_path = f"{run_dir}/T_{run_name}_Depth"

    if not asset_exists(material_path):
        raise RuntimeError(f"Generated material asset not found: {material_path}")
    if not asset_exists(depth_path):
        raise RuntimeError(f"Generated depth texture asset not found: {depth_path}")

    material = unreal.EditorAssetLibrary.load_asset(material_path)
    depth_texture = unreal.EditorAssetLibrary.load_asset(depth_path)
    if not material or not depth_texture:
        raise RuntimeError(f"Failed to load generated material or depth texture for: {run_dir}")

    return run_name, material, depth_texture


def find_depth_png(run_name, texture_dir=TEXTURE_SET_DIR):
    candidates = [
        os.path.join(texture_dir, f"depth_{run_name}.png"),
        os.path.join(texture_dir, f"Depth_{run_name}.png"),
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate

    pattern = re.compile(rf"^depth_{re.escape(run_name)}\.(png|jpg|jpeg|tga|exr)$", re.IGNORECASE)
    if os.path.isdir(texture_dir):
        for filename in os.listdir(texture_dir):
            if pattern.match(filename):
                return os.path.join(texture_dir, filename)

    raise RuntimeError(f"Depth source image not found for run '{run_name}' in: {texture_dir}")


def read_png_pixels(path):
    with open(path, "rb") as file:
        data = file.read()

    if not data.startswith(PNG_SIGNATURE):
        raise RuntimeError(f"Unsupported depth image format. Expected PNG: {path}")

    pos = len(PNG_SIGNATURE)
    width = height = color_type = bit_depth = None
    compressed = bytearray()

    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        chunk_type = data[pos + 4 : pos + 8]
        chunk_data = data[pos + 8 : pos + 8 + length]
        pos += 12 + length

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB",
                chunk_data,
            )
            if bit_depth != 8:
                raise RuntimeError(f"Only 8-bit PNG depth maps are supported: {path}")
            if compression != 0 or filter_method != 0 or interlace != 0:
                raise RuntimeError(f"Unsupported PNG encoding options in: {path}")
            if color_type not in (0, 2, 4, 6):
                raise RuntimeError(f"Unsupported PNG color type {color_type}: {path}")
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise RuntimeError(f"Invalid PNG missing IHDR: {path}")

    channels_by_color_type = {
        0: 1,
        2: 3,
        4: 2,
        6: 4,
    }
    channels = channels_by_color_type[color_type]
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))

    rows = []
    cursor = 0
    previous = [0] * stride
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        current = list(raw[cursor : cursor + stride])
        cursor += stride
        current = unfilter_scanline(current, previous, filter_type, channels)
        rows.append(current)
        previous = current

    return width, height, channels, rows


def unfilter_scanline(current, previous, filter_type, bytes_per_pixel):
    result = current[:]
    for i, value in enumerate(result):
        left = result[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
        up = previous[i] if previous else 0
        up_left = previous[i - bytes_per_pixel] if previous and i >= bytes_per_pixel else 0

        if filter_type == 0:
            recon = value
        elif filter_type == 1:
            recon = value + left
        elif filter_type == 2:
            recon = value + up
        elif filter_type == 3:
            recon = value + ((left + up) // 2)
        elif filter_type == 4:
            recon = value + paeth_predictor(left, up, up_left)
        else:
            raise RuntimeError(f"Unsupported PNG filter type: {filter_type}")

        result[i] = recon & 0xFF
    return result


def paeth_predictor(left, up, up_left):
    p = left + up - up_left
    pa = abs(p - left)
    pb = abs(p - up)
    pc = abs(p - up_left)
    if pa <= pb and pa <= pc:
        return left
    if pb <= pc:
        return up
    return up_left


def sample_height(width, height, channels, rows, u, v):
    x = min(width - 1, max(0, int(round(u * (width - 1)))))
    y = min(height - 1, max(0, int(round((1.0 - v) * (height - 1)))))
    row = rows[y]
    value = row[x * channels]
    return value / 255.0


def write_displaced_plane_obj(
    obj_path,
    depth_png_path,
    subdivisions_x=SUBDIVISIONS_X,
    displacement_strength_cm=DISPLACEMENT_STRENGTH_CM,
    plane_width_cm=PLANE_WIDTH_CM,
):
    width, height, channels, rows = read_png_pixels(depth_png_path)
    aspect_ratio = width / height
    subdivisions_y = max(1, round(subdivisions_x / aspect_ratio))
    plane_height_cm = plane_width_cm / aspect_ratio

    with open(obj_path, "w", encoding="utf-8", newline="\n") as file:
        file.write("# PlaneToPBR generated displaced plane\n")
        file.write("o PlaneToPBR_DisplacedPlane\n")

        for y_index in range(subdivisions_y + 1):
            v = y_index / subdivisions_y
            y_pos = (v - 0.5) * plane_height_cm
            for x_index in range(subdivisions_x + 1):
                u = x_index / subdivisions_x
                x_pos = (u - 0.5) * plane_width_cm
                height_value = sample_height(width, height, channels, rows, u, v)
                z_pos = (height_value - HEIGHT_CENTER) * displacement_strength_cm
                file.write(f"v {x_pos:.6f} {y_pos:.6f} {z_pos:.6f}\n")

        for y_index in range(subdivisions_y + 1):
            v = y_index / subdivisions_y
            for x_index in range(subdivisions_x + 1):
                u = x_index / subdivisions_x
                file.write(f"vt {u:.6f} {v:.6f}\n")

        row_width = subdivisions_x + 1
        for y_index in range(subdivisions_y):
            for x_index in range(subdivisions_x):
                a = y_index * row_width + x_index + 1
                b = a + 1
                c = a + row_width
                d = c + 1
                file.write(f"f {a}/{a} {b}/{b} {d}/{d}\n")
                file.write(f"f {a}/{a} {d}/{d} {c}/{c}\n")

    return subdivisions_y, width, height


def import_static_mesh(obj_path, destination_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = obj_path
    task.destination_path = destination_path
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.imported_object_paths)
    asset_path = imported_paths[0] if imported_paths else f"{destination_path}/{asset_name}"
    static_mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not static_mesh:
        raise RuntimeError(f"Failed to import static mesh asset: {asset_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(static_mesh)
    return static_mesh


def spawn_static_mesh_actor(static_mesh, material, actor_label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(static_mesh, unreal.Vector(0.0, 0.0, 0.0))
    if not actor:
        raise RuntimeError("Failed to spawn generated static mesh actor.")

    actor.set_actor_label(actor_label)
    component = actor.static_mesh_component
    component.set_material(0, material)
    unreal.EditorLevelLibrary.save_current_level()
    return actor


def create_displaced_plane_from_latest_generated_assets(
    asset_root=ASSET_ROOT,
    texture_dir=TEXTURE_SET_DIR,
    subdivisions_x=SUBDIVISIONS_X,
    displacement_strength_cm=DISPLACEMENT_STRENGTH_CM,
    plane_width_cm=PLANE_WIDTH_CM,
):
    run_dir = get_latest_generated_run_dir(asset_root)
    run_name, material, depth_texture = find_generated_assets(run_dir)
    depth_png_path = find_depth_png(run_name, texture_dir)

    os.makedirs(MESH_OUTPUT_DIR, exist_ok=True)
    obj_path = os.path.join(MESH_OUTPUT_DIR, f"SM_{run_name}_DisplacedPlane.obj")
    subdivisions_y, depth_width, depth_height = write_displaced_plane_obj(
        obj_path,
        depth_png_path,
        subdivisions_x,
        displacement_strength_cm,
        plane_width_cm,
    )

    mesh_asset_name = f"SM_{run_name}_DisplacedPlane"
    static_mesh = import_static_mesh(obj_path, run_dir, mesh_asset_name)
    actor = spawn_static_mesh_actor(static_mesh, material, f"PlaneToPBR_{run_name}_DisplacedPlane")

    log(f"run_name={run_name}")
    log(f"run_dir={run_dir}")
    log(f"depth_texture={depth_texture.get_path_name()}")
    log(f"depth_png={depth_png_path}")
    log(f"depth_size={depth_width}x{depth_height}")
    log(f"subdivisions={subdivisions_x}x{subdivisions_y}")
    log(f"displacement_strength_cm={displacement_strength_cm}")
    log(f"obj_path={obj_path}")
    log(f"static_mesh={static_mesh.get_path_name()}")
    log(f"actor={actor.get_actor_label()}")
    return static_mesh, actor


if __name__ == "__main__":
    create_displaced_plane_from_latest_generated_assets()
