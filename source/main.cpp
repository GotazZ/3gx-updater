#include <3ds.h>
#include <citro2d.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include "network.hpp"
#include "plugin_manager.hpp"

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define ITEM_HEIGHT   48
#define HEADER_HEIGHT 28
#define FOOTER_HEIGHT 80

const std::string INDEX_URL = "https://raw.githubusercontent.com/gotaz/3gx-updater/main/index.json";

static C2D_TextBuf s_titleBuf = nullptr;
static C2D_TextBuf s_listBuf = nullptr;
static C2D_TextBuf s_statusBuf = nullptr;
static C2D_TextBuf s_centerBuf = nullptr;

static C2D_Text s_titleText;
static C2D_Text s_centerText;
static std::vector<C2D_Text> s_nameTexts;
static std::vector<C2D_Text> s_infoTexts;
static C2D_Text s_footerText;

static std::vector<PluginInfo> s_plugins;
static std::string s_statusMsg;
static int s_selectedIndex = 0;
static int s_scrollOffset = 0;
static bool s_loading = false;

static void rebuildListTexts() {
    if (!s_listBuf) return;
    s_nameTexts.clear();
    s_infoTexts.clear();
    s_nameTexts.reserve(s_plugins.size());
    s_infoTexts.reserve(s_plugins.size());

    for (const auto& p : s_plugins) {
        C2D_Text nt, it;
        std::string nameStr = p.name + " (v. latest)";
        std::string infoStr = "par " + p.author + (p.installed ? " [INSTALLE]" : "");
        C2D_TextParse(&nt, s_listBuf, nameStr.c_str());
        C2D_TextParse(&it, s_listBuf, infoStr.c_str());
        C2D_TextOptimize(&nt);
        C2D_TextOptimize(&it);
        s_nameTexts.push_back(nt);
        s_infoTexts.push_back(it);
    }
}

static void updateFooterText() {
    if (!s_statusBuf) return;
    std::string footerMsg = "Status: " + s_statusMsg + "\n\n"
        "Controles:\n"
        "DPAD: Naviguer | (A): Installer\n"
        "SELECT: Rafraichir | START: Quitter";
    C2D_TextParse(&s_footerText, s_statusBuf, footerMsg.c_str());
    C2D_TextOptimize(&s_footerText);
}

static void refreshIndex() {
    s_statusMsg = "Actualisation de l'index...";
    s_plugins.clear();
    s_selectedIndex = 0;
    s_scrollOffset = 0;
    updateFooterText();

    std::string jsonContent = Network::fetchUrl(INDEX_URL);
    if (!jsonContent.empty()) {
        s_plugins = PluginManager::parseIndexJson(jsonContent);
        if (!s_plugins.empty()) {
            s_statusMsg = "Index actualise avec succes.";
            rebuildListTexts();
        } else {
            s_statusMsg = "Aucun plugin trouve dans l'index.";
        }
    } else {
        s_statusMsg = "Echec de l'actualisation de l'index.";
    }
    updateFooterText();
}

int main(int argc, char** argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    s_titleBuf = C2D_TextBufNew(4096);
    s_listBuf = C2D_TextBufNew(16384);
    s_statusBuf = C2D_TextBufNew(4096);
    s_centerBuf = C2D_TextBufNew(4096);

    C2D_TextParse(&s_titleText, s_titleBuf, "3GX Updater - Homebrew Manager");
    C2D_TextOptimize(&s_titleText);

    bool netOk = Network::init();
    if (netOk) {
        s_statusMsg = "Chargement de l'index...";
        std::string jsonContent = Network::fetchUrl(INDEX_URL);
        if (!jsonContent.empty()) {
            s_plugins = PluginManager::parseIndexJson(jsonContent);
            if (!s_plugins.empty()) {
                s_statusMsg = "Index charge. Appuyez sur (A) pour installer.";
                rebuildListTexts();
            } else {
                s_statusMsg = "Aucun plugin trouve dans l'index.";
            }
        } else {
            s_statusMsg = "Erreur lors du telechargement de l'index.";
        }
    } else {
        s_statusMsg = "Erreur d'initialisation du reseau (SOC).";
    }
    updateFooterText();

    u32 bgColor = C2D_Color32(0x1E, 0x1E, 0x2E, 0xFF);
    u32 cardColor = C2D_Color32(0x31, 0x32, 0x44, 0xFF);
    u32 selColor = C2D_Color32(0x45, 0x47, 0x5A, 0xFF);
    u32 textColor = C2D_Color32(0xCD, 0xD6, 0xF4, 0xFF);
    u32 greenColor = C2D_Color32(0xA6, 0xE3, 0xA1, 0xFF);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (kDown & KEY_SELECT) {
            refreshIndex();
        }

        if (!s_plugins.empty() && !s_loading) {
            int maxVisible = (SCREEN_HEIGHT - HEADER_HEIGHT) / ITEM_HEIGHT;

            if ((kDown & KEY_DDOWN) || (kDown & KEY_CPAD_DOWN)) {
                if (s_selectedIndex < (int)s_plugins.size() - 1) {
                    s_selectedIndex++;
                    if (s_selectedIndex >= s_scrollOffset + maxVisible) {
                        s_scrollOffset = s_selectedIndex - maxVisible + 1;
                    }
                }
            }
            if ((kDown & KEY_DUP) || (kDown & KEY_CPAD_UP)) {
                if (s_selectedIndex > 0) {
                    s_selectedIndex--;
                    if (s_selectedIndex < s_scrollOffset) {
                        s_scrollOffset = s_selectedIndex;
                    }
                }
            }

            if (kDown & KEY_A) {
                PluginInfo& selected = s_plugins[s_selectedIndex];
                s_statusMsg = "Recuperation de la release GitHub...";
                s_loading = true;
                updateFooterText();

                std::string downloadUrl = PluginManager::getLatestReleaseDownloadUrl(selected.repo);
                if (!downloadUrl.empty()) {
                    s_statusMsg = "Telechargement et installation de " + selected.name + "...";
                    updateFooterText();
                    if (PluginManager::installPlugin(selected, downloadUrl)) {
                        selected.installed = true;
                        s_statusMsg = selected.name + " installe avec succes !";
                        rebuildListTexts();
                    } else {
                        s_statusMsg = "Echec de l'installation.";
                    }
                } else {
                    s_statusMsg = "Aucun fichier .3gx trouve sur la release.";
                }
                s_loading = false;
                updateFooterText();
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topTarget, bgColor);
        C2D_SceneBegin(topTarget);

        C2D_DrawRectSolid(0, 0, 0, SCREEN_WIDTH, HEADER_HEIGHT, cardColor);
        C2D_DrawText(&s_titleText, C2D_WithColor, 10, 5, 0.5f, 0.6f, 0.6f, textColor);

        if (s_plugins.empty()) {
            C2D_TextParse(&s_centerText, s_centerBuf, s_statusMsg.c_str());
            C2D_TextOptimize(&s_centerText);
            C2D_DrawText(&s_centerText, C2D_WithColor, 10, 100, 0.5f, 0.5f, 0.5f, textColor);
        } else {
            int yPos = HEADER_HEIGHT + 2;
            int maxVisible = (SCREEN_HEIGHT - HEADER_HEIGHT) / ITEM_HEIGHT;
            int start = s_scrollOffset;
            int end = std::min(start + maxVisible, (int)s_plugins.size());

            for (int i = start; i < end; i++) {
                u32 currentCardColor = (i == s_selectedIndex) ? selColor : cardColor;
                C2D_DrawRectSolid(10, yPos, 0, SCREEN_WIDTH - 20, ITEM_HEIGHT - 3, currentCardColor);

                if (i < (int)s_nameTexts.size()) {
                    C2D_DrawText(&s_nameTexts[i], C2D_WithColor, 20, yPos + 5, 0.5f, 0.55f, 0.55f, textColor);
                    C2D_DrawText(&s_infoTexts[i], C2D_WithColor, 20, yPos + 26, 0.5f, 0.45f, 0.45f, s_plugins[i].installed ? greenColor : textColor);
                }
                yPos += ITEM_HEIGHT;
            }

            if (s_plugins.size() > maxVisible) {
                float scrollBarHeight = (float)maxVisible / s_plugins.size() * (SCREEN_HEIGHT - HEADER_HEIGHT);
                float scrollBarY = HEADER_HEIGHT + (float)s_scrollOffset / s_plugins.size() * (SCREEN_HEIGHT - HEADER_HEIGHT);
                C2D_DrawRectSolid(SCREEN_WIDTH - 8, scrollBarY, 0, 4, scrollBarHeight, textColor);
            }
        }

        C2D_TargetClear(bottomTarget, bgColor);
        C2D_SceneBegin(bottomTarget);

        C2D_DrawRectSolid(5, 5, 0, SCREEN_WIDTH - 10, FOOTER_HEIGHT - 10, cardColor);
        C2D_DrawText(&s_footerText, C2D_WithColor, 15, 15, 0.5f, 0.5f, 0.5f, textColor);

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(s_statusBuf);
    C2D_TextBufDelete(s_listBuf);
    C2D_TextBufDelete(s_centerBuf);
    C2D_TextBufDelete(s_titleBuf);
    Network::exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
