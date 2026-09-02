# 3GX Updater

**3GX Updater** est une application homebrew pour Nintendo 3DS (format `.3dsx`) permettant de parcourir, télécharger et installer des plugins `.3gx` (compatibles CTRPluginFramework / Luma3DS) depuis un index centralisé et les releases GitHub.

*Inspiré par [Universal-Updater](https://github.com/Universal-Team/Universal-Updater).*

---

## ✨ Fonctionnalités

- 📡 **Connexion HTTPS** via `libcurl` + `mbedtls`
- 📑 **Index JSON centralisé** pour la liste des plugins
- 📦 **Installation automatique** des `.3gx` dans `/luma/plugins/<TITLE_ID>/`
- 🔄 **Mise à jour automatique de l'application** au démarrage (vérification des releases GitHub)
- 📊 **Barre de progression** en temps réel pour chaque téléchargement
- 🎨 **Interface moderne** avec cartes ombrées, badges de catégorie, spinners de chargement
- 🎮 **Contrôles** : DPAD, A (installer), SELECT (rafraîchir), START (quitter)

### Catégories de plugins supportées

| Catégorie | Label | Couleur |
|-----------|-------|---------|
| `game` | JEU | Violet |
| `system` | SYS | Orange |
| `plugin` | PLG | Bleu |
| `tool` | OUTIL | Vert |

---

## 🛠️ Prérequis

Environnement **devkitPro** configuré pour la 3DS.

```bash
sudo dkp-pacman -S 3ds-dev 3ds-curl 3ds-mbedtls 3ds-citro2d 3ds-citro3d 3ds-cjson 3ds-zlib
```

---

## 🏗️ Build

```bash
git clone https://github.com/GotazZ/3gx-updater.git
cd 3gx-updater
make
```

Le binaire `3gx-updater.3dsx` est généré à la racine.

---

## 📄 Format de l'index JSON

L'application charge la liste depuis une URL JSON. Le repo et le depot GitHub de l'application sont interrogés pour récupérer la dernière release.

### Schéma d'une entrée plugin

| Champ | Type | Description |
|-------|------|-------------|
| `id` | string | Identifiant unique |
| `name` | string | Nom affiché |
| `author` | string | Auteur |
| `description` | string | Description courte |
| `repo` | string | Repo GitHub `owner/name` |
| `titleIds` | string[] | Title IDs cibles (le plugin sera installé dans `/luma/plugins/<titleId>/`) |
| `category` | string | `game`, `system`, `plugin`, `tool` |

### Exemple

```json
{
  "plugins": [
    {
      "id": "ykw2-editor-plugin",
      "name": "YKW2 Editor Plugin",
      "author": "gotaz",
      "description": "Plugin CTRPF pour Yo-kai Watch 2",
      "repo": "gotaz/ykw2-editor",
      "titleIds": ["00040000001B0500", "00040000001B0600"],
      "category": "game"
    }
  ]
}
```

---

## 🔄 Mise à jour automatique

À chaque lancement, l'application :
1. Charge l'index des plugins
2. Interroge l'API GitHub `releases/latest` du dépôt courant
3. Si une nouvelle version est disponible, **télécharge et installe automatiquement** le nouveau `.3dsx`
4. Affiche un message invitant à redémarrer l'application

Le numéro de version est défini dans `include/app_updater.hpp` (`AppUpdater::CURRENT_VERSION`).

---

## 🎮 Utilisation

1. Copier `3gx-updater.3dsx` à la racine de la carte SD
2. Lancer via le Homebrew Launcher
3. Naviguer avec le **DPAD**
4. Appuyer sur **(A)** pour installer le plugin sélectionné
5. Appuyer sur **(SELECT)** pour rafraîchir l'index
6. Appuyer sur **(START)** pour quitter

L'indicateur `[INSTALLE]` apparaît à côté des plugins déjà présents sur la carte SD.

---

## 📁 Structure du projet

```
3gx-updater/
├── Makefile
├── index.json              # Index par défaut (référencé par l'app)
├── icon.png
├── include/
│   ├── network.hpp         # Wrapper libcurl
│   ├── plugin_manager.hpp  # Parsing JSON + installation
│   ├── app_updater.hpp     # Auto-update
│   └── cJSON.h
└── source/
    ├── main.cpp            # UI + boucle principale
    ├── network.cpp
    ├── plugin_manager.cpp
    ├── app_updater.cpp
    └── cJSON.c
```

---

## 🤝 Contribution

PRs bienvenues. Pour ajouter un plugin, modifier `index.json` et soumettre une PR.

---

## 📜 Licence

MIT. Voir `LICENSE`.
