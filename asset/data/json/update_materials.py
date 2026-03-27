import json

# 定义5种类别的材质属性
# 格式: (hex_R, hex_G, hex_B): (albedoValue, metallicValue, roughnessValue)
CATEGORIES = [
    {
        "name": "000000 黑色镜面按钮",
        "hex": (0x00, 0x00, 0x00),
        "albedo": [0.0, 0.0, 0.0],
        "metallic": 0.392,
        "roughness": 0.651,
    },
    {
        "name": "333333 漫反射灰板",
        "hex": (0x33, 0x33, 0x33),
        "albedo": [0.176, 0.2, 0.151],
        "metallic": 0.909,
        "roughness": 0.899,
    },
    {
        "name": "BBBBBB 漫反射灰板",
        "hex": (0xBB, 0xBB, 0xBB),
        "albedo": [0.176, 0.2, 0.151],
        "metallic": 0.909,
        "roughness": 0.899,
    },
    {
        "name": "FF0000 塑料红板",
        "hex": (0xFF, 0x00, 0x00),
        "albedo": [0.780, 0.013, 0.013],
        "metallic": 0.384,
        "roughness": 0.273,
    },
]

DEFAULT_CATEGORY = {
    "name": "其他 黄色文字/线条",
    "albedo": [0.160, 0.072, 0.003],
    "metallic": 0.124,
    "roughness": 0.963,
}

TOLERANCE = 0.004


def rgb_float_to_hex(r, g, b):
    """将 0~1 浮点 RGB 转为 hex 值"""
    return (round(r * 255), round(g * 255), round(b * 255))


def find_category(r_hex, g_hex, b_hex):
    """找到最匹配的类别，容差 0.004"""
    best_cat = None
    best_dist = float("inf")

    for cat in CATEGORIES:
        hr, hg, hb = cat["hex"]
        # 计算每个通道的差值（归一化到 0~1）
        dist = max(
            abs(r_hex - hr) / 255.0,
            abs(g_hex - hg) / 255.0,
            abs(b_hex - hb) / 255.0,
        )
        if dist < best_dist:
            best_dist = dist
            best_cat = cat

    if best_dist <= TOLERANCE:
        return best_cat
    return DEFAULT_CATEGORY


def main():
    json_path = "asset/data/json/Materials.json"
    with open(json_path, "r") as f:
        data = json.load(f)

    stats = {}
    updated = 0

    for mat_name, mat_params in data.get("material_params", {}).items():
        bcf = mat_params.get("baseColorFactor", [0, 0, 0, 1])
        r_hex, g_hex, b_hex = rgb_float_to_hex(bcf[0], bcf[1], bcf[2])

        cat = find_category(r_hex, g_hex, b_hex)
        cat_name = cat["name"]

        # 覆盖写入
        mat_params["baseColorFactor"] = cat["albedo"] + [1.0]
        mat_params["metallicFactor"] = cat["metallic"]
        mat_params["roughnessFactor"] = cat["roughness"]

        stats[cat_name] = stats.get(cat_name, 0) + 1
        updated += 1

    with open(json_path, "w") as f:
        json.dump(data, f, indent=4)

    print(f"共更新 {updated} 个材质:")
    for name, count in sorted(stats.items(), key=lambda x: -x[1]):
        print(f"  {name}: {count}")


if __name__ == "__main__":
    main()
