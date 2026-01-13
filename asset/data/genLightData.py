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
    
    # ========== 关键修正1：反向推导GLSL的phi计算 ==========
    phi = -(u - 0.5) * TWO_PI
    
    # ========== 关键修正2：theta和GLSL的acos(v.z)对齐 ==========
    theta = v * PI
    
    # ========== 关键修正3：方向向量计算（对齐GLSL的atan(v.x, v.y)） ==========
    sin_theta = np.sin(theta)
    cos_theta = np.cos(theta)
    cos_phi = np.cos(phi)
    sin_phi = np.sin(phi)
    
    x = sin_theta * sin_phi
    y = sin_theta * cos_phi
    z = cos_theta            
    
    direction = np.stack([x, y, z], axis=-1)
    return direction.squeeze()

def extract_lights(hdr_path: str,
                   pixel_regions: List[Tuple[Tuple[int, int], Tuple[int, int]]],
                   hdr_index: int,
                   brightness_threshold: float = 0.1) -> dict:
    """
    提取单个HDR文件的光源信息
    返回格式: {
        "light_count": int,
        "lights": list[dict]
    }
    """
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
        
        # ========== 关键修正4：UV计算保持和GLSL一致（u=x/w, v=y/h） ==========
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
        
        # 使用修正后的函数计算世界方向
        world_direction = equi_rect_uv_to_direction(center_uv).tolist()
        avg_brightness = float(np.mean(np.mean(valid_pixels, axis=-1)))
        
        # ========== 光源面积计算也同步修正（对齐phi/theta的定义） ==========
        theta_min = v_min * np.pi
        theta_max = v_max * np.pi
        phi_min = -(u_min - 0.5) * 2 * np.pi
        phi_max = -(u_max - 0.5) * 2 * np.pi
        sphere_area = abs((np.cos(theta_min) - np.cos(theta_max)) * (phi_max - phi_min))
        
        lights.append({
            "light_id": idx,
            "direction": [round(x, 6) for x in world_direction],
            "brightness": round(avg_brightness, 6),
            "area": round(sphere_area, 6)
        })
    
    return {
        "light_count": len(lights),
        "lights": lights
    }

if __name__ == "__main__":
    # 配置所有需要处理的HDR和对应的像素区域
    hdr_configs = [
        {
            "hdr_index": 1,
            "pixel_regions": [
                ((759, 115), (792, 146)),  # 第1组像素区域
                ((561, 125), (617, 155)),  # 第2组像素区域
                ((80, 135), (133, 154))    # 第3组像素区域
            ],
            "brightness_threshold": 0.5
        },
        {
            "hdr_index": 2,
            "pixel_regions": [
                ((469, 217), (591, 253)),  # 第1组像素区域
                ((299, 24), (425, 74)),    # 第2组像素区域
                ((11, 149), (46, 163))     # 第3组像素区域
            ],
            "brightness_threshold": 0.5
        },
        {
            "hdr_index": 3,
            "pixel_regions": [
                ((477, 217), (591, 253))   # 第1组像素区域
            ],
            "brightness_threshold": 0.5
        },
        {
            "hdr_index": 4,
            "pixel_regions": [
                ((380, 160), (413, 184)),  # 第1组像素区域
                ((566, 148), (594, 171)),  # 第2组像素区域
                ((747, 230), (772, 244))   # 第3组像素区域
            ],
            "brightness_threshold": 0.5
        },
        {
            "hdr_index": 5,
            "pixel_regions": [
                ((533, 223), (556, 238)),  # 第1组像素区域
                ((739, 239), (765, 249))   # 第2组像素区域
            ],
            "brightness_threshold": 0.5
        }
    ]
    
    # 存储所有HDR的光源信息
    all_hdr_lights = {}
    
    # 循环处理每个HDR配置
    for config in hdr_configs:
        hdr_index = config["hdr_index"]
        pixel_regions = config["pixel_regions"]
        threshold = config["brightness_threshold"]
        
        # 构建HDR文件路径
        hdr_path = f"textures/{hdr_index}.hdr"
        
        try:
            # 提取该HDR的光源信息
            light_info = extract_lights(hdr_path, pixel_regions, hdr_index, threshold)
            all_hdr_lights[str(hdr_index)] = light_info
            print(f"成功处理HDR {hdr_index}，提取到 {light_info['light_count']} 个有效光源")
        except FileNotFoundError as e:
            print(f"警告：{e}，跳过该HDR")
        except Exception as e:
            print(f"处理HDR {hdr_index} 时出错：{e}，跳过该HDR")
    
    # 将完整的光源信息写入JSON文件
    output_file = "../LightInfo.json"
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(all_hdr_lights, f, indent=2, ensure_ascii=False)
    
    print(f"\n所有HDR光源信息已保存到：{output_file}")
    print(f"共处理 {len(all_hdr_lights)} 个有效HDR文件")
