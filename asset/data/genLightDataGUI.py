#!/usr/bin/env python3
"""
Interactive HDR light extraction GUI tool
- LassoSelector for free-form light region selection
- Auto-scans all numbered .hdr files in textures/
- Computes direction, brightness, area (pixel-level solid angle), baseBrightness
- Outputs LightInfo.json with hdr_image_max_num
"""

import os
os.environ.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)

import re
import numpy as np
import cv2
import json
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.widgets import LassoSelector
from matplotlib.path import Path
from matplotlib.patches import PathPatch


def equi_rect_uv_to_direction(hdr_uv) -> np.ndarray:
    hdr_uv = np.asarray(hdr_uv, dtype=np.float32)
    if hdr_uv.ndim == 1:
        hdr_uv = hdr_uv[np.newaxis, :]
    u = hdr_uv[:, 0]
    v = hdr_uv[:, 1]

    PI = np.pi
    TWO_PI = 2 * PI

    phi = -(u - 0.5) * TWO_PI
    theta = v * PI

    sin_theta = np.sin(theta)
    cos_theta = np.cos(theta)
    cos_phi = np.cos(phi)
    sin_phi = np.sin(phi)

    x = sin_theta * sin_phi
    y = sin_theta * cos_phi
    z = cos_theta

    direction = np.stack([x, y, z], axis=-1)
    return direction.squeeze()


def tonemap_aces(hdr, exposure=1.0):
    x = hdr * exposure
    a = 2.51
    b = 0.03
    c = 2.43
    d = 0.59
    e = 0.14
    mapped = (x * (a * x + b)) / (x * (c * x + d) + e)
    return np.clip(mapped, 0, 1)


def pixels_in_lasso(verts, h, w):
    """Return a bool mask (h, w) of pixels inside the lasso polygon."""
    path = Path(verts)
    # Build pixel coordinate grid
    yy, xx = np.mgrid[0:h, 0:w]
    points = np.column_stack([xx.ravel(), yy.ravel()])
    mask = path.contains_points(points).reshape(h, w)
    return mask


def compute_pixel_solid_angles(h, w):
    """Precompute per-pixel solid angle for equirectangular projection.
    dOmega = sin(theta) * dtheta * dphi
    """
    dtheta = np.pi / h
    dphi = 2.0 * np.pi / w
    v_coords = (np.arange(h) + 0.5) / h  # pixel centers
    theta = v_coords * np.pi
    sin_theta = np.sin(theta)
    # Each pixel's solid angle
    pixel_omega = sin_theta * dtheta * dphi  # shape (h,)
    return pixel_omega  # broadcast over w when needed


def extract_light_from_mask(hdr_data, mask, pixel_omega, light_id, brightness_threshold=0.1):
    """Extract light info from a free-form mask region."""
    h, w = hdr_data.shape[:2]

    # Get pixels inside mask
    region_pixels = hdr_data[mask]  # (N, 3)
    if len(region_pixels) == 0:
        return None

    brightness = np.mean(region_pixels, axis=-1)
    valid = brightness >= brightness_threshold
    valid_pixels = region_pixels[valid]
    if len(valid_pixels) == 0:
        return None

    # Average brightness of valid pixels
    avg_brightness = float(np.mean(np.mean(valid_pixels, axis=-1)))

    # Direction: solid-angle-weighted centroid of mask pixels
    ys, xs = np.where(mask)
    us = (xs + 0.5) / w  # u in [0, 1]
    vs = (ys + 0.5) / h  # v in [0, 1]
    uvs = np.column_stack([us, vs])
    dirs = equi_rect_uv_to_direction(uvs)  # (N, 3)

    # Weight by solid angle * luminance
    lum = 0.2126 * hdr_data[ys, xs, 0] + 0.7152 * hdr_data[ys, xs, 1] + 0.0722 * hdr_data[ys, xs, 2]
    weights = pixel_omega[ys] * lum
    weights_sum = np.sum(weights)
    if weights_sum > 0:
        weighted_dir = np.sum(dirs * weights[:, np.newaxis], axis=0) / weights_sum
    else:
        weighted_dir = np.mean(dirs, axis=0)
    # Normalize
    norm = np.linalg.norm(weighted_dir)
    if norm > 0:
        weighted_dir /= norm

    # Area: sum of solid angles of all mask pixels
    sphere_area = float(np.sum(pixel_omega[ys]))

    return {
        "light_id": light_id,
        "direction": [round(float(x), 6) for x in weighted_dir],
        "brightness": round(avg_brightness, 6),
        "area": round(sphere_area, 6)
    }


def compute_base_brightness(hdr_data, light_masks):
    """Compute weighted average luminance of non-light pixels."""
    h, w = hdr_data.shape[:2]

    # Combined light mask
    combined_mask = np.zeros((h, w), dtype=bool)
    for m in light_masks:
        combined_mask |= m

    non_light_mask = ~combined_mask

    luminance = 0.2126 * hdr_data[:, :, 0] + 0.7152 * hdr_data[:, :, 1] + 0.0722 * hdr_data[:, :, 2]

    v_coords = (np.arange(h) + 0.5) / h
    theta = v_coords * np.pi
    weights = np.sin(theta)
    weight_2d = np.broadcast_to(weights[:, np.newaxis], (h, w))

    weighted_lum = luminance * weight_2d
    total_weight = np.sum(weight_2d[non_light_mask])
    if total_weight > 0:
        base_brightness = np.sum(weighted_lum[non_light_mask]) / total_weight
    else:
        base_brightness = 0.0

    return float(base_brightness)


class HDRLightSelector:
    """Interactive HDR light selector with free-form lasso."""

    def __init__(self, hdr_path, hdr_index):
        self.hdr_path = hdr_path
        self.hdr_index = hdr_index
        self.masks = []       # list of bool masks
        self.patches = []     # display patches
        self.texts = []       # display text labels
        self.current_verts = None
        self.finished = False

        self.hdr_data = cv2.imread(hdr_path, cv2.IMREAD_ANYDEPTH | cv2.IMREAD_COLOR)
        if self.hdr_data is None:
            raise FileNotFoundError(f"Cannot read HDR: {hdr_path}")
        self.hdr_data = cv2.cvtColor(self.hdr_data, cv2.COLOR_BGR2RGB)
        self.h, self.w = self.hdr_data.shape[:2]

        self.display_img = tonemap_aces(self.hdr_data.astype(np.float32), exposure=1.0)

    def run(self):
        self.fig, self.ax = plt.subplots(1, 1, figsize=(14, 7))
        self.ax.imshow(self.display_img)
        self.ax.set_title(
            f"HDR {self.hdr_index}: Draw lasso around light | "
            f"Enter=Confirm | Backspace=Undo | Close=Skip"
        )

        self.lasso = LassoSelector(self.ax, self.on_lasso_select)

        self.fig.canvas.mpl_connect('key_press_event', self.on_key)
        plt.tight_layout()
        plt.show()

        return self.masks

    def on_lasso_select(self, verts):
        """Lasso selection callback - verts is list of (x, y) tuples."""
        if len(verts) < 3:
            return

        # Compute mask
        mask = pixels_in_lasso(verts, self.h, self.w)
        pixel_count = np.sum(mask)
        if pixel_count < 4:
            print(f"  Region too small ({pixel_count} pixels), ignored")
            return

        self.masks.append(mask)

        # Draw the lasso outline
        codes = [Path.MOVETO] + [Path.LINETO] * (len(verts) - 2) + [Path.CLOSEPOLY]
        verts_closed = list(verts) + [verts[0]]  # close the path
        if len(codes) != len(verts_closed):
            codes = [Path.MOVETO] + [Path.LINETO] * (len(verts_closed) - 2) + [Path.CLOSEPOLY]
        path = Path(verts_closed, codes)
        patch = PathPatch(path, linewidth=2, edgecolor='red', facecolor='red', alpha=0.15)
        self.ax.add_patch(patch)
        self.patches.append(patch)

        # Label at centroid
        cx = np.mean([v[0] for v in verts])
        cy = np.mean([v[1] for v in verts])
        txt = self.ax.text(cx, cy, f"L{len(self.masks) - 1}",
                           color='yellow', fontsize=12, fontweight='bold',
                           ha='center', va='center')
        self.texts.append(txt)

        self.fig.canvas.draw_idle()
        print(f"  Light {len(self.masks) - 1}: {pixel_count} pixels selected")

    def on_key(self, event):
        if event.key == 'enter':
            print(f"  HDR {self.hdr_index}: confirmed {len(self.masks)} light region(s)")
            self.finished = True
            plt.close(self.fig)
        elif event.key in ('delete', 'backspace'):
            if self.masks:
                self.masks.pop()
                if self.patches:
                    self.patches.pop().remove()
                if self.texts:
                    self.texts.pop().remove()
                self.fig.canvas.draw_idle()
                print(f"  Undone, {len(self.masks)} region(s) remaining")


def scan_hdr_files(textures_dir):
    """Scan for all numbered .hdr files, return sorted list of (index, path)."""
    result = []
    for fname in os.listdir(textures_dir):
        m = re.match(r'^(\d+)\.hdr$', fname)
        if m:
            idx = int(m.group(1))
            if idx >= 1:  # skip 0.hdr if it exists
                result.append((idx, os.path.join(textures_dir, fname)))
    result.sort(key=lambda x: x[0])
    return result


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    textures_dir = os.path.join(script_dir, "textures")
    output_file = os.path.join(script_dir, "json", "LightInfo.json")

    hdr_files = scan_hdr_files(textures_dir)
    if not hdr_files:
        print("No numbered .hdr files found in", textures_dir)
        return

    print(f"Found {len(hdr_files)} HDR files: {[f'{i}.hdr' for i, _ in hdr_files]}")

    all_hdr_lights = {}
    brightness_threshold = 0.5

    for hdr_index, hdr_path in hdr_files:
        print(f"\n========== HDR {hdr_index} ==========")
        try:
            selector = HDRLightSelector(hdr_path, hdr_index)
            masks = selector.run()

            if not masks:
                print(f"  HDR {hdr_index}: no light selected, skipped")
                continue

            pixel_omega = compute_pixel_solid_angles(selector.h, selector.w)

            lights = []
            for idx, mask in enumerate(masks):
                light = extract_light_from_mask(
                    selector.hdr_data, mask, pixel_omega, idx, brightness_threshold
                )
                if light:
                    lights.append(light)
                    print(f"  Light {idx}: direction={light['direction']}, "
                          f"brightness={light['brightness']:.4f}, area={light['area']:.6f}")

            base_brightness = compute_base_brightness(selector.hdr_data, masks)
            print(f"  baseBrightness = {base_brightness:.6f}")

            all_hdr_lights[str(hdr_index)] = {
                "light_count": len(lights),
                "lights": lights,
                "baseBrightness": round(base_brightness, 6)
            }

        except Exception as e:
            print(f"Error processing HDR {hdr_index}: {e}")
            import traceback
            traceback.print_exc()

    if all_hdr_lights:
        # Add hdr_image_max_num = max index among processed HDRs
        max_index = max(int(k) for k in all_hdr_lights.keys())
        output = {
            "hdr_image_max_num": max_index,
        }
        output.update(all_hdr_lights)

        with open(output_file, "w", encoding="utf-8") as f:
            json.dump(output, f, indent=2, ensure_ascii=False)
        print(f"\nSaved to: {output_file}")
        print(f"Processed {len(all_hdr_lights)} HDR files, hdr_image_max_num = {max_index}")
    else:
        print("\nNo HDR files processed")


if __name__ == "__main__":
    main()
