static const char *profile_label(const char *en, const char *es, const char *pt) {
    const int language = Port_GameProfileLanguage();
    return language == 0 ? es : language == 2 ? pt : en;
}

static void profile_frame(ImDrawList *dl,ImVec2 p,ImVec2 q,float cut,ImU32 fill,ImU32 rim,float stroke=1.0f) {
    const ImVec2 edges[]={
        {p.x+cut,p.y},{q.x-cut,p.y},{q.x,p.y+cut},{q.x,q.y-cut},
        {q.x-cut,q.y},{p.x+cut,q.y},{p.x,q.y-cut},{p.x,p.y+cut}
    };
    dl->AddConvexPolyFilled(edges,8,fill);
    dl->AddPolyline(edges,8,rim,ImDrawFlags_Closed,stroke);
}

static void profile_tbo_panel(ImDrawList *dl,ImVec2 p,ImVec2 q,float u) {
    profile_frame(dl,ImVec2(p.x+4*u,p.y+6*u),ImVec2(q.x+4*u,q.y+6*u),15*u,
                  IM_COL32(0,9,16,185),IM_COL32(3,16,24,200),2*u);
    profile_frame(dl,p,q,14*u,IM_COL32(12,43,55,250),IM_COL32(9,23,28,255),4*u);
    profile_frame(dl,ImVec2(p.x+2*u,p.y+2*u),ImVec2(q.x-2*u,q.y-2*u),13*u,
                  IM_COL32(23,64,73,250),IM_COL32(144,204,206,255),1.6f*u);
    profile_frame(dl,ImVec2(p.x+6*u,p.y+6*u),ImVec2(q.x-6*u,q.y-6*u),10*u,
                  IM_COL32(19,52,65,248),IM_COL32(87,137,139,255),u);
    const ImU32 gold=IM_COL32(248,209,109,255);
    for(int i=0;i<2;++i) {
        const float y=i?q.y-9*u:p.y+9*u;
        dl->AddLine(ImVec2(p.x+24*u,y),ImVec2(p.x+56*u,y),gold,1.5f*u);
        dl->AddLine(ImVec2(q.x-56*u,y),ImVec2(q.x-24*u,y),gold,1.5f*u);
    }
}

static void profile_heading(ImDrawList *dl,ImVec2 p,float w,float h,const char *title) {
    const float s=h/44.0f;
    profile_frame(dl,p,ImVec2(p.x+w,p.y+h),10*s,IM_COL32(17,44,56,255),IM_COL32(176,202,183,255),1.3f*s);
    const ImVec2 strip[]={{p.x+8*s,p.y+7*s},{p.x+18*s,p.y+7*s},
                          {p.x+29*s,p.y+h-7*s},{p.x+19*s,p.y+h-7*s}};
    dl->AddConvexPolyFilled(strip,4,IM_COL32(245,161,47,255));
    dl->AddLine(ImVec2(p.x+36*s,p.y+h-3*s),ImVec2(p.x+w-12*s,p.y+h-3*s),IM_COL32(218,172,76,255),1.3f*s);
    const float size=26*s;
    ImFont *font=Ui_FontBold();
    if(!font)font=ImGui::GetFont();
    dl->AddText(font,size,ImVec2(p.x+41*s,p.y+(h-size)*0.5f),IM_COL32(255,234,178,255),title);
}

struct ProfileTexture {
    char id[37]={};
    SDL_GPUTexture *texture=nullptr;
    int used=-1;
    int height=0,count=0,delays[6]={};
};

static const ProfileTexture *profile_texture(const char *id) {
    static ProfileTexture cache[48];
    if (!*id) return nullptr;
    const int now=ImGui::GetFrameCount();
    ProfileTexture *slot=&cache[0];
    for (auto &entry:cache) {
        if (entry.texture&&strcmp(entry.id,id)==0) {
            entry.used=now;
            return &entry;
        }
        if (entry.used<slot->used) slot=&entry;
    }
    if (slot->used==now) return nullptr;
    uint8_t pixels[24576];
    int width=0,height=0,count=0,delays[6]={};
    if (!Port_ProfileMediaRead(id,pixels,&width,&height,&count,delays)) return nullptr;
    const size_t bytes=size_t(width)*height*count*4;
    uint8_t *upload=(uint8_t *)malloc(bytes);
    if (!upload) return nullptr;
    memcpy(upload,pixels,bytes);
    SDL_GPUTexture *texture=make_strip(upload,bytes,width,height,count);
    if (!texture) return nullptr;
    Ui_TextureFree(slot->texture);
    *slot={};
    SDL_strlcpy(slot->id,id,sizeof(slot->id));
    slot->texture=texture;slot->used=now;slot->height=height;slot->count=count;
    memcpy(slot->delays,delays,sizeof(delays));
    return slot;
}

static void profile_image_draw(ImDrawList *dl,const ProfileTexture &asset,ImVec2 p,ImVec2 q,float rounding,ImU32 tint) {
    int cycle=0;
    for (int i=0;i<asset.count;i++) cycle+=asset.delays[i];
    int time=cycle?int(SDL_GetTicks()%Uint64(cycle)):0,frame=0;
    while (frame+1<asset.count&&time>=asset.delays[frame]) time-=asset.delays[frame++];
    const float inset=0.5f/(asset.height*asset.count);
    const float v0=float(frame)/asset.count+inset,v1=float(frame+1)/asset.count-inset;
    dl->AddImageRounded(ImTextureRef((ImTextureID)(intptr_t)asset.texture),p,q,ImVec2(0,v0),ImVec2(1,v1),tint,rounding);
}

static constexpr ImU32 profileBannerColors[] = {
        IM_COL32(94, 47, 37, 255), IM_COL32(27, 58, 92, 255),
        IM_COL32(65, 39, 78, 255), IM_COL32(24, 75, 65, 255),
        IM_COL32(64, 68, 78, 255), IM_COL32(18, 25, 37, 255),
        IM_COL32(33, 93, 104, 255), IM_COL32(100, 67, 25, 255)
};
static constexpr ImU32 profileNameColors[] = {
        IM_COL32(255, 246, 228, 255), IM_COL32(255, 182, 114, 255), IM_COL32(148, 225, 255, 255),
        IM_COL32(201, 246, 151, 255), IM_COL32(241, 182, 255, 255), IM_COL32(255, 233, 152, 255),
        IM_COL32(255, 168, 166, 255), IM_COL32(202, 218, 232, 255)
};
static void profile_card_draw(ImDrawList *dl, ImVec2 p, float w, float h, const char *name, const Bt4ProfileCard &card) {
    const ImU32 ink = profileNameColors[card.nameColor];
    const ImVec2 bottom(p.x + w, p.y + h);
    const float cut=h*0.17f;
    profile_frame(dl,p,bottom,cut,IM_COL32(9,32,40,255),IM_COL32(4,17,24,255),h*0.06f);
    const ImVec2 inner(p.x+h*0.05f,p.y+h*0.05f),end(bottom.x-h*0.05f,bottom.y-h*0.05f);
    profile_frame(dl,inner,end,cut*0.78f,profileBannerColors[card.background],IM_COL32(163,217,217,230),h*0.023f);
    const auto *banner=profile_texture(card.bannerMotion);
    if(!banner)banner=profile_texture(card.banner);
    if (banner)
        profile_image_draw(dl,*banner,inner,end,cut*0.90f,IM_COL32(151,157,157,180));
    dl->AddLine(ImVec2(p.x+cut,p.y+h*0.09f),ImVec2(bottom.x-cut,p.y+h*0.09f),IM_COL32(217,249,239,148),h*0.018f);

    const ImVec2 portrait(p.x + h * 0.56f, p.y + h * 0.50f);
    const float r=h*0.35f;
    profile_frame(dl,ImVec2(portrait.x-r-h*0.04f,portrait.y-r-h*0.04f),
                  ImVec2(portrait.x+r+h*0.04f,portrait.y+r+h*0.04f),h*0.12f,
                  IM_COL32(11,34,49,255),card.premium?IM_COL32(250,206,119,255):IM_COL32(149,212,220,240),h*0.024f);
    const auto *photo=profile_texture(card.motion);
    if (!photo) photo=profile_texture(card.avatar);
    if (photo) {
        profile_image_draw(dl,*photo,ImVec2(portrait.x-r,portrait.y-r),ImVec2(portrait.x+r,portrait.y+r),h*0.10f,IM_COL32_WHITE);
    } else {
        dl->AddCircleFilled(portrait, h * 0.25f, IM_COL32(255, 238, 214, 36));
        dl->AddCircleFilled(ImVec2(portrait.x, portrait.y - h * 0.06f), h * 0.068f, ink);
        dl->AddRectFilled(ImVec2(portrait.x - h * 0.12f, portrait.y + h * 0.025f),
            ImVec2(portrait.x + h * 0.12f, portrait.y + h * 0.13f), ink, h * 0.05f);
    }

    const float textSize = h * 0.48f;
    const ImVec2 clipFrom(p.x + h * 1.05f, p.y);
    const ImVec2 clipTo(bottom.x - h * (card.decoration ? 0.56f : 0.16f), bottom.y);
    if (clipTo.x > clipFrom.x) {
        dl->PushClipRect(clipFrom, clipTo, true);
        const float ty=p.y+(h-textSize)*0.5f;
        dl->AddText(ImGui::GetFont(),textSize,ImVec2(clipFrom.x+1.0f,ty+1.5f),IM_COL32(1,14,22,245),name);
        dl->AddText(ImGui::GetFont(),textSize,ImVec2(clipFrom.x,ty),ink,name);
        dl->PopClipRect();
    }
    if (card.decoration) {
        const ImVec2 center(bottom.x - h * 0.30f, p.y + h * 0.50f);
        const float radius = h * 0.15f;
        const ImU32 gold = IM_COL32(255, 202, 130, 255);
        if (card.decoration == 1)
            dl->AddQuadFilled(ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
                ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y), gold);
        else if (card.decoration == 2) {
            dl->AddCircle(center, radius, gold, 0, h * 0.035f);
            dl->AddCircleFilled(center, radius * 0.42f, gold);
        } else if(card.decoration==3)
            dl->AddTriangleFilled(ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y + radius),
                ImVec2(center.x - radius, center.y + radius), gold);
        else if(card.decoration==4) {
            dl->AddRectFilled(ImVec2(center.x-radius,center.y-radius*0.6f),ImVec2(center.x+radius,center.y+radius*0.6f),gold,radius*0.5f);
            dl->AddLine(ImVec2(center.x,center.y-radius*0.55f),ImVec2(center.x,center.y+radius*0.55f),IM_COL32(74,53,26,255),h*0.027f);
        } else if(card.decoration==5) {
            ImVec2 star[10];
            for(int i=0;i<10;++i){
                const float angle=0.62831853f*i-1.5707963f,r=i&1?radius*0.47f:radius;
                star[i]=ImVec2(center.x+cosf(angle)*r,center.y+sinf(angle)*r);
            }
            for(int i=0;i<10;++i)dl->AddTriangleFilled(center,star[i],star[(i+1)%10],gold);
        } else {
            dl->AddCircleFilled(center,radius,gold);
            dl->AddLine(ImVec2(center.x-radius*0.5f,center.y),ImVec2(center.x+radius*0.5f,center.y),IM_COL32(74,53,26,255),h*0.032f);
        }
    }
}

static void profile_card_player(ImDrawList *dl, ImVec2 p, float w, float h, const char *name) {
    Bt4ProfileCard card={};
    Port_ProfileCardForPlayer(name, &card);
    profile_card_draw(dl, p, w, h, name, card);
}

static void profile_combat_card(ImDrawList *dl, float anchor, float y, float width, float h,
                                const char *name, const Bt4ProfileCard &card, bool mirror, int ping,
                                bool showLatency=true) {
    const float badgeWidth=showLatency?h*2.12f:0.0f;
    if (width < h*2.7f+badgeWidth)
        return;

    ImFont *font=Ui_FontBold();
    if (!font) font=ImGui::GetFont();
    const float x=mirror?anchor-width:anchor;
    const float right=x+width,cy=y+h*0.50f;
    const float avatarSize=h*0.90f;
    const float avatarCenter=mirror?right-h*0.54f:x+h*0.54f;
    const float bandLeft=mirror?x+h*0.13f:x+h*0.71f;
    const float bandRight=mirror?right-h*0.71f:right-h*0.13f;
    const ImU32 silver=IM_COL32(214,230,229,255),gold=IM_COL32(255,203,110,255);
    const ImU32 rim=card.premium?gold:silver;

    // A low rail echoes the health gauge. Both badges use the same placement and width.
    dl->AddRectFilled(ImVec2(bandLeft,y+h*0.22f),ImVec2(bandRight,y+h*0.87f),
                      IM_COL32(11,22,34,225),h*0.13f);
    dl->AddRectFilledMultiColor(ImVec2(bandLeft+h*0.11f,y+h*0.28f),ImVec2(bandRight-h*0.11f,y+h*0.76f),
                      IM_COL32(43,66,81,240),IM_COL32(43,66,81,240),
                      IM_COL32(17,28,43,240),IM_COL32(17,28,43,240));
    const ImVec2 ribbonStart(bandLeft+h*0.15f,y+h*0.77f),ribbonEnd(bandRight-h*0.15f,y+h*0.86f);
    dl->AddRectFilled(ribbonStart,ribbonEnd,profileBannerColors[card.background]);
    const auto *banner=profile_texture(card.bannerMotion);
    if (!banner) banner=profile_texture(card.banner);
    if (banner) {
        profile_image_draw(dl,*banner,ImVec2(ribbonStart.x,y+h*0.31f),ribbonEnd,
                           h*0.05f,IM_COL32(113,136,148,105));
    }
    dl->AddLine(ImVec2(ribbonStart.x,y+h*0.22f),ImVec2(ribbonEnd.x,y+h*0.22f),
                IM_COL32(202,228,231,115),h*0.029f);

    const ImVec2 avatar(avatarCenter,cy);
    dl->AddCircleFilled(avatar,avatarSize*0.56f,IM_COL32(9,16,26,255));
    dl->AddCircle(avatar,avatarSize*0.50f,rim,0,h*0.048f);
    const float radius=avatarSize*0.405f;
    const auto *photo=profile_texture(card.motion);
    if (!photo) photo=profile_texture(card.avatar);
    if (photo) {
        profile_image_draw(dl,*photo,ImVec2(avatar.x-radius,cy-radius),
                           ImVec2(avatar.x+radius,cy+radius),radius,IM_COL32_WHITE);
    } else {
        dl->AddCircleFilled(avatar,radius,profileBannerColors[card.background]);
        dl->AddCircleFilled(ImVec2(avatar.x,cy-h*0.08f),h*0.09f,silver);
        dl->AddRectFilled(ImVec2(avatar.x-h*0.15f,cy+h*0.043f),
                          ImVec2(avatar.x+h*0.15f,cy+h*0.18f),silver,h*0.06f);
    }
    if (card.decoration) {
        const float notch=mirror?avatar.x+radius*0.8f:avatar.x-radius*0.8f;
        const float bottom=cy+radius*0.84f,mark=h*0.095f;
        dl->AddQuadFilled(ImVec2(notch,bottom-mark),ImVec2(notch+mark,bottom),
                          ImVec2(notch,bottom+mark),ImVec2(notch-mark,bottom),gold);
    }

    const float badgeLeft=mirror?x+h*0.10f:right-h*0.10f-badgeWidth;
    const float nameLeft=mirror?x+(showLatency?h*0.25f+badgeWidth:h*0.25f):x+h*1.16f;
    const float nameRight=mirror?right-h*1.16f:right-(showLatency?h*0.25f+badgeWidth:h*0.25f);
    const float nameSize=h*0.49f;
    const char *nameEnd=nullptr;
    const ImVec2 nameExtent=font->CalcTextSizeA(nameSize,nameRight-nameLeft,0,name,nullptr,&nameEnd);
    if (nameEnd && nameEnd>name) {
        const float nameX=mirror?nameRight-nameExtent.x:nameLeft;
        const float nameY=cy-nameSize*0.45f;
        dl->PushClipRect(ImVec2(nameLeft,y),ImVec2(nameRight,y+h),true);
        dl->AddText(font,nameSize,ImVec2(nameX,nameY+h*0.045f),IM_COL32(2,10,22,240),name,nameEnd);
        dl->AddText(font,nameSize,ImVec2(nameX,nameY),profileNameColors[card.nameColor],name,nameEnd);
        dl->PopClipRect();
    }
    if (!showLatency)
        return;

    char latency[16];
    if (ping>=0) SDL_snprintf(latency,sizeof(latency),"%d ms",ping);
    else SDL_strlcpy(latency,"-- ms",sizeof(latency));
    const ImVec2 badgeMin(badgeLeft,y+h*0.24f),badgeMax(badgeLeft+badgeWidth,y+h*0.79f);
    profile_frame(dl,badgeMin,badgeMax,h*0.095f,IM_COL32(251,236,202,255),IM_COL32(125,91,54,255),h*0.034f);
    const ImU32 tone=ping<0?IM_COL32(110,114,120,255):ping<85?IM_COL32(47,119,66,255):
                     ping<150?IM_COL32(155,94,23,255):IM_COL32(169,57,42,255);
    const float dotX=badgeLeft+h*0.23f,dotY=(badgeMin.y+badgeMax.y)*0.5f;
    dl->AddCircleFilled(ImVec2(dotX,dotY),h*0.065f,tone,10);
    const float pingSize=h*0.37f;
    const float available=badgeWidth-h*0.47f;
    const float measured=font->CalcTextSizeA(pingSize,FLT_MAX,0,latency).x;
    const float fit=measured>available?pingSize*available/measured:pingSize;
    dl->AddText(font,fit,ImVec2(badgeLeft+h*0.38f+(available-fminf(measured,available))*0.5f,dotY-fit*0.50f),
                IM_COL32(37,30,26,255),latency);
}

static void profile_duel_cards_draw(float x,float y,float w,float h,float narrow,bool centered,bool team,bool modernHud,
                                    const char *left,const char *right,int leftPing=-1,int rightPing=-1) {
    const float sx=w/512.0f,sy=h/448.0f;
    const float in=centered?256.0f+(106.0f-256.0f)*narrow:106.0f*narrow;
    const float width=fminf(111.0f*sx,fmaxf(0.0f,(249.0f-in)*sx));
    const float row=team?(modernHud?88.0f:70.0f):(modernHud?53.0f:49.0f);
    const float cardY=y+row*sy;
    ImDrawList *dl=ImGui::GetBackgroundDrawList();
    if (*left) {
        Bt4ProfileCard card={};
        Port_ProfileCardForPlayer(left,&card);
        profile_combat_card(dl,x+in*sx,cardY,width,23.0f*sy,left,card,false,leftPing);
    }
    if (*right) {
        Bt4ProfileCard card={};
        Port_ProfileCardForPlayer(right,&card);
        profile_combat_card(dl,x+(512.0f-in)*sx,cardY,width,23.0f*sy,right,card,true,rightPing);
    }
}

static std::atomic<int> sProfilePicker{0};
static std::atomic<unsigned> sProfileCropSequence{0};
static bool sRosterCrop=false;
static void profile_crop_editor(float u) {
    static float x=0.5f,y=0.5f,zoom=1.0f;
    static SDL_GPUTexture *texture=nullptr;
    static int w=0,h=0;
    static unsigned sequence=0;
    const unsigned current=sProfileCropSequence.load();
    if(sequence!=current) {
        sequence=current;Ui_TextureFree(texture);texture=nullptr;
        x=0.5f;y=sRosterCrop?0.08f:0.5f;zoom=sRosterCrop?1.65f:1.0f;
    }
    ImGui::TextUnformatted(profile_label("Frame your image","Encuadra tu imagen","Enquadre sua imagem"));
    ImGui::TextDisabled("%s",profile_label("Drag the preview to reposition it.","Arrastra la vista previa para mover la imagen.",
        "Arraste a prévia para reposicionar a imagem."));
    ImGui::SetNextItemWidth(fminf(320*u,ImGui::GetContentRegionAvail().x*0.68f));
    bool update=ImGui::SliderFloat(profile_label("Zoom","Zoom","Zoom"),&zoom,1.0f,3.0f,"%.2fx");
    ImGui::SetNextItemWidth(fminf(320*u,ImGui::GetContentRegionAvail().x*0.68f));
    update|=ImGui::SliderFloat(profile_label("Horizontal","Horizontal","Horizontal"),&x,0.0f,1.0f,"%.2f");
    ImGui::SetNextItemWidth(fminf(320*u,ImGui::GetContentRegionAvail().x*0.68f));
    update|=ImGui::SliderFloat(profile_label("Vertical","Vertical","Vertical"),&y,0.0f,1.0f,"%.2f");
    static bool dragged=false;
    update|=dragged;
    dragged=false;
    if (!texture||update) {
        auto *rgba=(uint8_t *)malloc(128*128*4);
        if(rgba&&Port_ProfileDraftPreview(x,y,zoom,rgba,&w,&h)) {
            auto *replacement=make_strip(rgba,size_t(w)*h*4,w,h,1);
            if(replacement){Ui_TextureFree(texture);texture=replacement;}
        }else free(rgba);
    }
    if(texture) {
        const float width=fminf((w>h?340:150)*u,ImGui::GetContentRegionAvail().x);
        const ImVec2 p=ImGui::GetCursorScreenPos(),viewport(width,width*h/w);
        ImGui::InvisibleButton("##drag-photo",viewport);
        ImGui::GetWindowDrawList()->AddImage(ImTextureRef((ImTextureID)(intptr_t)texture),p,ImGui::GetItemRectMax());
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta=ImGui::GetIO().MouseDelta;
            x=fminf(1.0f,fmaxf(0.0f,x-delta.x/viewport.x));
            y=fminf(1.0f,fmaxf(0.0f,y-delta.y/viewport.y));
            dragged=true;
        }
    }
    const bool cancel=ImGui::Button(profile_label("Cancel","Cancelar","Cancelar"));
    ImGui::SameLine();
    const bool use=ImGui::Button(profile_label("Apply image","Aplicar imagen","Aplicar imagem"));
    if(cancel||use) {
        sProfilePicker=cancel||Port_ProfileDraftUpload(x,y,zoom)?0:-1;
        Port_ProfileDraftReset();
        // ImGui still references this frame's image. Free it on entry to the next crop.
        sRosterCrop=false;
    }
}
static void SDLCALL profile_media_selected(void *kind,const char *const *paths,int) {
    const bool selected=paths&&*paths;
    const bool success=selected&&Port_ProfileDraftFile((const char *)kind,*paths);
    if(success)++sProfileCropSequence;
    sProfilePicker=success?2:(!selected&&paths)?0:-1;
}

static void profile_roster_picker(float u) {
    struct Entry {int id;char name[80];};
    static Entry entries[256];
    static int count=0;
    static ImGuiTextFilter filter;
    if(ImGui::Button(profile_label("Choose BT4 character","Elegir personaje de BT4","Escolher personagem do BT4"))) {
        filter.Clear();count=0;
        for(int id=0;id<256;++id)if(Port_ProfileRosterName(id,entries[count].name,sizeof(entries[count].name)))
            entries[count++].id=id;
        ImGui::OpenPopup("##bt4-roster-photo");
    }
    ImGui::SetNextWindowSize(ImVec2(320*u,0),ImGuiCond_Appearing);
    if(ImGui::BeginPopup("##bt4-roster-photo")) {
        filter.Draw(profile_label("Search","Buscar","Buscar"),-1);
        if(ImGui::BeginChild("##characters",ImVec2(0,224*u))) {
            for(int i=0;i<count;++i)if(filter.PassFilter(entries[i].name)) {
                ImGui::PushID(entries[i].id);
                if(ImGui::Selectable(entries[i].name)) {
                    sRosterCrop=true;
                    const bool ready=Port_ProfileDraftRoster(entries[i].id)!=0;
                    if(ready)++sProfileCropSequence;
                    sProfilePicker=ready?2:-1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}

static void profile_media_control(const char *kind,const char *label,const char *id,bool animated,float u) {
    static const SDL_DialogFileFilter photo[]={{"PNG / JPEG","png;jpg;jpeg"}};
    static const SDL_DialogFileFilter gif[]={{"GIF","gif"}};
    static const SDL_DialogFileFilter anyAvatar[]={{"PNG / JPEG / GIF","png;jpg;jpeg;gif"}};
    const bool premiumAvatar=strcmp(kind,"avatar_auto")==0;
    ImGui::PushID(kind);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(172*u);
    const char *pick=*id?profile_label("Change","Cambiar","Alterar"):profile_label("Upload","Subir","Enviar");
    if (ImGui::Button(pick,ImVec2(88*u,0))) {
        sRosterCrop=false;
        sProfilePicker=1;
        SDL_ShowOpenFileDialog(profile_media_selected,const_cast<char *>(kind),SDL_GetKeyboardFocus(),
            premiumAvatar?anyAvatar:animated?gif:photo,1,nullptr,false);
    }
    if (*id) {
        ImGui::SameLine();
        if (ImGui::Button(profile_label("Remove","Quitar","Remover"))) Port_ProfileMediaSave(premiumAvatar?"avatar":kind,nullptr);
    }
    ImGui::PopID();
}

static void profile_card_editor(const char *name,float u,bool &open) {
    if(sProfilePicker==2){profile_crop_editor(u);return;}
    static Bt4ProfileCard draft={};
    static int previous=0;
    Bt4ProfileCard current={};
    const int state=Port_ProfileCardView(&current);
    if (!state) Port_ProfileCardRequest(nullptr);
    if (state==2 && (state!=previous || draft.premium!=current.premium)) draft=current;
    previous=state;
    if (state==1 || !state) {
        ImGui::TextUnformatted(profile_label("Loading profile…", "Cargando perfil…", "Carregando perfil…"));
        return;
    }
    if (state<0) {
        ImGui::TextUnformatted(profile_label("Profile unavailable", "Perfil no disponible", "Perfil indisponível"));
        if (ImGui::Button(profile_label("Try again", "Reintentar", "Tentar novamente"))) Port_ProfileCardRequest(nullptr);
        return;
    }
    const char *nameColors[] = {
        profile_label("White","Blanco","Branco"), profile_label("Orange","Naranja","Laranja"),
        profile_label("Blue","Azul","Azul"), profile_label("Green","Verde","Verde"),
        profile_label("Purple","Morado","Roxo"), profile_label("Yellow","Amarillo","Amarelo"),
        profile_label("Red","Rojo","Vermelho"), profile_label("Gray","Gris","Cinza")
    };
    const char *backgrounds[] = {
        profile_label("Red","Rojo","Vermelho"), profile_label("Blue","Azul","Azul"),
        profile_label("Purple","Morado","Roxo"), profile_label("Green","Verde","Verde"),
        profile_label("Gray","Gris","Cinza"), profile_label("Black","Negro","Preto"),
        profile_label("Cyan","Cian","Ciano"), profile_label("Yellow","Amarillo","Amarelo")
    };
    const char *decorations[] = {
        profile_label("None","Ninguna","Nenhuma"), profile_label("Diamond","Diamante","Diamante"),
        profile_label("Halo","Halo","Halo"), profile_label("Triangle","Triángulo","Triângulo"),
        profile_label("Capsule","Cápsula","Cápsula"), profile_label("Star","Estrella","Estrela"),
        profile_label("Medallion","Medallón","Medalhão")
    };
    SDL_strlcpy(draft.avatar,current.avatar,sizeof(draft.avatar));
    SDL_strlcpy(draft.motion,current.motion,sizeof(draft.motion));
    SDL_strlcpy(draft.banner,current.banner,sizeof(draft.banner));
    SDL_strlcpy(draft.bannerMotion,current.bannerMotion,sizeof(draft.bannerMotion));
    const bool columns=ImGui::GetContentRegionAvail().x>=680*u;
    const float bodyStart=ImGui::GetCursorPosY();
    if (ImGui::BeginTable("##profile-layout",columns?2:1,ImGuiTableFlags_SizingStretchProp)) {
        if (columns) {
            ImGui::TableSetupColumn("##style",ImGuiTableColumnFlags_WidthStretch,1.05f);
            ImGui::TableSetupColumn("##preview",ImGuiTableColumnFlags_WidthStretch,0.95f);
        }
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.70f,0.92f,0.92f,1),"%s",profile_label("Appearance","Apariencia","Aparência"));
        auto combo=[u](const char *id,const char *label,int *value,const char *const *items,int count) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(172*u);
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo(id,value,items,count);
        };
        combo("##name-color",profile_label("Name color","Color del nombre","Cor do nome"),&draft.nameColor,nameColors,8);
        ImGui::TextUnformatted(profile_label("Banner color","Color del banner","Cor do banner"));
        ImGui::SameLine();ImGui::TextDisabled("%s",backgrounds[draft.background]);
        for(int i=0;i<8;++i){
            if(i)ImGui::SameLine(0,5*u);
            ImGui::PushID(i);
            const ImVec4 color=ImGui::ColorConvertU32ToFloat4(profileBannerColors[i]);
            if(ImGui::ColorButton(backgrounds[i],color,ImGuiColorEditFlags_NoTooltip,ImVec2(23*u,23*u)))draft.background=i;
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",backgrounds[i]);
            if(draft.background==i)ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),
                IM_COL32(255,218,111,255),3*u,0,2*u);
            ImGui::PopID();
        }
        if (current.premium)
            combo("##decoration",profile_label("Decoration","Decoración","Decoração"),&draft.decoration,decorations,7);
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.70f,0.92f,0.92f,1),"%s",profile_label("Images","Imágenes","Imagens"));
        ImGui::BeginDisabled(sProfilePicker==1);
        profile_media_control(current.premium?"avatar_auto":"avatar",profile_label("Profile photo","Foto de perfil","Foto de perfil"),
                              current.motion[0]?current.motion:current.avatar,false,u);
        profile_roster_picker(u);
        if (current.premium) {
            profile_media_control("banner",profile_label("Custom banner","Banner personalizado","Banner personalizado"),current.banner,false,u);
            profile_media_control("banner_motion",profile_label("Animated banner","Banner animado","Banner animado"),current.bannerMotion,true,u);
        }
        ImGui::EndDisabled();
        if (sProfilePicker==-1) ImGui::TextWrapped("%s",profile_label(
            "Could not import the image. Check its format and size (up to 4 MB).",
            "No se pudo importar la imagen. Revisa el formato y el tamaño (hasta 4 MB).",
            "Não foi possível importar a imagem. Confira o formato e o tamanho (até 4 MB)."));
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.70f,0.92f,0.92f,1),"%s",profile_label("Preview","Vista previa","Prévia"));
        const ImVec2 size(ImGui::GetContentRegionAvail().x,72*u);
        profile_card_draw(ImGui::GetWindowDrawList(),ImGui::GetCursorScreenPos(),size.x,size.y,name,draft);
        ImGui::Dummy(size);
        ImGui::TextColored(ImVec4(0.99f,0.81f,0.39f,1),"%s",current.premium?
            profile_label("Profile pass active","Pase de perfil activo","Passe de perfil ativo"):
            profile_label("Free profile","Perfil gratuito","Perfil gratuito"));
        if (!current.premium) ImGui::TextWrapped("%s",profile_label("The pass unlocks GIFs, banners and decorations.",
            "El pase habilita GIFs, banners y decoraciones.","O passe libera GIFs, banners e decorações."));
        if (current.checkout&&!current.premium) {
            char url[2048]={};
            const int checkout=Port_ProfileCheckout(0,url,sizeof(url));
            ImGui::BeginDisabled(checkout==1);
            if (ImGui::Button(profile_label("30 days · USD 1","30 días · 1 USD","30 dias · US$ 1")))
                Port_ProfileCheckout(1,nullptr,0);
            ImGui::SameLine();
            if (ImGui::Button(profile_label("Refresh pass","Actualizar pase","Atualizar passe"))) {
                Port_ProfileCardRequest(nullptr);
                previous=1;
            }
            ImGui::EndDisabled();
            if (checkout<0||(checkout==2&&!SDL_OpenURL(url)))
                ImGui::TextUnformatted(profile_label("Couldn't open checkout.","No se pudo abrir el pago.","Não foi possível abrir o pagamento."));
        }
        ImGui::EndTable();
    }
    if (columns) ImGui::SetCursorPosY(fmaxf(ImGui::GetCursorPosY(),bodyStart+345*u));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button(profile_label("Back","Volver","Voltar"),ImVec2(92*u,0))) open=false;
    ImGui::SameLine();
    const char *save=profile_label("Save changes","Guardar cambios","Salvar alterações");
    const float buttonWidth=ImGui::CalcTextSize(save).x+40*u;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+fmaxf(0.0f,ImGui::GetContentRegionAvail().x-buttonWidth));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.17f,0.10f,0.05f,1));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.95f,0.72f,0.26f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(1,0.86f,0.46f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.90f,0.62f,0.18f,1));
    ImGui::BeginDisabled(sProfilePicker==1);
    if (ImGui::Button(save,ImVec2(buttonWidth,0))) {
        Port_ProfileCardRequest(&draft);
        previous=1;
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor(4);
}

static void profile_card_editor_window(const char *name,bool &open) {
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    const float u=fmaxf(0.64f,fminf(1.37f,fminf(display.x/1050.0f,display.y/640.0f)));
    const ImVec2 size(fminf(display.x-20,790*u),fminf(display.y-22,493*u));
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y*0.46f),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0,0),display,IM_COL32(6,25,33,55));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(22*u,18*u));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10*u,6*u));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4*u);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(9*u,7*u));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(8*u,3*u));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.94f,0.97f,0.95f,1));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.16f,0.37f,0.46f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.23f,0.56f,0.57f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.20f,0.46f,0.48f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0.10f,0.24f,0.33f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0.18f,0.41f,0.49f,1));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0.06f,0.20f,0.27f,1));
    if (ImGui::Begin("##TBOProfileCard",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings)) {
        const ImVec2 p=ImGui::GetWindowPos();
        ImDrawList *dl=ImGui::GetWindowDrawList();
        ImGui::PushClipRect(p,ImVec2(p.x+size.x,p.y+size.y),false);
        profile_tbo_panel(dl,p,ImVec2(p.x+size.x,p.y+size.y),u);
        ImGui::PopClipRect();
        const ImVec2 titlePos=ImGui::GetCursorScreenPos();
        const float usable=ImGui::GetContentRegionAvail().x;
        profile_heading(dl,titlePos,usable-50*u,44*u,profile_label("Profile card","Tarjeta de perfil","Cartão de perfil"));
        ImGui::Dummy(ImVec2(usable-50*u,44*u));
        ImGui::SameLine();
        if(ImGui::Button("×##close-profile",ImVec2(41*u,44*u))) open=false;
        ImGui::Spacing();
        profile_card_editor(name,u,open);
    }
    ImGui::End();
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);
}
