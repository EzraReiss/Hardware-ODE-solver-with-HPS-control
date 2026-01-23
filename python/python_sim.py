import matplotlib.pyplot as plt
import numpy as np

dt = (1./256)
x = [-1.]
y = [0.1]
z = [25.]
sigma = 10.0
beta = 8./3.
rho = 28.0

def dx(sigma, x, y):
    return sigma*(y-x)

def dy(rho, x, y, z):
    return x*(rho-z)-y

def dz(beta, x, y, z):
    return x*y - beta*z

for i in range(10000):
    x.extend([x[i] + dt*dx(sigma, x[i], y[i])])
    y.extend([y[i] + dt*dy(rho, x[i], y[i], z[i])])
    z.extend([z[i] + dt*dz(beta, x[i], y[i], z[i])])



"""AI Code starts here"""
fig, axs = plt.subplots(3)
axs[0].plot(x)
axs[0].set_ylabel('x')
axs[1].plot(y)
axs[1].set_ylabel('y')
axs[2].plot(z)
axs[2].set_ylabel('z')
plt.show()
"""AI Code ends here"""
