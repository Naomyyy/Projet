import matplotlib.pyplot as plt

# Dados
threads = [1, 4, 6, 8, 10, 12, 14, 16]
temps_logique = [9.96, 7.32, 6.21, 5.95, 5.63, 5.93, 7.27, 27.11]

# Calcular speedup
speedup = [temps_logique[0]/t for t in temps_logique]

# Criar o gráfico
plt.figure(figsize=(10,6))
plt.plot(threads, speedup, marker='o', linestyle='-', color='b', label='Speedup')

# Destacar regiões
plt.axvspan(1, 10, color='green', alpha=0.1, label='Amélioration')
plt.axvspan(10, 14, color='yellow', alpha=0.1, label='Saturation')
plt.axvspan(14, 16, color='red', alpha=0.1, label='Dégradation')

# Anotação para o melhor speedup
best_idx = speedup.index(max(speedup))
plt.annotate(f'Max Speedup: {max(speedup):.2f}',
             xy=(threads[best_idx], speedup[best_idx]),
             xytext=(threads[best_idx]+0.5, speedup[best_idx]+0.1),
             arrowprops=dict(facecolor='black', shrink=0.05))

# Labels e título
plt.xlabel('Nombre de threads')
plt.ylabel('Speedup (T_seq / T_parallel)')
plt.title('Scalabilité avec OpenMP')
plt.xticks(threads)
plt.grid(True)
plt.legend()
plt.tight_layout()

# Salvar a imagem
plt.savefig('speedup_plot.png', dpi=300)  # salva como PNG de alta qualidade
plt.show()