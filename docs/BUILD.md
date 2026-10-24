# Build

Deux builds distincts :

| Build | Commande | Ce qu'il faut |
|---|---|---|
| **Hôte (PC)** — tests + CLI | `make && make fixtures && make test` | gcc/clang, python3 |
| **Console (XEX)** | voir ci-dessous | XDK **ou** xkelib + MSVC, console RGH/JTAG |
| Vérification des types console | `make check-xbox` | gcc/g++ (stubs) |

---

## 1. Build hôte

```bash
make            # -> build/xiboot-host
make fixtures   # -> build/fixtures/{game-trim.iso, game-xgd2.iso, game-src/}
make test       # -> build/test-core (108 vérifications)
```

Le CLI hôte partage **exactement** le même pipeline que l'appli console
(`src/core/session.c`) ; seule la couche `fs`/`iosrc`/lanceur change.

---

## 2. Build console (XEX)

### Option A — XDK officiel (Xbox 360 SDK)

1. Créer un projet **Xbox 360 Application** (C/C++) dans Visual Studio.
2. Ajouter les sources :
   - `src/core/*.c`
   - `src/xbox/main.c`, `src/xbox/fs_xbox.c`, `src/xbox/iosrc_xbox.c`, `src/xbox/launch_xbox.c`
   - `src/xbox/ui_d3d.cpp`
3. Inclure les répertoires `src/core` et `src/xbox`.
4. Édition de liens : `d3d9.lib d3dx9.lib xinput.lib xapilib.lib`.
5. Compiler en **Release**, sortie `default.xex`.

Le code n'utilise que des API stables du XDK : fichiers Win32 (`FindFirstFileA`,
`CreateFileA`, `SetFilePointerEx`, `ReadFile`, `GetDiskFreeSpaceExA`), `XLaunchNewImage`,
`XamInputGetState`, Direct3D 9 + D3DX (font/line).

### Option B — xkelib (homebrew, sans XDK)

[xkelib](https://github.com/XboxDev/xkelib) reconstruit les en-têtes et bibliothèques
du XDK à partir de libxenon. Avec MSVC :

1. Récupérer xkelib et son `xbox360.toolchain.cmake`.
2. Compiler les mêmes sources (`main.c` peut être renommé `main.cpp` si besoin).
3. Édition de liens avec `xkelib.lib` (+ `d3d9.lib`, `d3dx9.lib` selon la distribution).
4. Passer l'ELF/XEX obtenu dans `xextool -i` pour vérifier les en-têtes.

### Option C — libxenon

libxenon fournit `xenon-gcc` et un `Makefile` type. Deux adaptations :

- remplacer `XamInputGetState` par l'API manette de libxenon (`xenon_input`),
- remplacer `XLaunchNewImage` par le lanceur de libxenon (`xenon_launch`).

Tout le reste (`src/core`) est du C99 portable et compile tel quel.

---

## 3. Vérification sans SDK

```bash
make check-xbox
```

Compile `src/xbox/*` contre `src/xbox/stub/xbox_stub.h` (déclarations minimales).
Objectif : attraper les erreurs de type/typo sans posséder le SDK. Ce n'est **pas** un
build XEX et cela ne valide pas le comportement matériel.

---

## 4. Signature et installation

| Console | Exigence |
|---|---|
| **RGH / JTAG** | `.xex` non signé accepté — rien à faire |
| **Devkit** | signer avec `xextool`/`Xbox 360 Neighborhood`, ou lancer en mode « unsigned » |
| **Retail** | impossible sans modchip/RGH : le chargeur vérifie les signatures |

Déploiement (au choix) :

```
hdd:\XIBoot\default.xex          <- l'application
hdd:\Games\MonJeu.iso            <- les images à lancer
hdd:\XIBoot\cache\               <- créé automatiquement, purgé à chaque session
```

Copie par FTP (XeXMenu/FSD/Aurora) ou clé USB formatée Xbox (`usb0:\XIBoot\`).

Lancement : depuis XeXMenu / Aurora / FSD / Dashlaunch.

---

## 5. Réglages utiles

| Constante | Fichier | Rôle |
|---|---|---|
| `XIB_CACHE_DIR` | `src/xbox/platform.h` | dossier de cache par défaut |
| `XIB_GAMES_DIR` | `src/xbox/platform.h` | dossier scanné au démarrage |
| `MAX_GAMES` | `src/xbox/main.c` | nombre max d'ISO listées (128) |
| `COPY_CHUNK` | `src/core/extract.c` | taille des blocs d'extraction (256 Kio) |
| marges d'espace libre | `src/core/cache.c` | marge de sécurité (64 Mio) |

Pour déboguer en gardant le cache : compiler avec `-DXIB_KEEP_CACHE` ou appeler
`xib_run` avec `opt.keep_cache = 1` (le CLI hôte : `--keep`).
