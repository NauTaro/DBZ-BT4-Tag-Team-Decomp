struct DragonBallNotice {
    char finder[20];
    unsigned stars;
    Uint64 started;
};

static SDL_SpinLock sDragonBallLock;
static DragonBallNotice sDragonBallNotice;
static constexpr Uint64 dragonBallNoticeDuration = 4200;

extern "C" void Port_UiDragonBallFound(const char *finder, unsigned stars) {
    SDL_LockSpinlock(&sDragonBallLock);
    SDL_strlcpy(sDragonBallNotice.finder, finder, sizeof(sDragonBallNotice.finder));
    sDragonBallNotice.stars = stars;
    sDragonBallNotice.started = SDL_GetTicks();
    SDL_UnlockSpinlock(&sDragonBallLock);
}

static DragonBallNotice dragon_ball_notice() {
    SDL_LockSpinlock(&sDragonBallLock);
    const DragonBallNotice notice = sDragonBallNotice;
    SDL_UnlockSpinlock(&sDragonBallLock);
    return notice;
}

static void dragon_ball_star(ImDrawList *draw, ImVec2 center, float radius, ImU32 color) {
    const float angle = 3.14159265f / 5.0f;
    for (int p = 0; p < 5; ++p) {
        float tip = p * 2.0f * angle - 2.5f * angle;
        ImVec2 a(center.x + cosf(tip) * radius, center.y + sinf(tip) * radius);
        ImVec2 b(center.x + cosf(tip + angle) * radius * .44f, center.y + sinf(tip + angle) * radius * .44f);
        ImVec2 c(center.x + cosf(tip - angle) * radius * .44f, center.y + sinf(tip - angle) * radius * .44f);
        draw->AddQuadFilled(c, a, b, center, color);
    }
}

static void dragon_ball_notice_draw(const DragonBallNotice &notice, Uint64 elapsed) {
    const float in = ImClamp(elapsed / 480.0f, 0.0f, 1.0f);
    const float out = ImClamp((dragonBallNoticeDuration - elapsed) / 480.0f, 0.0f, 1.0f);
    const float travel = 1.0f - (1.0f - in) * (1.0f - in) * (1.0f - in);
    const float opacity = travel * out * out * (3.0f - 2.0f * out);
    const float scale = ImClamp(ImMin(gUiPresentH / 720.0f, gUiPresentW / 940.0f), .45f, 2.5f);
    const auto color = [opacity](int r, int g, int b, int a) {
        return IM_COL32(r, g, b, (int)(a * opacity));
    };
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const char *title = PortOnline_Text("Dragon Ball found");
    const float titleSize = 17.0f * scale;
    const float nameSize = 24.0f * scale;
    const float titleW = font->CalcTextSizeA(titleSize, FLT_MAX, 0, title).x;
    const float nameW = font->CalcTextSizeA(nameSize, FLT_MAX, 0, notice.finder).x;
    const float width = ImMax(226.0f * scale, ImMax(titleW, nameW) + 108.0f * scale);
    const float x = gUiPresentX + (gUiPresentW - width) * .5f;
    const float y = gUiPresentY + gUiPresentH * .224f - (1.0f - travel) * 22.0f * scale;
    const float h = 64.0f * scale;
    const ImVec2 tl(x, y), br(x + width, y + h);
    const float corner = 14.0f * scale;
    draw->AddRectFilled(ImVec2(x + scale, y + 4.0f * scale),
                        ImVec2(br.x + scale, br.y + 4.0f * scale), color(2, 7, 16, 134), corner);
    draw->AddRectFilled(tl, br, color(11, 28, 38, 243), corner);
    draw->AddRect(tl, br, color(244, 178, 69, 190), corner);
    draw->AddLine(ImVec2(x + 70.0f * scale, y + 9.0f * scale),
                  ImVec2(x + 70.0f * scale, br.y - 9.0f * scale), color(224, 167, 69, 100), scale);
    const ImVec2 mark(x + 36.0f * scale, y + .5f * h);
    const float pulse = sinf(in * 3.14159265f);
    const float r = (20.0f + 2.0f * pulse) * scale;
    draw->AddCircleFilled(mark, r + (6.0f + 3.0f * pulse) * scale, color(255, 145, 34, 55));
    draw->AddCircleFilled(mark, r, color(202, 80, 19, 255), 48);
    for (int i = 1; i <= 12; ++i) {
        const float blend = i / 12.0f;
        const ImVec2 highlight(mark.x - r * blend * .30f, mark.y - r * blend * .31f);
        draw->AddCircleFilled(highlight, r * (1.0f - blend * .80f),
                              color(255, 133 + (int)(110 * blend), 30 + (int)(100 * blend), 255), 40);
    }
    draw->AddCircle(mark, r, color(255, 220, 122, 255), 48, scale);
    for (unsigned i = 0; i < notice.stars; ++i) {
        ImVec2 point = mark;
        if (notice.stars > 1 && !(notice.stars == 7 && i == 6)) {
            const float angle = i * 6.2831853f / ImMin(notice.stars, 6u) - 1.5707963f;
            point.x += cosf(angle) * r * .54f;
            point.y += sinf(angle) * r * .54f;
        }
        dragon_ball_star(draw, point, (notice.stars > 4 ? 3.6f : 4.8f) * scale, color(158, 32, 20, 255));
    }
    const float textX = x + 83.0f * scale;
    draw->AddText(font, titleSize, ImVec2(textX, y + 9.0f * scale), color(255, 207, 99, 255), title);
    draw->AddText(font, nameSize, ImVec2(textX, y + 29.0f * scale), color(255, 249, 227, 255), notice.finder);
    const float life = 1.0f - elapsed / (float)dragonBallNoticeDuration;
    const float endX = ImClamp(textX + (br.x - textX - 13.0f * scale) * life, textX, br.x);
    draw->AddLine(ImVec2(textX, br.y - 7.0f * scale), ImVec2(endX, br.y - 7.0f * scale),
                  color(247, 165, 49, 155), scale);
    const float burst = ImClamp(elapsed / 1050.0f, 0.0f, 1.0f);
    for (int i = 0; i < 7 && burst < 1.0f; ++i) {
        const float a = -.73f + i * .897598f;
        const float travelRadius = r + (5.0f + 25.0f * burst) * scale;
        ImVec2 star(mark.x + cosf(a) * travelRadius, mark.y + sinf(a) * travelRadius);
        dragon_ball_star(draw, star, (3.8f - 2.4f * burst) * scale, color(255, 228, 143, (int)(225 * (1.0f - burst))));
    }
}
