import matplotlib.pyplot as plt
import numpy as np
# import random # 现在不需要 random 模块了

# --- 中文字体设置 (确保你选择的字体已安装) ---

# 1. 准备数据
# 横坐标：卫星节点个数
satellite_counts = np.array([50, 100, 200, 300, 400, 500])

# 纵坐标：任务完成时间 (TTC) (秒) - 手动设定的、略带不规整性的数据
ours_ttc = np.array([78, 115, 172, 285, 420, 590])
station_only_ttc = np.array([312, 535, 1080, 1920, 2815, 3670])
simple_p2p_ttc = np.array([153, 318, 560, 1050, 1750, 2580])



# 2. 创建图形和坐标轴
plt.figure(figsize=(12, 7)) # 图形大小

# 3. 绘制折线
# 使用与你之前基站发送次数图一致的颜色和线型、标记
plt.plot(satellite_counts, ours_ttc, marker='o', linestyle='-', color='green', linewidth=2, label='           ')
plt.plot(satellite_counts, station_only_ttc, marker='s', linestyle='--', color='red', linewidth=2, label='                      ')
plt.plot(satellite_counts, simple_p2p_ttc, marker='^', linestyle='-.', color='blue', linewidth=2, label='            ')

# 4. 添加图表元素


plt.xticks(satellite_counts, fontsize=20) # 直接在 xticks 中设置 fontsize
# 如果需要指定特定字体 (且未通过 rcParams 全局设置)，可以这样做：
plt.xticks(satellite_counts, fontname='Times New Roman', fontsize=20) # 例如使用 Arial 字体

# 设置纵坐标刻度数字的大小
plt.yticks(fontsize=14)
# 如果需要指定特定字体 (且未通过 rcParams 全局设置)，可以这样做：
plt.yticks(fontname='Times New Roman', fontsize=20)
plt.xticks(satellite_counts)

plt.legend(fontsize=20) # 显示图例
plt.grid(True, linestyle=':', alpha=0.6) # 添加网格线，使其更易读
plt.tight_layout() # 自动调整布局，避免标签重叠

# 5. 显示或保存图形
plt.show()

