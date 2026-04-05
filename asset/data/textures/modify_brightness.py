import tkinter as tk
from tkinter import filedialog, ttk, messagebox
import cv2
import numpy as np
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt

# 配置中文显示
plt.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

class HDRAdjuster:
    def __init__(self, root):
        self.root = root
        self.root.title("HDR贴图RGB通道调整工具")
        self.root.geometry("1000x700")
        self.hdr_image = None
        self.adjusted_image = None  # 存储调整后的完整数值图像（用于导出）
        # 修改最大倍数为100000，初始值保持1.0
        self.r_scale = tk.DoubleVar(value=1.0)
        self.g_scale = tk.DoubleVar(value=1.0)
        self.b_scale = tk.DoubleVar(value=1.0)
        self._create_widgets()
        self.r_scale.trace('w', self._update_image)
        self.g_scale.trace('w', self._update_image)
        self.b_scale.trace('w', self._update_image)

    def _create_widgets(self):
        # 顶部按钮区域
        button_frame = ttk.Frame(self.root)
        button_frame.pack(pady=10, fill=tk.X, padx=20)
        
        load_btn = ttk.Button(button_frame, text="加载HDR贴图", command=self._load_hdr)
        load_btn.pack(side=tk.LEFT, padx=5)
        
        save_btn = ttk.Button(button_frame, text="保存调整后HDR", command=self._save_hdr)
        save_btn.pack(side=tk.LEFT, padx=5)

        # 滑块调整区域（最大倍数改为100000）
        slider_frame = ttk.Frame(self.root)
        slider_frame.pack(pady=10, fill=tk.X, padx=20)
        for i, (label, var, val_label) in enumerate([
            ("R通道放大倍数:", self.r_scale, "r_value_label"),
            ("G通道放大倍数:", self.g_scale, "g_value_label"),
            ("B通道放大倍数:", self.b_scale, "b_value_label")
        ]):
            ttk.Label(slider_frame, text=label).grid(row=i, column=0, padx=5, pady=5, sticky=tk.W)
            # 修改to=100000，步长适配大数值
            ttk.Scale(slider_frame, from_=0, to=100, variable=var, orient=tk.HORIZONTAL).grid(row=i, column=1, padx=5, pady=5, sticky=tk.EW)
            setattr(self, val_label, ttk.Label(slider_frame, text=f"{var.get():.1f}x"))
            getattr(self, val_label).grid(row=i, column=2, padx=5, pady=5)
        slider_frame.columnconfigure(1, weight=1)

        # 预览区域
        self.fig, self.ax = plt.subplots(figsize=(8, 5))
        self.ax.axis('off')
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(pady=10, fill=tk.BOTH, expand=True, padx=20)
        self.ax.text(0.5, 0.5, "请加载HDR贴图", ha='center', va='center', transform=self.ax.transAxes, fontsize=14)
        self.canvas.draw()

    def _load_hdr(self):
        file_path = filedialog.askopenfilename(
            title="选择HDR贴图",
            filetypes=[("HDR文件", "*.hdr"), ("EXR文件", "*.exr"), ("所有文件", "*.*")]
        )
        if not file_path:
            return
        try:
            self.hdr_image = cv2.imread(file_path, cv2.IMREAD_ANYDEPTH | cv2.IMREAD_COLOR)
            if self.hdr_image is None:
                raise ValueError("OpenCV无法识别该文件格式")
            self.hdr_image = cv2.cvtColor(self.hdr_image, cv2.COLOR_BGR2RGB)
            self._update_image()
        except Exception as e:
            messagebox.showerror("错误", f"加载HDR文件失败：{str(e)}\n建议检查文件路径/格式是否正确。")

    def _update_image(self, *args):
        if self.hdr_image is None:
            return
        
        # 获取真实放大倍数（无任何修改）
        r_factor = self.r_scale.get()
        g_factor = self.g_scale.get()
        b_factor = self.b_scale.get()
        
        # 更新数值标签（显示真实倍数，适配大数值）
        self.r_value_label.config(text=f"{r_factor:.1f}x")
        self.g_value_label.config(text=f"{g_factor:.1f}x")
        self.b_value_label.config(text=f"{b_factor:.1f}x")

        # 严格按倍数放大通道，不做任何优化（核心修改：移除所有预览优化）
        self.adjusted_image = self.hdr_image.copy().astype(np.float32)
        self.adjusted_image[..., 0] *= r_factor  # R通道严格乘以设置的倍数
        self.adjusted_image[..., 1] *= g_factor  # G通道严格乘以设置的倍数
        self.adjusted_image[..., 2] *= b_factor  # B通道严格乘以设置的倍数

        # 完全原始的预览：不裁剪、不归一化，该什么样就什么样
        self.ax.clear()
        self.ax.imshow(self.adjusted_image)
        self.ax.axis('off')
        # 仅显示倍数信息，不添加任何优化相关内容
        self.ax.text(0.02, 0.98, f"当前R:{r_factor:.1f}x G:{g_factor:.1f}x B:{b_factor:.1f}x", 
                    ha='left', va='top', transform=self.ax.transAxes, 
                    fontsize=10, color='white', bbox=dict(facecolor='black', alpha=0.7))
        self.canvas.draw()

    def _save_hdr(self):
        """保存调整后的HDR图像，保留完整的数值信息"""
        if self.adjusted_image is None:
            messagebox.showwarning("提示", "请先加载并调整HDR贴图后再保存！")
            return
        
        file_path = filedialog.asksaveasfilename(
            title="保存调整后HDR贴图",
            defaultextension=".hdr",
            filetypes=[
                ("HDR文件", "*.hdr"),
                ("EXR文件", "*.exr"),
                ("所有文件", "*.*")
            ]
        )
        if not file_path:
            return
        
        try:
            # 将RGB转回BGR（适配OpenCV的保存格式）
            save_image = cv2.cvtColor(self.adjusted_image, cv2.COLOR_RGB2BGR)
            # 保存HDR/EXR文件（无损保存浮点型数据）
            success = cv2.imwrite(file_path, save_image)
            if success:
                messagebox.showinfo("成功", f"HDR文件已保存至：\n{file_path}")
            else:
                raise ValueError("OpenCV保存失败，可能是格式不支持")
        except Exception as e:
            messagebox.showerror("错误", f"保存HDR文件失败：{str(e)}\n建议尝试保存为.exr格式。")

if __name__ == "__main__":
    root = tk.Tk()
    app = HDRAdjuster(root)
    root.mainloop()
