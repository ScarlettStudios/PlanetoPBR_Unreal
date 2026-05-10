import json
import os
import socket
import tempfile
import time
import urllib.error
import urllib.request
import uuid


SPACE_BASE = "https://ascarlettvfx-testpbr2026.hf.space"
IMAGE_PATH = r"F:\test_image.png"
REQUEST_TIMEOUT = 180


def log(message):
    print(f"PLANETOPBR_VERIFY: {message}")


def resolve_fn_index(api_name="predict"):
    with urllib.request.urlopen(f"{SPACE_BASE}/config", timeout=REQUEST_TIMEOUT) as resp:
        config = json.loads(resp.read().decode("utf-8"))

    dependencies = config.get("dependencies") or []
    log(f"dependencies_count={len(dependencies)}")
    for i, dep in enumerate(dependencies):
        log(f"dependency[{i}].api_name={dep.get('api_name')}")
        if dep.get("api_name") == api_name:
            return i

    raise RuntimeError(f"api_name '{api_name}' not found in Space config")


def upload_file(image_path, raw_bytes):
    boundary = "----Boundary" + uuid.uuid4().hex
    filename = os.path.basename(image_path)
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="files"; filename="{filename}"\r\n'
        f"Content-Type: image/png\r\n\r\n"
    ).encode("utf-8") + raw_bytes + f"\r\n--{boundary}--\r\n".encode("utf-8")

    req = urllib.request.Request(
        f"{SPACE_BASE}/gradio_api/upload",
        data=body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
        upload_data = json.loads(resp.read().decode("utf-8"))
    return upload_data[0]


def join_queue(payload):
    req = urllib.request.Request(
        f"{SPACE_BASE}/gradio_api/queue/join",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
        join_data = json.loads(resp.read().decode("utf-8"))
    event_id = join_data.get("event_id")
    if not event_id:
        raise RuntimeError(f"Invalid queue response: {join_data}")
    return event_id


def poll_queue(session_hash):
    poll_url = f"{SPACE_BASE}/gradio_api/queue/data?session_hash={session_hash}"
    with urllib.request.urlopen(poll_url, timeout=REQUEST_TIMEOUT) as resp:
        for raw_line in resp:
            line = raw_line.decode("utf-8").strip()
            if not line.startswith("data:"):
                continue
            json_str = line.replace("data:", "", 1).strip()
            if not json_str:
                continue
            event = json.loads(json_str)
            msg = event.get("msg")
            log(f"queue_msg={msg}")
            if msg == "process_completed":
                output = event.get("output")
                if not isinstance(output, dict) or not output.get("data"):
                    raise RuntimeError(f"Unexpected process_completed output: {event}")
                return output["data"]
            if msg == "process_failed":
                raise RuntimeError(f"Space failed: {event}")
    raise RuntimeError("No output received from Space")


def download_and_check(url, index):
    req = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
        data = resp.read()

    suffix = ".png" if url.lower().split("?", 1)[0].endswith(".png") else ".bin"
    path = os.path.join(tempfile.gettempdir(), f"planetopbr_verify_{index}{suffix}")
    with open(path, "wb") as f:
        f.write(data)

    png_signature = b"\x89PNG\r\n\x1a\n"
    is_png = data.startswith(png_signature)
    log(f"download[{index}].url={url}")
    log(f"download[{index}].bytes={len(data)}")
    log(f"download[{index}].is_png={is_png}")
    log(f"download[{index}].path={path}")
    return is_png


def main():
    log("starting")
    if not os.path.exists(IMAGE_PATH):
        raise RuntimeError(f"Missing input image: {IMAGE_PATH}")

    fn_index = resolve_fn_index("predict")
    log(f"fn_index={fn_index}")

    with open(IMAGE_PATH, "rb") as f:
        raw_bytes = f.read()

    uploaded_path = upload_file(IMAGE_PATH, raw_bytes)
    log(f"uploaded_path={uploaded_path}")

    session_hash = uuid.uuid4().hex
    payload = {
        "data": [
            {
                "path": uploaded_path,
                "orig_name": os.path.basename(IMAGE_PATH),
                "size": len(raw_bytes),
                "mime_type": "image/png",
            },
            "windows",
        ],
        "event_data": None,
        "fn_index": fn_index,
        "session_hash": session_hash,
    }
    event_id = join_queue(payload)
    log(f"event_id={event_id}")
    output = poll_queue(session_hash)
    log(f"output_len={len(output)}")

    all_png = True
    for i, item in enumerate(output):
        if not isinstance(item, dict) or not item.get("url"):
            raise RuntimeError(f"output[{i}] missing url: {item}")
        all_png = download_and_check(item["url"], i) and all_png

    log(f"all_outputs_png={all_png}")
    if not all_png:
        raise RuntimeError("One or more outputs were not PNG files")


try:
    main()
except Exception as exc:
    log(f"FAILED: {exc}")
    raise
