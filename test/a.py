import matplotlib.pyplot as plt
import numpy as np # 通常与matplotlib一起使用，便于处理数组

# 1. 准备数据
# 横坐标：卫星节点个数
satellite_counts = np.array([50, 100, 200, 300, 400, 500])

# 纵坐标：Type 0 数据报文总发送次数 (使用你之前图片中的数据)
# (请确保这些数据与上述 satellite_counts 一一对应)
hybrid_sends = np.array([290, 490, 950, 1900, 3190, 3990])       # 你的算法
flooding_sends = np.array([1740, 2440, 5940, 10940, 17940, 23940]) # 基础 P2P (Flooding)
sr_sends = np.array([360, 1270, 2750, 6040, 10900, 16760])       # 对照策略 SR

# 2. 创建图形和坐标轴
plt.figure(figsize=(10, 6)) # 设置图形大小，可以根据需要调整

# 3. 绘制折线
# 使用不同的标记(marker)、线型(linestyle)和颜色(color)来区分不同的策略
plt.plot(satellite_counts, hybrid_sends, marker='o', linestyle='-', color='green', linewidth=2, label='           ')
plt.plot(satellite_counts, flooding_sends, marker='s', linestyle='--', color='red', linewidth=2, label='                      ')
plt.plot(satellite_counts, sr_sends, marker='^', linestyle='-.', color='blue', linewidth=2, label='            ')

# 4. 添加图表元素
#plt.title('', fontsize=16, fontfamily='SimHei') # SimHei支持中文
#plt.xlabel('Satellite Count', fontsize=14, fontfamily='Times New Roman')
#plt.ylabel('Sending Times', fontsize=14, fontfamily='Times New Roman')



# 设置横坐标刻度，确保所有定义的卫星数量都显示出来
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
plt.show() # 在窗口中显示图形

# 如果需要保存图片到文件:
# plt.savefig('type0_sends_vs_satellite_count.png', dpi=300) # dpi可以调整图片清晰度
