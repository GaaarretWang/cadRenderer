import numpy as np
import cv2
import json
from typing import List, Tuple

def equi_rect_uv_to_direction(hdr_uv) -> np.ndarray:
    hdr_uv = np.asarray(hdr_uv, dtype=np.float32)
    if hdr_uv.ndim == 1:
        hdr_uv = hdr_uv[np.newaxis, :]
    u = hdr_uv[:, 0]
    v = hdr_uv[:, 1]
    
    PI = np.pi
    TWO_PI = 2 * PI
    phi = (u - 0.5) * TWO_PI
    theta = v * PI
    
    sin_theta = np.sin(theta)
    cos_theta = np.cos(theta)
    cos_phi = np.cos(phi)
    sin_phi = np.sin(phi)
    
    x = sin_theta * cos_phi
    y = cos_theta
    z = sin_theta * sin_phi
    
    direction = np.stack([x, y, z], axis=-1)
    return direction.squeeze()

def extract_lights(hdr_path: str,
                   pixel_regions: List[Tuple[Tuple[int, int], Tuple[int, int]]],
                   hdr_index: int,
                   brightness_threshold: float = 0.1) -> None:
    hdr_data = cv2.imread(hdr_path, cv2.IMREAD_ANYDEPTH | cv2.IMREAD_COLOR)
    if hdr_data is None:
        raise FileNotFoundError(f"无法读取HDR文件：{hdr_path}")
    hdr_data = cv2.cvtColor(hdr_data, cv2.COLOR_BGR2RGB)
    h, w = hdr_data.shape[:2]
    
    lights = []
    for idx, (p1, p2) in enumerate(pixel_regions):
        x1, y1 = p1
        x2, y2 = p2
        x_min = min(x1, x2)
        x_max = max(x1, x2)
        y_min = min(y1, y2)
        y_max = max(y1, y2)
        
        if x_min < 0 or x_max >= w or y_min < 0 or y_max >= h:
            continue
        
        u_min = x_min / w
        u_max = x_max / w
        v_min = y_min / h
        v_max = y_max / h
        center_uv = ((u_min + u_max) / 2, (v_min + v_max) / 2)
        
        region_pixels = hdr_data[y_min:y_max+1, x_min:x_max+1]
        brightness = np.mean(region_pixels, axis=-1)
        valid_mask = brightness >= brightness_threshold
        valid_pixels = region_pixels[valid_mask]
        
        if len(valid_pixels) == 0:
            continue
        
        world_direction = equi_rect_uv_to_direction(center_uv).tolist()
        avg_brightness = float(np.mean(np.mean(valid_pixels, axis=-1)))
        
        # 光源面积：球面积分计算（非UV大小相乘）
        theta_min = v_min * np.pi
        theta_max = v_max * np.pi
        phi_min = (u_min - 0.5) * 2 * np.pi
        phi_max = (u_max - 0.5) * 2 * np.pi
        sphere_area = (np.cos(theta_min) - np.cos(theta_max)) * (phi_max - phi_min)
        
        lights.append({
            "light_id": idx,
            "direction": [round(x, 6) for x in world_direction],
            "brightness": round(avg_brightness, 6),
            "area": round(sphere_area, 6)
        })
    
    if not lights:
        raise ValueError("未提取到有效光源")
    
    hdr_light_info = {
        f"{hdr_index}": {
            "light_count": len(lights),
            "lights": lights
        }
    }
    
    print(json.dumps(hdr_light_info, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    HDR_INDEX = 4
    HDR_PATH = "textures/" + str(HDR_INDEX) + ".hdr"
    # PIXEL_REGIONS = [ # 1
    #     ((759, 115), (792, 146)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    #     ((561, 125), (617, 155)),  # 第2组像素区域（可选，取消注释启用）
    #     ((80, 135), (133, 154))   # 第3组像素区域（可选，取消注释启用）
    # ]

    # PIXEL_REGIONS = [ # 2
    #     ((469, 217), (591, 253)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    #     ((299, 24), (425, 74)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    #     ((11, 149), (46, 163)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    # ]

    # PIXEL_REGIONS = [ # 3
    #     ((477, 217), (591, 253)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    # ]
    PIXEL_REGIONS = [ # 4
        ((380, 160), (413, 184)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
        ((566, 148), (594, 171)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
        ((747, 230), (772, 244)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    ]
    # PIXEL_REGIONS = [ # 5
    #     ((533, 223), (556, 238)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    #     ((739, 239), (765, 249)),  # 第1组像素区域（你原本的像素坐标，直接粘贴）
    # ]

    BRIGHTNESS_THRESHOLD = 0.02
    extract_lights(HDR_PATH, PIXEL_REGIONS, HDR_INDEX, BRIGHTNESS_THRESHOLD)