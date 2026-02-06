import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# Read the CSV, skip any lines that don't parse as numbers
df = pd.read_csv('coords.csv', comment='^', on_bad_lines='skip')
df.columns = df.columns.str.strip()
df = df.apply(pd.to_numeric, errors='coerce').dropna()

fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')

ax.plot(df['X'], df['Y'], df['Z'], lw=0.5, color='blue')

ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')
ax.set_title('Lorenz Attractor (FPGA Output)')

plt.tight_layout()
plt.show()