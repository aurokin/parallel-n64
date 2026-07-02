# /// script
# requires-python = ">=3.11"
# dependencies = ["mcp"]
# ///
"""Stdio MCP shim exposing the haste vision-tools service to coding agents.

Agents that lack image input (droid / opencode Go-tier models) get two tools:
  describe_image(image_path, question?) -> grounded scene description
  locate_on_image(image_path, query)    -> labeled boxes in image pixel coords

The shim runs on the GAMEPLAY host, reads the local screenshot, and uploads it
to the vision-tools HTTP service (default http://haste.home.arpa:8022), which
fronts qwen3.6-35b (describe) and NVIDIA LocateAnything-3B (locate). Agents
pass small local paths; the shim does the network hop — this is why it is a
local stdio server rather than a remote streamable-HTTP MCP endpoint.

Wire-up (project-level config, scoped to the eval workspace):

droid `.factory/mcp.json`:
  {"mcpServers": {"vision-tools": {"type": "stdio",
    "command": "uv", "args": ["run", "<path>/vision_tools_mcp.py"]}}}

opencode `opencode.json`:
  {"mcp": {"vision-tools": {"type": "local",
    "command": ["uv", "run", "<path>/vision_tools_mcp.py"]}}}

Env: VISION_TOOLS_URL overrides the service base URL.
"""
import base64, json, os, urllib.request

from mcp.server.fastmcp import FastMCP

BASE_URL = os.environ.get("VISION_TOOLS_URL", "http://haste.home.arpa:8022")

mcp = FastMCP("vision-tools")


def _post(endpoint: str, payload: dict, timeout: int) -> dict:
    req = urllib.request.Request(
        f"{BASE_URL}{endpoint}",
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def _image_b64(image_path: str) -> str:
    path = os.path.expanduser(image_path)
    if not os.path.exists(path):
        raise FileNotFoundError(f"no such image: {path}")
    return base64.b64encode(open(path, "rb").read()).decode()


@mcp.tool()
def describe_image(image_path: str, question: str = "") -> str:
    """Describe a local screenshot: game state, visible characters/objects with
    rough positions, and any on-screen text. Optionally ask a specific question
    about the image."""
    out = _post("/describe", {"image_b64": _image_b64(image_path),
                              "question": question}, timeout=240)
    return out.get("description") or json.dumps(out)


@mcp.tool()
def locate_on_image(image_path: str, query: str) -> str:
    """Find named things in a local screenshot. query is a short description
    (e.g. "Mario", "the red door", "the START button"); multiple targets can be
    separated with " and ". Returns labeled bounding boxes [x1, y1, x2, y2] in
    pixel coordinates of the original image."""
    out = _post("/locate", {"image_b64": _image_b64(image_path),
                            "query": query.replace(" and ", "</c>")}, timeout=660)
    return json.dumps(out)


if __name__ == "__main__":
    mcp.run()
