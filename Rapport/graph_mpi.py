import matplotlib.pyplot as plt
import numpy as np

labels = ['2 Processos', '4 Processos']
computacao = [4.39, 39.83]
comunicacao = [9.13, 49.75]

x = np.arange(len(labels))
width = 0.35

fig, ax = plt.subplots()
rects1 = ax.bar(x - width/2, computacao, width, label='Computação (Lógica)', color='#4CAF50')
rects2 = ax.bar(x + width/2, comunicacao, width, label='Comunicação (MPI)', color='#F44336')

ax.set_ylabel('Tempo (segundos)')
ax.set_title('Computação vs Comunicação por Processos MPI')
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.legend()

plt.show()