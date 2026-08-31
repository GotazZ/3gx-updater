# 3GX Updater

**3GX Updater** est une application homebrew pour Nintendo 3DS (format `.3dsx`) permettant de parcourir, télécharger et mettre à jour automatiquement des plugins `.3gx` (compatibles CTRPluginFramework / Luma3DS) depuis un index centralisé et les releases GitHub.

Inspired by [Universal-Updater](https://github.com/Universal-Team/Universal-Updater).

---

## 🚀 Fonctionnalités

- 📡 **Connexion HTTPS sécurisée** via `libcurl` et `mbedtls`.
- 🔍 **Index JSON centralisé** pour charger dynamiquement la liste des plugins disponibles.
- 📦 **Installation automatique** des fichiers `.3gx` directement dans les dossiers cibles de Luma3DS (`/luma/plugins/<TITLE_ID>/`).
- 🎨 **Interface graphique fluide** basée sur `citro2d` / `citro3d`.
- 🎮 **Support complet des entrées 3DS** (boutons, écran tactile).

---

## 🛠️ Noyau & Prérequis de Compilation

Pour compiler le projet depuis les sources, vous devez disposer de l'environnement **devkitPro** configuré pour la 3DS.

### Dépendances pacman (devkitPro) :
```bash
sudo dkp-pacman -S 3ds-dev 3ds-curl 3ds-mbedtls 3ds-citro2d 3ds-citro3d 3ds-cjson 3ds-zlib
```

---

## 🏗️ Build

Pour construire l'exécutable `.3dsx` :

```bash
git clone https://github.com/gotaz/3gx-updater.git
cd 3gx-updater
make
```

Le fichier compilé `3gx-updater.3dsx` sera généré à la racine du projet.

---

## 📄 Structure du fichier d'Index JSON

L'application récupère la liste des plugins via une URL JSON de cette forme :

```json
{
  "plugins": [
    {
      "id": "ywb-plugin",
      "name": "YWB Plugin",
      "author": "gotaz",
      "description": "Plugin CTRPF pour Yo-kai Watch",
      "repo": "gotaz/ywb-plugin",
      "titleIds": ["00040000000A2C00"],
      "category": "game"
    }
  ]
}
```

---

## 🤝 Contribution

Les contributions et les Pull Requests sont les bienvenues !
Pour ajouter un plugin à la liste officielle, soumettez une PR modifiant le fichier `index.json`.

---

## 📜 Licence

Distribué sous la licence **MIT**. Voir `LICENSE` pour plus de détails.
