#include <3ds.h>
#include <citro2d.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "network.hpp"
#include "plugin_manager.hpp"
#include "app_updater.hpp"

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define ITEM_HEIGHT   56
#define HEADER_HEIGHT 32
#define FOOTER_HEIGHT 88
#define CARD_RADIUS   6

const std::string INDEX_URL = "https://raw.githubusercontent.com/GotazZ/3gx-updater/main/index.json";

static C2D_TextBuf s_titleBuf = nullptr;
static C2D_TextBuf s_listBuf = nullptr;
static C2D_TextBuf s_footerBuf = nullptr;
static C2D_TextBuf s_statusBuf = nullptr;
static C2D_TextBuf s_centerBuf = nullptr;

static C2D_Text s_titleText;
static C2D_Text s_versionText;
static std::vector<C2D_Text> s_nameTexts;
static std::vector<C2D_Text> s_infoTexts;
static std::vector<C2D_Text> s_catTexts;
static C2D_Text s_footerText;
static C2D_Text s_centerText;

static std::vector<PluginInfo> s_plugins;
static std::string s_statusMsg;
static int s_selectedIndex = 0;
static int s_scrollOffset = 0;
static bool s_loading = false;
static float s_progress = 0.0f;
static bool s_progressActive = false;
static int s_animFrame = 0;
static NetworkProgress s_dlProgress;
static bool s_updateAvailable = false;
static std::string s_updateVersion;
static std::string s_updateUrl;
static bool s_updatePendingInstall = false;

static u32 ColorLerp(u32 a, u32 b, float t) {
    int ar = (a >> 24) & 0xFF, ag = (a >> 16) & 0xFF, ab = (a >> 8) & 0xFF, aa = a & 0xFF;
    int br = (b >> 24) & 0xFF, bg = (b >> 16) & 0xFF, bb = (b >> 8) & 0xFF, ba = b & 0xFF;
    return ((u32)(ar + (br - ar) * t) << 24) |
           ((u32)(ag + (bg - ag) * t) << 16) |
           ((u32)(ab + (bb - ab) * t) << 8) |
           (u32)(aa + (ba - aa) * t);
}

static void DrawRoundedRect(float x, float y, float w, float h, float r, u32 color) {
    if (r < 0.0f) r = 0.0f;
    if (r > w / 2.0f) r = w / 2.0f;
    if (r > h / 2.0f) r = h / 2.0f;
    float innerX = x + r;
    float innerY = y + r;
    float innerW = w - r * 2.0f;
    float innerH = h - r * 2.0f;
    if (innerW <= 0.0f || innerH <= 0.0f) {
        C2D_DrawRectSolid(x, y, 0, w, h, color);
        return;
    }
    C2D_DrawRectSolid(innerX, y, 0, innerW, r, color);
    C2D_DrawRectSolid(innerX, y + h - r, 0, innerW, r, color);
    C2D_DrawRectSolid(x, innerY, 0, r, innerH, color);
    C2D_DrawRectSolid(x + w - r, innerY, 0, r, innerH, color);
    C2D_DrawRectSolid(innerX, innerY, 0, innerW, innerH, color);
}

static void DrawShadow(float x, float y, float w, float h, float r, u32 shadowColor) {
    DrawRoundedRect(x + 1.0f, y + 2.0f, w, h, r, shadowColor);
}

static const char* CategoryLabel(const std::string& cat) {
    if (cat == "game") return "JEU";
    if (cat == "system") return "SYS";
    if (cat == "plugin") return "PLG";
    if (cat == "tool") return "OUTIL";
    return "AUTRE";
}

static u32 CategoryColor(const std::string& cat) {
    if (cat == "game") return C2D_Color32(0x7E, 0x57, 0xC2, 0xFF);
    if (cat == "system") return C2D_Color32(0xE6, 0x73, 0x4A, 0xFF);
    if (cat == "plugin") return C2D_Color32(0x39, 0xBA, 0xE6, 0xFF);
    if (cat == "tool") return C2D_Color32(0x98, 0xC3, 0x79, 0xFF);
    return C2D_Color32(0x88, 0x88, 0x88, 0xFF);
}

static void rebuildListTexts() {
    if (!s_listBuf) return;
    s_nameTexts.clear();
    s_infoTexts.clear();
    s_catTexts.clear();
    s_nameTexts.reserve(s_plugins.size());
    s_infoTexts.reserve(s_plugins.size());
    s_catTexts.reserve(s_plugins.size());

    for (const auto& p : s_plugins) {
        C2D_Text nt, it, ct;
        std::string nameStr = p.name;
        std::string infoStr = "par " + p.author + (p.installed ? "  [INSTALLE]" : "");
        C2D_TextParse(&nt, s_listBuf, nameStr.c_str());
        C2D_TextParse(&it, s_listBuf, infoStr.c_str());
        C2D_TextParse(&ct, s_listBuf, CategoryLabel(p.category));
        C2D_TextOptimize(&nt);
        C2D_TextOptimize(&it);
        C2D_TextOptimize(&ct);
        s_nameTexts.push_back(nt);
        s_infoTexts.push_back(it);
        s_catTexts.push_back(ct);
    }
}

static void updateFooterText() {
    if (!s_footerBuf) return;
    std::string footerMsg = "STATUS: " + s_statusMsg + "\n\n"
        "DPAD: Naviguer    (A): Installer\n"
        "SELECT: Actualiser    (L): MAJ App    START: Quitter";
    C2D_TextParse(&s_footerText, s_footerBuf, footerMsg.c_str());
    C2D_TextOptimize(&s_footerText);
}

static void DrawLoadingSpinner(float cx, float cy, float radius, u32 color) {
    int segs = 12;
    for (int i = 0; i < segs; i++) {
        float angle1 = (float)i / segs * 2.0f * M_PI + s_animFrame * 0.15f;
        float angle2 = angle1 + (2.0f * M_PI / segs) * 0.6f;
        float alpha = 0.3f + 0.7f * (1.0f - (float)i / segs);
        u32 segColor = ColorLerp(color, C2D_Color32(0x1E, 0x1E, 0x2E, 0xFF), 1.0f - alpha);
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        C2D_DrawLine(x1, y1, segColor, x2, y2, segColor, 3.0f, 1.0f);
    }
}

static void DrawProgressBar(float x, float y, float w, float h, float progress, u32 bg, u32 fill, u32 border) {
    DrawRoundedRect(x, y, w, h, h / 2.0f, bg);
    if (progress > 0.0f) {
        float fillW = w * progress;
        if (fillW > h) {
            DrawRoundedRect(x + 1.0f, y + 1.0f, fillW - 2.0f, h - 2.0f, (h - 2.0f) / 2.0f, fill);
        }
    }
    DrawRoundedRect(x, y, w, h, h / 2.0f, border);

    if (progress > 0.0f && progress < 100.0f) {
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", progress);
        C2D_Text pctText;
        C2D_TextParse(&pctText, s_statusBuf, pctBuf);
        C2D_TextOptimize(&pctText);
        float textW = 0.0f;
        C2D_TextGetDimensions(&pctText, 0.5f, 0.5f, &textW, nullptr);
        C2D_DrawText(&pctText, C2D_WithColor, x + w / 2.0f - textW / 2.0f, y + 8, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
    }
}

static void refreshIndex() {
    s_statusMsg = "Actualisation de l'index...";
    s_plugins.clear();
    s_selectedIndex = 0;
    s_scrollOffset = 0;
    s_progressActive = false;
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
    s_listBuf = C2D_TextBufNew(24576);
    s_footerBuf = C2D_TextBufNew(4096);
    s_statusBuf = C2D_TextBufNew(2048);
    s_centerBuf = C2D_TextBufNew(4096);

    C2D_TextParse(&s_titleText, s_titleBuf, "3GX Updater");
    C2D_TextOptimize(&s_titleText);
    C2D_TextParse(&s_versionText, s_titleBuf, "v2.0");
    C2D_TextOptimize(&s_versionText);
    C2D_TextParse(&s_centerText, s_centerBuf, "");
    C2D_TextOptimize(&s_centerText);

    bool netOk = Network::init();
    if (netOk) {
        s_statusMsg = "Chargement de l'index...";
        std::string jsonContent = Network::fetchUrl(INDEX_URL);
        if (!jsonContent.empty()) {
            s_plugins = PluginManager::parseIndexJson(jsonContent);
            if (!s_plugins.empty()) {
                s_statusMsg = "Pret. Selectionnez un plugin.";
                rebuildListTexts();
            } else {
                s_statusMsg = "Aucun plugin trouve dans l'index.";
            }
        } else {
            s_statusMsg = "Erreur lors du telechargement de l'index.";
        }

        s_statusMsg = "Verification des mises a jour...";
        updateFooterText();
        AppUpdater::UpdateInfo updateInfo = AppUpdater::checkForUpdates("GotazZ/3gx-updater");
        if (updateInfo.available) {
            s_updateAvailable = true;
            s_updateVersion = updateInfo.latestVersion;
            s_updateUrl = updateInfo.downloadUrl;
            s_statusMsg = "Mise a jour v" + updateInfo.latestVersion + " disponible (L pour installer)";
        } else {
            s_statusMsg = "Application a jour (v" + std::string(AppUpdater::CURRENT_VERSION) + ")";
        }
    } else {
        s_statusMsg = "Erreur d'initialisation du reseau (SOC).";
    }
    updateFooterText();

    u32 bgColor = C2D_Color32(0x18, 0x18, 0x26, 0xFF);
    u32 cardColor = C2D_Color32(0x24, 0x24, 0x36, 0xFF);
    u32 selColor = C2D_Color32(0x35, 0x30, 0x6B, 0xFF);
    u32 selBorder = C2D_Color32(0x78, 0x70, 0xE8, 0xFF);
    u32 textColor = C2D_Color32(0xF0, 0xF0, 0xF8, 0xFF);
    u32 subColor = C2D_Color32(0x9E, 0x9E, 0xB8, 0xFF);
    u32 greenColor = C2D_Color32(0x8C, 0xE6, 0x8A, 0xFF);
    u32 shadowColor = C2D_Color32(0x08, 0x08, 0x12, 0x88);
    u32 headerColor = C2D_Color32(0x10, 0x0C, 0x28, 0xFF);
    u32 accentColor = C2D_Color32(0x8B, 0x7A, 0xFC, 0xFF);
    u32 footerBg = C2D_Color32(0x14, 0x14, 0x20, 0xFF);
    u32 progressBg = C2D_Color32(0x30, 0x30, 0x44, 0xFF);
    u32 progressFill = C2D_Color32(0x8B, 0x7A, 0xFC, 0xFF);
    u32 progressBorder = C2D_Color32(0x5A, 0x52, 0xA8, 0xFF);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        s_animFrame++;

        if (kDown & KEY_START) break;

        if (kDown & KEY_SELECT) {
            refreshIndex();
        }

        if (!s_plugins.empty() && !s_loading) {
            int maxVisible = (SCREEN_HEIGHT - HEADER_HEIGHT - 6) / ITEM_HEIGHT;

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
                s_statusMsg = "Recuperation de la release...";
                s_loading = true;
                s_progressActive = true;
                s_progress = 0.0f;
                Network::resetProgress();
                updateFooterText();

                std::string downloadUrl = PluginManager::getLatestReleaseDownloadUrl(selected.repo);
                if (!downloadUrl.empty()) {
                    s_statusMsg = "Telechargement : " + selected.name;
                    updateFooterText();
                    if (PluginManager::installPlugin(selected, downloadUrl)) {
                        selected.installed = true;
                        s_statusMsg = selected.name + " installe !";
                        rebuildListTexts();
                    } else {
                        s_statusMsg = "Echec de l'installation.";
                    }
                } else {
                    s_statusMsg = "Aucun .3gx trouve sur la release.";
                }
                s_loading = false;
                s_progressActive = false;
                s_progress = 100.0f;
                updateFooterText();
            }
        }

        if (s_progressActive) {
            s_dlProgress = Network::getProgress();
            if (s_dlProgress.dlTotal > 0) {
                s_progress = s_dlProgress.percent;
            } else {
                s_progress += 2.5f;
                if (s_progress > 90.0f) s_progress = 90.0f;
            }
        }

        if (kDown & KEY_L) {
            if (s_updatePendingInstall && s_updateAvailable) {
                s_statusMsg = "Telechargement de la mise a jour...";
                s_loading = true;
                s_progressActive = true;
                s_progress = 0.0f;
                Network::resetProgress();
                updateFooterText();

                std::string updatePath = "/3gx-updater-update.3dsx";
                if (AppUpdater::downloadUpdate(s_updateUrl, updatePath, &s_dlProgress)) {
                    s_statusMsg = "Installation de la mise a jour...";
                    updateFooterText();
                    if (AppUpdater::applyUpdate(updatePath, "/3gx-updater.3dsx")) {
                        s_statusMsg = "Mise a jour installee ! Redemarrez l'application.";
                        s_updateAvailable = false;
                        s_updatePendingInstall = false;
                    } else {
                        s_statusMsg = "Echec de l'installation de la mise a jour.";
                        s_updatePendingInstall = false;
                    }
                } else {
                    s_statusMsg = "Echec du telechargement de la mise a jour.";
                    s_updatePendingInstall = false;
                }
                s_loading = false;
                s_progressActive = false;
                s_progress = 0.0f;
                updateFooterText();
            } else {
                s_statusMsg = "Verification des mises a jour...";
                updateFooterText();
                AppUpdater::UpdateInfo updateInfo = AppUpdater::checkForUpdates("GotazZ/3gx-updater");
                if (updateInfo.available) {
                    s_updateAvailable = true;
                    s_updateVersion = updateInfo.latestVersion;
                    s_updateUrl = updateInfo.downloadUrl;
                    s_updatePendingInstall = true;
                    s_statusMsg = "Mise a jour v" + updateInfo.latestVersion + " disponible (L pour installer)";
                } else {
                    s_updateAvailable = false;
                    s_updatePendingInstall = false;
                    s_statusMsg = "Application a jour (v" + std::string(AppUpdater::CURRENT_VERSION) + ")";
                }
                updateFooterText();
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topTarget, bgColor);
        C2D_SceneBegin(topTarget);

        DrawRoundedRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, bgColor);

        DrawRoundedRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT + 4, 0.0f, headerColor);
        DrawRoundedRect(0, HEADER_HEIGHT, SCREEN_WIDTH, 4, 0.0f, accentColor);

        C2D_DrawText(&s_titleText, C2D_WithColor, 14, 6, 0.5f, 0.7f, 0.7f, textColor);
        C2D_DrawText(&s_versionText, C2D_WithColor, SCREEN_WIDTH - 38, 9, 0.5f, 0.4f, 0.4f, subColor);

        if (s_plugins.empty()) {
            float cx = SCREEN_WIDTH / 2.0f;
            float cy = SCREEN_HEIGHT / 2.0f - 10;
            DrawLoadingSpinner(cx, cy, 24.0f, accentColor);

            C2D_TextParse(&s_centerText, s_centerBuf, s_statusMsg.c_str());
            C2D_TextOptimize(&s_centerText);
            C2D_DrawText(&s_centerText, C2D_WithColor, 20, cy + 36, 0.5f, 0.5f, 0.5f, textColor);
        } else {
            int yPos = HEADER_HEIGHT + 10;
            int maxVisible = (SCREEN_HEIGHT - HEADER_HEIGHT - 6) / ITEM_HEIGHT;
            int start = s_scrollOffset;
            int end = std::min(start + maxVisible, (int)s_plugins.size());

            for (int i = start; i < end; i++) {
                float cardY = (float)yPos;
                bool selected = (i == s_selectedIndex);
                float cardW = SCREEN_WIDTH - 18;
                float cardH = ITEM_HEIGHT - 6;

                if (selected) {
                    DrawShadow(12.0f, cardY + 1.0f, cardW, cardH, CARD_RADIUS, shadowColor);
                    DrawRoundedRect(12.0f, cardY, cardW, cardH, CARD_RADIUS, selColor);
                    DrawRoundedRect(13.0f, cardY + 1.0f, cardW - 2.0f, cardH - 2.0f, CARD_RADIUS - 1.0f, ColorLerp(selColor, C2D_Color32(0x45, 0x40, 0x88, 0xFF), 0.3f));
                } else {
                    DrawShadow(12.0f, cardY + 1.0f, cardW, cardH, CARD_RADIUS, shadowColor);
                    DrawRoundedRect(12.0f, cardY, cardW, cardH, CARD_RADIUS, cardColor);
                }

                if (selected) {
                    C2D_DrawTriangle(8.0f, cardY + cardH / 2.0f - 6.0f, selBorder,
                                     8.0f, cardY + cardH / 2.0f + 6.0f, selBorder,
                                     14.0f, cardY + cardH / 2.0f, selBorder, 1.0f);
                }

                if (i < (int)s_nameTexts.size()) {
                    C2D_DrawText(&s_nameTexts[i], C2D_WithColor, 24, cardY + 7, 0.5f, 0.55f, 0.55f, textColor);
                    C2D_DrawText(&s_infoTexts[i], C2D_WithColor, 24, cardY + 27, 0.5f, 0.42f, 0.42f, s_plugins[i].installed ? greenColor : subColor);

                    if (i < (int)s_catTexts.size()) {
                        float tagX = SCREEN_WIDTH - 18 - 40.0f;
                        float tagY = cardY + 6;
                        u32 catCol = CategoryColor(s_plugins[i].category);
                        DrawRoundedRect(tagX, tagY, 36, 16, 3.0f, catCol);
                        C2D_DrawText(&s_catTexts[i], C2D_WithColor, tagX + 4, tagY + 2, 0.5f, 0.35f, 0.35f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
                    }

                    if (selected && s_progressActive) {
                        DrawProgressBar(24, cardY + 44, cardW - 30, 4, s_progress / 100.0f, progressBg, progressFill, progressBorder);
                    }
                }
                yPos += ITEM_HEIGHT;
            }

            if ((int)s_plugins.size() > maxVisible) {
                float scrollBarHeight = (float)maxVisible / s_plugins.size() * (SCREEN_HEIGHT - HEADER_HEIGHT - 10);
                float scrollBarY = HEADER_HEIGHT + 10 + (float)s_scrollOffset / s_plugins.size() * (SCREEN_HEIGHT - HEADER_HEIGHT - 10);
                C2D_DrawRectSolid(SCREEN_WIDTH - 7, scrollBarY, 0, 3, scrollBarHeight, ColorLerp(subColor, C2D_Color32(0x00, 0x00, 0x00, 0x00), 0.5f));
            }
        }

        C2D_TargetClear(bottomTarget, bgColor);
        C2D_SceneBegin(bottomTarget);

        DrawRoundedRect(6, 6, SCREEN_WIDTH - 12, FOOTER_HEIGHT - 12, 8.0f, footerBg);
        DrawRoundedRect(6, 6, SCREEN_WIDTH - 12, 3, 8.0f, accentColor);

        C2D_DrawText(&s_footerText, C2D_WithColor, 16, 16, 0.5f, 0.5f, 0.5f, textColor);

        if (s_loading) {
            DrawLoadingSpinner(SCREEN_WIDTH - 36, FOOTER_HEIGHT - 22, 10.0f, accentColor);
        }

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(s_statusBuf);
    C2D_TextBufDelete(s_footerBuf);
    C2D_TextBufDelete(s_listBuf);
    C2D_TextBufDelete(s_centerBuf);
    C2D_TextBufDelete(s_titleBuf);
    Network::exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
