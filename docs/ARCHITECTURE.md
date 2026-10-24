# Architecture & formats

## 1. Découpage

```
            ┌──────────────────────────┐
            │ src/core (C99 portable)  │
            │  session  ─ orchestration│
            │  xiso     ─ parseur      │
            │  extract  ─ écriture     │
            │  cache    ─ purge        │
            │  fs / iosrc (abstraits)  │
            └───────┬──────────┬───────┘
                    │          │
      ┌─────────────┴──┐   ┌───┴────────────────────┐
      │ src/host       │   │ src/xbox               │
      │ fs POSIX       │   │ fs FindFirstFileA      │
      │ iosrc stdio    │   │ iosrc CreateFile 64 b  │
      │ CLI + tests    │   │ ui_d3d + main + launch │
      └────────────────┘   └────────────────────────┘
```

Aucune ligne de `src/core` ne connaît la console : le moteur est testable sur PC, et
réutilisable pour une autre plateforme (UWP, Linux, émulateur).

### Flux d'une session (`xib_run`)

1. `xib_purge(cache_root)` — efface tout reste (démarrage d'appli).
2. `iosrc_open` + `xiso_probe` — trouve la partition jeu.
3. `xiso_stat` — inventaire (nombre de fichiers, octets).
4. `extract_find_launcher` — localise `default.xex` (ou le premier `.xex` à la racine).
5. `cache_has_space` — espace libre ≥ payload + 64 Mio.
6. `extract_prepare` / `extract_all` — extraction avec progression.
7. `cache_mark_session` — marqueur de session (détection d'arrêt brutal).
8. `launch(exe, cwd)` — `XLaunchNewImage` sur console.
9. `cache_purge` — nettoyage final si on reprend la main.

Codes de retour : `XIB_OK`, `XIB_ERR_OPEN_ISO`, `XIB_ERR_NOT_XISO`, `XIB_ERR_NO_XEX`,
`XIB_ERR_NO_SPACE`, `XIB_ERR_EXTRACT`, `XIB_ERR_LAUNCH`, `XIB_ERR_CACHE`.

---

## 2. Format XDVDFS (ce que le parseur implémente)

Secteurs de 2048 octets. La partition jeu commence à un offset qui dépend du mastering :

| Type | Offset de la partition |
|---|---|
| XISO « trimée » (extract-xiso, GoD) | `0x0` |
| XGD3 | `0x2080000` |
| XGD2 | `0xFD90000` |
| XGD1 | `0x18300000` |
| inconnu | scan par pas de 2048 octets (64 Mio max) |

### Descripteur de volume (secteur 32 de la partition)

| Offset | Type | Champ |
|---|---|---|
| `0x00` | char[20] | `MICROSOFT*XBOX*MEDIA` |
| `0x14` | u32 | secteur de la table du répertoire racine |
| `0x18` | u32 | taille de la table racine (octets) |
| `0x1C` | u64 | FILETIME de création |
| `0x7EC` | char[20] | `MICROSOFT*XBOX*MEDIA` (répété) |

### Entrée de répertoire

| Offset | Type | Champ |
|---|---|---|
| `0x00` | u16 | enfant gauche, en **mots de 4 octets** |
| `0x02` | u16 | enfant droit, idem |
| `0x04` | u32 | secteur de départ (fichier ou table) |
| `0x08` | u32 | taille en octets |
| `0x0C` | u8 | attributs (`0x10` dossier, `0x20` archive, …) |
| `0x0D` | u8 | longueur du nom |
| `0x0E` | char[] | nom (padding à 4 octets) |

Détails gérés par `xiso.c` :

- **Racine de l'arbre** = entrée à l'offset 0 de la première table.
- **Deux conventions d'offsets** : relatifs à la table (standard) ou relatifs à l'entrée
  (variantes) — détectées automatiquement par validation de l'arbre.
- **Sentinel `0xFFFF`** en fin de secteur (images « linked list ») → reprise au secteur
  suivant.
- Les entrées ne traversent jamais une frontière de secteur ; le reste est du padding.
- Garde-fous : profondeur ≤ 32, 200 000 entrées max, table ≤ 16 Mio, offsets bornés.

### Pourquoi une extraction et pas une lecture à la volée ?

Un XEX de jeu ne sait pas lire dans une ISO : le noyau Xbox 360 monte les disques, pas les
fichiers image. Les launchers (FSD, Aurora, XeXMenu) suivent tous la même stratégie :
extraire puis lancer. XIBoot l'implémente avec un cache **temporaire garanti**.

---

## 3. Cache et purge

```
hdd:\XIBoot\cache\
    .xiboot-session          <- marqueur : session en cours
    MonJeu\                  <- une extraction par jeu
        default.xex
        media\...
```

`cache_purge()` parcourt récursivement et supprime **tout** (fichiers + dossiers), en
conservant la racine. Appelée :

- au démarrage (`main`),
- avant chaque lancement,
- après un retour dans l'appli,
- manuellement (bouton **Y**).

Le marqueur `.xiboot-session` est supprimé en fin de session normale ; sa présence au
démarrage signale une session interrompue (coupure de courant) — la purge a de toute façon
déjà eu lieu ; il sert de diagnostic (affiché par l'UI).

### Choix du volume

Au démarrage, XIBoot compare l'espace libre de `hdd:`, `usb0:`, `usb1:` et place le cache
sur le plus généreux. Les jeux peuvent ainsi tenir sur une clé USB sans configuration.

---

## 4. Assainissement FATX

FATX (Xbox 360) impose : ≤ 42 caractères, pas de `" * / \ : < > ? |`, pas de point/espace
final, pas de `.`/`..`. `pu_sanitize()` et `extract_map_relpath()` appliquent ces règles
composant par composant — ce qui empêche aussi toute tentative de traversée de répertoire
(`../`) depuis une image piégée.

Les collisions de noms après troncature ne sont pas fusionnées : le dernier fichier écrit
gagne. Cas rare (noms > 42 caractères dans la même image).

---

## 5. Tests

`tests/test_core.c` couvre :

- assainissement FATX, découpage/normalisation de chemins ;
- lecture d'ISO trimée **et** XGD2 (partition noyée dans du bruit) ;
- détection d'une partition à un offset arbitraire (scan générique) ;
- comparaison octet par octet entre l'ISO et les fichiers extraits ;
- purge récursive (cache vide, racine conservée) ;
- session complète avec lanceur simulé ;
- purge d'un cache laissé par une session précédente + marqueur de session ;
- cas d'erreur (ISO absente, fichier non-XDVDFS, arguments invalides).

`tools/mkxiso.py` fabrique les images de test (arbre binaire, padding `0xFF`, noms
« sales », offset de partition paramétrable).
