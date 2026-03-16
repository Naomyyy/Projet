# Optimisation par Colonie de Fourmis sur un Paysage Fractal

Simulation d'un algorithme ACO (Ant Colony Optimisation) pour résoudre le problème de fourragement sur une carte fractale 2D. Quatre versions sont proposées : séquentielle, vectorisée, parallèle en mémoire partagée (OpenMP) et parallèle distribuée (MPI).

---

## Structure du projet

```
Projet/
├── Sequentiel/          # Version de référence (séquentielle)
├── Vectorized/          # Version avec données vectorisées (SoA)
├── Vectorized_par/      # Version OpenMP (mémoire partagée)
├── Vectorized_par_mpi/  # Version MPI + OpenMP (mémoire distribuée)
└── rapport/             # Rapport LaTeX
```

Chaque dossier contient :

```
<version>/
├── src/         # Sources C++ (.cpp)
├── include/     # En-têtes (.hpp)
├── obj/         # Objets compilés (généré)
├── make/        # Fichiers de configuration Make (Linux, macOS, MSYS2)
└── Makefile
```

---

## Dépendances

| Dépendance | Rôle |
|---|---|
| `g++` ≥ C++17 | Compilation |
| `SDL2` | Affichage graphique |
| `OpenMP` | Parallélisation mémoire partagée |
| `MPI` (`mpicxx`) | Parallélisation distribuée |

Installation sur Ubuntu/Debian :

```bash
sudo apt install build-essential libsdl2-dev libomp-dev libopenmpi-dev openmpi-bin
```

---

## Compilation

Depuis le dossier de chaque version :

```bash
# Compilation standard (optimisée)
make all

# Compilation en mode debug
make all DEBUG=yes

# Nettoyage
make clean
```

> Le `Makefile` de la version MPI utilise automatiquement `mpicxx` comme compilateur.

---

## Exécution

### Séquentiel / Vectorisé / OpenMP

```bash
./ant_simu.exe
```

Pour contrôler le nombre de threads OpenMP :

```bash
export OMP_NUM_THREADS=4
./ant_simu.exe
```

### MPI

```bash
mpirun -np 4 ./ant_simu.exe
```

> Remplacer `4` par le nombre de processus souhaité. Seul le processus de rang 0 ouvre la fenêtre SDL.

---

## Paramètres de simulation

Les paramètres sont définis dans `src/ant_simu.cpp` :

| Paramètre | Valeur par défaut | Description |
|---|---|---|
| `nb_ants` | 5000 | Nombre de fourmis |
| `eps` (ε) | 0.8 | Taux d'exploration aléatoire |
| `alpha` (α) | 0.7 | Paramètre de bruit (mise à jour phéromones) |
| `beta` (β) | 0.999 | Coefficient d'évaporation des phéromones |
| `pos_nest` | (256, 256) | Position de la fourmilière |
| `pos_food` | (500, 500) | Position de la source de nourriture |
| `seed` | 2026 | Graine pour la génération aléatoire |

Le terrain fractal est généré avec `fractal_land(8, 2, 1.0, 1024)` : une grille de 513×513 cellules dont les valeurs de traversée sont normalisées entre 0 et 1.

---

## Modèle ACO

À chaque pas de temps, chaque fourmi :

1. **Met à jour les phéromones** de sa cellule courante selon ses quatre voisins :

```
V(s) = α · max(voisins) + (1 − α) · moyenne(voisins)
```

2. **Se déplace** vers une cellule voisine :
   - avec probabilité ε : direction aléatoire parmi les voisins valides
   - avec probabilité (1 − ε) : direction du gradient de phéromone maximal

Les phéromones s'évaporent à chaque itération : `V(s) → β · V(s)`.

Deux types de phéromones sont maintenus : V1 guide les fourmis non chargées vers la nourriture, V2 guide les fourmis chargées vers le nid.

---

## Architecture du code

### Classes principales

**`fractal_land`** — Terrain de simulation.
Génère un paysage fractal par subdivision récursive (algorithme diamond-square). Chaque cellule stocke une valeur de coût de traversée normalisée entre 0 et 1.

**`pheronome`** — Carte des phéromones.
Stocke deux cartes (V1, V2) avec un double buffer pour découpler lecture et écriture dans un même pas de temps. Gère la mise à jour, l'évaporation et les conditions aux bords (cellules fantômes à −1).

**`ant`** — Agent fourmi.
- Version séquentielle : chaque objet `ant` possède ses attributs (`m_position`, `m_state`, `m_seed`).
- Versions vectorisées : les attributs sont déplacés dans quatre tableaux statiques partagés (`xs`, `ys`, `states`, `seeds`). Chaque fourmi n'est identifiée que par un indice `m_id`.

**`Renderer` / `Window`** — Affichage SDL2 de la simulation en temps réel.

### Vectorisation des données (SoA)

La vectorisation transforme la représentation des fourmis d'un tableau de structures (AoS) en une structure de tableaux (SoA) :

```
Avant (AoS) :   [ {x, y, state, seed}, {x, y, state, seed}, ... ]
Après (SoA) :   xs[]     = [ x0, x1, x2, ... ]
                ys[]     = [ y0, y1, y2, ... ]
                states[] = [ s0, s1, s2, ... ]
                seeds[]  = [ g0, g1, g2, ... ]
```

Cela améliore la localité de cache lors des accès à un seul attribut (ex. toutes les coordonnées x), ce qui est favorable à la vectorisation automatique par le compilateur.

---

## Mesure des performances

Chaque version mesure et affiche en fin d'exécution :

- Le temps total et moyen par itération pour chacune des trois phases : déplacement des fourmis, évaporation des phéromones, mise à jour des phéromones.
- Le nombre d'itérations jusqu'à la première livraison de nourriture au nid.
- Le temps total de simulation.

La version séquentielle est également profilée avec `gprof` (flag `-pg` activé dans le Makefile).

---

## Résultats de performance

### Comparaison séquentiel / vectorisé

| Version | Itérations | Temps total | Temps moyen/it. (déplacement) |
|---|---|---|---|
| Séquentiel | 2226 | 74.8 s | 2.48 ms |
| Vectorisé | 2585 | 86.3 s | 2.24 ms |

La différence du nombre d'itérations est due à la stochasticité de l'algorithme (les seeds divergent entre les deux versions à cause du bug de seed non initialisée en séquentiel). Le temps de déplacement par itération est légèrement meilleur en vectorisé (−10 %), ce qui confirme l'amélioration de la localité de cache, sans toutefois être spectaculaire car le goulot d'étranglement reste l'accès à la carte de phéromones partagée.

### Accélération OpenMP (déplacement des fourmis)

| Threads | Temps déplacement (ms) | Accélération |
|---|---|---|
| 1 | 5781.72 | ×1.00 |
| 2 | 4027.83 | ×1.44 |
| 4 | 2595.99 | ×2.23 |
| 6 | 1981.15 | ×2.92 |
| 10 | 1573.26 | ×3.67 |
| 14 | 1710.82 | ×3.38 |
| 16 | 5874.92 | ×0.98 |

L'accélération est réelle jusqu'à 10 threads. La régression à 16 threads s'explique par un dépassement du nombre de cœurs physiques de la machine (over-subscription) et par les accès concurrents non protégés à `mark_pheronome()` (data race). Par ailleurs, la loi d'Amdahl limite l'accélération globale car l'affichage SDL (~27 % du temps total) reste séquentiel.

### Performances MPI (méthode 1 — carte complète répliquée)

| Processus | Temps total | Temps déplacement | Temps évap. + sync |
|---|---|---|---|
| 2 | 86.6 s | 3287 ms | 3792 ms |
| 3 | 86.5 s | 1963 ms | 3891 ms |
| 4 | 86.8 s | 2702 ms | 5479 ms |
| 6 | 119.3 s | 7219 ms | 8490 ms |
| 8 | 153.7 s | 17792 ms | 14873 ms |

La version MPI se dégrade fortement au-delà de 3 processus. La cause principale est le `MPI_Allreduce` effectué à chaque itération sur la totalité de la carte de phéromones (≈ 4 Mo par échange), dont le coût de communication croît plus vite que le gain obtenu par la réduction du nombre de fourmis locales. C'est le défaut inhérent à la méthode 1 décrite dans le sujet : elle n'est efficace que si la carte est petite et le nombre de fourmis très grand.

---

## Points d'attention et bugs connus

**Bug de seed non initialisée (séquentiel)** — Dans `ant.hpp` (version séquentielle), le constructeur ne copie pas l'argument `seed` dans le membre `m_seed`. La fourmi utilise donc une valeur indéterminée pour ses tirages aléatoires. Ce bug est absent des versions vectorisées qui utilisent le tableau `seeds[]`.

**Data race sur `cpteur_food` (OpenMP)** — Le compteur de nourriture est incrémenté sans protection dans la boucle parallèle. Il faudrait utiliser une clause `reduction(+:cpteur)` sur le `#pragma omp parallel for` pour garantir un résultat correct.

**Data race sur `mark_pheronome()` (OpenMP)** — Plusieurs fourmis peuvent écrire simultanément dans le buffer de phéromones. L'algorithme étant stochastique, l'impact pratique est limité, mais le comportement est formellement indéfini.
