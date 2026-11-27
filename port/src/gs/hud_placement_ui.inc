struct HudPlacementItem {
    const char *key, *en, *es, *pt;
    float x, y;
};

static constexpr HudPlacementItem hudPlacementItems[] = {
    {},
    {"gauge1","P1 gauges","Barras J1","Barras J1",90,24},
    {"gauge2","P2 gauges","Barras J2","Barras J2",422,24},
    {"team1","P1 team","Equipo J1","Equipe J1",42,85},
    {"team2","P2 team","Equipo J2","Equipe J2",470,85},
    {"clock","Battle timer","Reloj de combate","Cronômetro",256,25},
    {"prompt1","P1 move prompts","Indicaciones J1","Comandos J1",87,303},
    {"prompt2","P2 move prompts","Indicaciones J2","Comandos J2",425,303},
    {"combo1","P1 combo","Combo J1","Combo J1",87,160},
    {"combo2","P2 combo","Combo J2","Combo J2",425,160},
    {"notice","Battle announcements","Avisos de combate","Avisos de combate",256,185},
    {"caption","Technique captions","Nombres de técnicas","Nomes de técnicas",256,100},
    {"online1","P1 profile card","Tarjeta online J1","Cartão online J1",133,63},
    {"online2","P2 profile card","Tarjeta online J2","Cartão online J2",379,63},
    {"taggauge1","P1 Tag gauges","Barras Tag J1","Barras Tag J1",90,24},
    {"taggauge2","P2 Tag gauges","Barras Tag J2","Barras Tag J2",422,24},
    {"allies1","P1 allies","Aliados J1","Aliados J1",42,393},
    {"allies2","P2 allies","Aliados J2","Aliados J2",470,393},
    {"radar","Tag radar","Radar Tag","Radar Tag",468,44},
    {"target","Target indicator","Indicador de objetivo","Indicador de alvo",256,208},
    {"assist1","P1 support","Asistencia J1","Assistência J1",65,251},
    {"assist2","P2 support","Asistencia J2","Assistência J2",319,251},
    {"tagclock","Tag clock / score","Reloj y marcador Tag","Relógio e placar Tag",405,18},
    {"revival","Revival indicators","Indicadores de reanimación","Indicadores de reanimação",256,271},
    {"tagonline1","P1 Tag profile card","Tarjeta Tag J1","Cartão Tag J1",103,84},
    {"tagonline2","P2 Tag profile card","Tarjeta Tag J2","Cartão Tag J2",322,84},
    {"tagspec","Tag spectator controls","Controles de espectador Tag","Controles do espectador Tag",88,124},
    {"spar","Training while waiting","Entrenamiento en espera","Treino durante a busca",256,416}
};
static_assert(sizeof(hudPlacementItems)/sizeof(*hudPlacementItems)==HUD_PLACE_COUNT);

// Both axes travel together from the settings UI to the renderer.
static std::atomic<int> hudPlacementPacked[HUD_PLACE_COUNT];
static bool hudPlacementOpen=false;

static void hud_placement_key(int group,char (&key)[24]) {
    SDL_snprintf(key,sizeof(key),"hud_pos_%s",hudPlacementItems[group].key);
}

static void hud_placement_load() {
    static const bool loaded=[]{
        for(int i=1;i<HUD_PLACE_COUNT;i++){
            char key[24];hud_placement_key(i,key);
            hudPlacementPacked[i].store(std::clamp(Port_Setting(key,512*1024+512),0,1024*1024-1));
        }
        return true;
    }();
    (void)loaded;
}

extern "C" void Port_HudGetOffset(int group,int *x,int *y) {
    hud_placement_load();
    const int value=hudPlacementPacked[group].load();
    *x=value%1024-512;
    *y=value/1024-512;
}

static ImVec2 hud_placement_position(int group,ImVec2 p,float sx,float sy) {
    int x,y;Port_HudGetOffset(group,&x,&y);
    return ImVec2(p.x+x*sx,p.y+y*sy);
}

static void hud_placement_save(int group) {
    char key[24];hud_placement_key(group,key);
    Port_SettingSave(key,hudPlacementPacked[group].load());
    Port_SettingsWrite();
}

static void hud_placement_editor() {
    static int group=HUD_PLACE_GAUGE_1;
    static ImVec2 dragStart;
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    const bool battle=GsDraw_HudShown()&&!gPortMenuMode;
    const char *title=profile_label("Customize HUD","Personalizar HUD","Personalizar HUD");
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y-8),ImGuiCond_Always,ImVec2(0.5f,1));
    ImGui::Begin(title,&hudPlacementOpen,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    const auto label=[](int i){
        const auto &item=hudPlacementItems[i];
        return profile_label(item.en,item.es,item.pt);
    };
    ImGui::SetNextItemWidth(fminf(display.x*0.50f,340));
    if(ImGui::BeginCombo("##hud-element",label(group))){
        for(int i=1;i<HUD_PLACE_COUNT;i++)
            if(ImGui::Selectable(label(i),i==group))group=i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button(profile_label("Reset element","Restaurar elemento","Restaurar elemento"))){
        hudPlacementPacked[group].store(512*1024+512);
        hud_placement_save(group);
    }
    ImGui::SameLine();
    if(ImGui::Button(profile_label("Done","Listo","Concluir")))hudPlacementOpen=false;
    ImGui::TextUnformatted(battle?
        profile_label("Drag the selected element. Changes save when you release it.","Arrastra el elemento. Los cambios se guardan al soltarlo.","Arraste o elemento. As alterações são salvas ao soltar."):
        profile_label("Layout preview. Open during a battle to see the live HUD.","Vista del diseño. Ábrelo en combate para ver el HUD en vivo.","Prévia do layout. Abra em combate para ver o HUD ao vivo."));
    const float controlsTop=ImGui::GetWindowPos().y;
    ImGui::End();

    const float width=gUiPresentW>0?float(gUiPresentW):display.x;
    const float height=gUiPresentH>0?float(gUiPresentH):display.y;
    const float canvasBottom=fminf(float(gUiPresentY)+height,controlsTop-4);
    if(canvasBottom<=gUiPresentY+24)return;
    const ImVec2 origin{float(gUiPresentX),float(gUiPresentY)};
    const float sx=width/512,sy=height/448;
    const auto &item=hudPlacementItems[group];
    const ImVec2 center=hud_placement_position(group,ImVec2(origin.x+item.x*sx,origin.y+item.y*sy),sx,sy);
    if(width<24)return;
    const ImVec2 size(fminf(width-12,210),34);
    const ImVec2 pos(std::clamp(center.x-size.x/2,origin.x+2,origin.x+width-size.x-2),
                     std::clamp(center.y-size.y/2,origin.y,fmaxf(origin.y,canvasBottom-size.y)));
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("##hud-placement-handle",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushID(group);
    ImGui::InvisibleButton("element",size);
    if(ImGui::IsItemActivated()){
        int x,y;Port_HudGetOffset(group,&x,&y);
        dragStart=ImVec2(float(x),float(y));
    }
    if(ImGui::IsItemActive()){
        const ImVec2 delta=ImGui::GetMouseDragDelta(ImGuiMouseButton_Left,0);
        const int x=int(std::clamp(dragStart.x+delta.x/sx,-item.x,512-item.x));
        const int y=int(std::clamp(dragStart.y+delta.y/sy,-item.y,448-item.y));
        hudPlacementPacked[group].store((y+512)*1024+x+512);
    }
    if(ImGui::IsItemDeactivated())hud_placement_save(group);
    ImDrawList *dl=ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x+size.x,pos.y+size.y);
    dl->AddRectFilled(pos,end,IM_COL32(12,33,49,224),5);
    dl->AddRect(pos,end,IM_COL32(255,205,107,255),5);
    dl->AddText(ImVec2(pos.x+8,pos.y+8),IM_COL32(251,236,199,255),label(group));
    ImGui::PopID();
    ImGui::End();
    ImGui::PopStyleVar();
}
