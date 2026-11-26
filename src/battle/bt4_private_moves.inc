/* Built from the active BT4 resources so model, voice language and control style stay local. */
#include "battle/bt4_private_balance.h"

typedef struct Bt4BalanceMoveEdits {
    s16 skillDonor;         /* fighter + 1; all replacements target Blast 1 slot 0 */
    s16 superDonor;         /* fighter + 1, same Blast 2 slot on the donor */
    s16 blastAnimDonor;     /* fighter + 1 */
    s16 throwDonor;         /* fighter + 1 */
    u8 superSlot;           /* zero-based Blast 2 slot */
    u8 step;                /* 1: keep attack and omit the retreat; 2+: override encoded entry */
    s8 combo[4];            /* native combo kind + 1; 0: leave alone */
} Bt4BalanceMoveEdits;

static const Bt4BalanceMoveEdits sBalanceMoves[250] = {
    [6]   = {.combo={0, 0, 0, 2}},
    [21]  = {.step=8}, /* Adult Gohan's entry kick without the optional retreat. */
    [34]  = {.skillDonor=33},
    [35]  = {.step=10}, /* Vegeta's first step-in attack. */
    [39]  = {.step=6},
    [40]  = {.step=6},
    [46]  = {.blastAnimDonor=45},
    [47]  = {.blastAnimDonor=46},
    [68]  = {.skillDonor=33, .step=1},
    [69]  = {.throwDonor=191, .step=1},
    [70]  = {.skillDonor=33, .superDonor=95, .superSlot=0},
    [71]  = {.step=1},
    [72]  = {.step=1},
    [73]  = {.skillDonor=33, .step=1},
    [75]  = {.skillDonor=135},
    [77]  = {.skillDonor=33, .blastAnimDonor=30},
    [80]  = {.blastAnimDonor=195},
    [87]  = {.blastAnimDonor=195},
    [99]  = {.step=1},
    [125] = {.step=1},
    [127] = {.step=1, .combo={0, 0, 0, 1}},
    [134] = {.skillDonor=33, .combo={0, 4, 0, 0}},
    [136] = {.step=1},
    [138] = {.skillDonor=135, .throwDonor=118},
    [142] = {.combo={0, 0, 0, 6}},
    [149] = {.step=1},
    [150] = {.skillDonor=98},
    [154] = {.skillDonor=191},
    [158] = {.combo={4, 0, 0, 0}},
    [159] = {.step=1},
    [160] = {.step=1},
    [166] = {.step=1},
    [183] = {.throwDonor=191},
    [189] = {.step=1},
    [199] = {.skillDonor=191},
    [208] = {.skillDonor=33},
    [210] = {.skillDonor=33, .combo={0, 0, 0, 6}},
    [215] = {.blastAnimDonor=195},
    [225] = {.step=1},
    [226] = {.step=1, .superDonor=37, .superSlot=1},
    [228] = {.combo={1, 3, 4, 6}},
};

static Bt4BalanceMoveEdits Bt4Balance_MoveEdits(s32 fighter) {
    Bt4BalanceMoveEdits edit = sBalanceMoves[fighter];
    if (!Bt4Balance_UsesOnlineRules()) return edit;
    switch (fighter) {
        case 3: edit.combo[3] = 3; break;
        case 69: edit.skillDonor = 191; break;
        case 73:
        case 77: edit.combo[3] = 4; break;
        case 103: edit.blastAnimDonor = 195; break;
        case 154:
        case 199: edit.skillDonor = 201; break;
        case 184: edit.blastAnimDonor = 45; break;
    }
    return edit;
}

static void *sBalanceText[250];
static void *sBalanceTables[250];
static void Bt4Balance_FreeMoves(void);

static u8 *Bt4Balance_Parameters(s32 chara) {
    return Bt4_PackEntry(Bt4_CharaPackById(chara, 0), 2);
}

/* The packed tables store each parameter as a column, with one value per slot. */
typedef struct Bt4BalanceColumn {
    u16 start, end;
    u8 size;
} Bt4BalanceColumn;

static void Bt4Balance_CopySlot(u8 *dst, s32 slot, const u8 *src, s32 sourceSlot, s32 slots,
                                const Bt4BalanceColumn *fields, s32 count) {
    s32 i, offset;
    for (i = 0; i < count; i++) {
        s32 size = fields[i].size;
        for (offset = fields[i].start; offset < fields[i].end; offset += slots * size) {
            memcpy(dst + offset + slot * size, src + offset + sourceSlot * size, size);
        }
    }
}

static void Bt4Balance_Skill(u8 *dst, s32 chara, s32 donor) {
    static const Bt4BalanceColumn columns[] = {
        {0, 0x10, 4}, {0x10, 0x18, 2}, {0x18, 0x38, 4}, {0x38, 0x74, 1},
        {0x74, 0x94, 4}, {0x94, 0xA0, 1}, {0xA0, 0xC8, 4}
    };
    u8 *src = Bt4Balance_Parameters(donor);
    u32 flags;
    if (src == NULL) return;
    Bt4Balance_CopySlot(dst, 0, src + 0x2C50, 0, 2, columns, sizeof(columns) / sizeof(columns[0]));
    if (chara == 73) {
        memcpy(&flags, dst + 8, sizeof(flags));
        flags |= 0x200;
        memcpy(dst + 8, &flags, sizeof(flags));
    }
}

static void Bt4Balance_Super(u8 *dst, s32 slot, const u8 *src) {
    static const Bt4BalanceColumn columns[] = {
        {0, 0x18, 4}, {0x18, 0x24, 2}, {0x24, 0x90, 4}, {0x90, 0x16B, 1},
        {0x16C, 0x208, 4}, {0x208, 0x220, 2}, {0x220, 0x22C, 1}, {0x22C, 0x244, 4}
    };
    Bt4Balance_CopySlot(dst, slot, src, slot, 3, columns, sizeof(columns) / sizeof(columns[0]));
}

static void Bt4Balance_Animation(BObjMdl *mdl, s32 donor, s32 first, s32 count) {
    s32 i;
    void *pack = Bt4_CharaPackById(donor, 3);
    if (pack == NULL) return;
    for (i = first; i < first + count; i++) {
        void *animation = Bt4_PackEntry(pack, i + 1);
        if (animation != NULL) mdl->anims[i] = animation;
    }
}

static void Bt4Balance_Movement(f32 *dst, s32 points) {
    /* The seven stock movement tiers, slowest first. */
    static const s16 speedTiers[] = {170, 190, 106, 39, 16, 91, 7};
    static const f32 anchors[] = {18.0f, 27.0f, 38.0f, 47.0f, 55.0f, 65.0f, 74.0f};
    const f32 *from, *to;
    u8 *src, *target;
    f32 best = 10000.0f;
    s32 i, tier = 0;
    for (i = 0; i < 7; i++) {
        f32 delta = __builtin_fabsf(dst[0] - anchors[i]);
        if (delta < best) {
            best = delta;
            tier = i;
        }
    }
    i = tier + points;
    if (i < 0) i = 0;
    if (i > 6) i = 6;
    if (tier == i) return;
    src = Bt4Balance_Parameters(speedTiers[tier]);
    target = Bt4Balance_Parameters(speedTiers[i]);
    if (src == NULL || target == NULL) return;
    from = (f32 *)(src + 0x2500);
    to = (f32 *)(target + 0x2500);
    for (i = 0; i < 22; i++) {
        dst[i] += to[i] - from[i];
        if (dst[i] < 0.0f) dst[i] = 0.0f;
    }
    /* Turn rates and accelerations vary with the tier. Angles, heights and ki costs remain native. */
    for (i = 29; i < 36; i++) dst[i] += to[i] - from[i];
}

static void Bt4Balance_FreeMoves(void) {
    s32 i;
    for (i = 0; i < 250; i++) {
        if (sBalanceTables[i] != NULL) Heap_Free(sBalanceTables[i]);
        if (sBalanceText[i] != NULL) Heap_Free(sBalanceText[i]);
        sBalanceTables[i] = NULL;
        sBalanceText[i] = NULL;
    }
}

static const u16 *Bt4Balance_LineEnd(const u16 *line) {
    while (*line != 0 && *line != '\n' && *line != '\r') line++;
    return line;
}

static const u16 *Bt4Balance_NextLine(const u16 *line) {
    line = Bt4Balance_LineEnd(line);
    while (*line == '\r' || *line == '\n') line++;
    return line;
}

static const u16 *Bt4Balance_ListRow(s32 chara, s32 row) {
    const u16 *text = Bt4_CharaPackById(chara, 1);
    if (text == NULL || text[0] != 0xFEFF) return NULL;
    for (text++; *text; text = Bt4Balance_NextLine(text)) {
        const u16 *end = Bt4Balance_LineEnd(text);
        if (end - text >= 6 && text[0] == '!' && text[2] == '*' && row-- == 0) return text;
    }
    return NULL;
}

static const u16 *Bt4Balance_ReplacementRow(s32 chara, const Bt4BalanceMoveEdits *edit, s32 row) {
    if (row == 0 && edit->skillDonor) return Bt4Balance_ListRow(edit->skillDonor - 1, 0);
    if (row == 2 + edit->superSlot && edit->superDonor) {
        return Bt4Balance_ListRow(edit->superDonor - 1, row);
    }
    if (row >= 5 && row <= 8 && edit->combo[row - 5]) {
        if (chara == 228) return Bt4Balance_ListRow(227, row);
        switch (edit->combo[row - 5] - 1) {
            case 0: return Bt4Balance_ListRow(3, 5);  /* Heavy Finish */
            case 1: return Bt4Balance_ListRow(3, 6);  /* Ki Cannon */
            case 2: return Bt4Balance_ListRow(31, 8); /* Ki Blast Cannon */
            case 3: return Bt4Balance_ListRow(54, 5); /* Flying Kicks */
            case 5: return Bt4Balance_ListRow(36, 6); /* Rolling Hammer */
        }
    }
    return NULL;
}

/* Native move-list entries contain their own controls, description and language markers. */
static s32 Bt4Balance_TitleMatches(const u16 *line, const u16 *title) {
    const u16 *end = Bt4Balance_LineEnd(line);
    if (end - line < 6 || line[0] != '!' || line[2] != '*') return 0;
    line += 6;
    while (line < end && *title && *line == *title) {
        ++line;
        ++title;
    }
    return line == end && *title == 0;
}

static u16 *Bt4Balance_AppendControl(u16 *out, s32 donor, const u16 *es, const u16 *en, s32 down) {
    const u16 *line = Bt4_CharaPackById(donor, 1);
    if (line == NULL || *line != 0xFEFF) return out;
    for (++line; *line; line = Bt4Balance_NextLine(line)) {
        if (!Bt4Balance_TitleMatches(line, es) && !Bt4Balance_TitleMatches(line, en)) continue;
        do {
            const u16 *end = Bt4Balance_LineEnd(line);
            while (line < end) {
                const u16 ch = *line++;
                *out++ = down && ch == 0x2191 ? 0x2193 : ch;
            }
            *out++ = '\n';
            line = Bt4Balance_NextLine(line);
        } while (*line && (Bt4Balance_LineEnd(line) - line < 3 || (line[2] != '*' && line[2] != '$')));
        break;
    }
    return out;
}

static u16 *Bt4Balance_ListText(s32 chara, u16 *text) {
    const Bt4BalanceMoveEdits editData = Bt4Balance_MoveEdits(chara);
    const Bt4BalanceMoveEdits *edit = &editData;
    const Bt4BalanceRule *rule = Bt4Balance_Rule(chara);
    const s32 online = Bt4Balance_UsesOnlineRules();
    const s32 groundThrow = online && (chara == 18 || chara == 19);
    const u8 *fighter = online && (rule->classFlags & BT4_BALANCE_GIANT) ? Bt4Balance_Parameters(chara) : NULL;
    const s32 expulsion = fighter != NULL && !(fighter[0x15] & 0x20);
    const u16 *line, *end, *donor = NULL;
    u16 *data, *out;
    s32 row = -1, chars = 0, held, title, override;
    if (text == NULL || text[0] != 0xFEFF) return text;
    if (!edit->skillDonor && !edit->superDonor && !edit->combo[0] && !edit->combo[1] &&
        !edit->combo[2] && !edit->combo[3] && !rule->blast[0] && !rule->blast[1] &&
        !rule->ki[0] && !rule->ki[1] && !groundThrow && !expulsion &&
        (online || (!rule->damage[0] && !rule->damage[1]))) return text;
    if (sBalanceText[chara] != NULL) return sBalanceText[chara];
    while (chars < 32768 && text[chars] != 0) chars++;
    if (chars == 32768) return text;
    data = (void *)Port_GameBigAlloc((chars + 1024) * sizeof(u16));
    if (data == NULL) return text;
    out = data;
    *out++ = 0xFEFF;
    held = 0;
    for (line = text + 1; *line; line = Bt4Balance_NextLine(line)) {
        end = Bt4Balance_LineEnd(line);
        title = end - line >= 6 && line[0] == '!' && line[2] == '*';
        if (title) {
            row++;
            donor = Bt4Balance_ReplacementRow(chara, edit, row);
            const u16 *label = donor != NULL ? donor : line;
            override = row < 2 ? rule->blast[row] : row < 4 ? rule->ki[row - 2] : 0;
            memcpy(out, line, 6 * sizeof(u16));
            out[3] = label[3];
            out[4] = label[4];
            out[5] = label[5];
            if (override) out[row < 2 ? 5 : 4] = '0' + override;
            out += 6;
            label += 6;
            while (*label != 0 && *label != '\n' && *label != '\r') *out++ = *label++;
            *out++ = '\n';
            held = donor != NULL || (chara == 150 && row == 2); /* Hakai is now blockable. */
            if (!Bt4Balance_UsesOnlineRules() && row >= 2 && row <= 3 && rule->damage[row - 2]) {
                s32 value = rule->damage[row - 2];
                extern s32 Port_GameProfileEnglish(void);
                static const u16 descEs[] = {'!', 'F', '%', '0', 'D', 'a', 0x00F1, 'o', ' ', 'b', 'a', 's', 'e', ':', ' ', 0};
                static const u16 descEn[] = {'!', 'F', '%', '0', 'B', 'a', 's', 'e', ' ', 'd', 'a', 'm', 'a', 'g', 'e', ':', ' ', 0};
                static const u16 descPt[] = {'!', 'F', '%', '0', 'D', 'a', 'n', 'o', ' ', 'b', 'a', 's', 'e', ':', ' ', 0};
                const u16 *desc = getenv("BT4_PTBR_ROOT") ? descPt : Port_GameProfileEnglish() ? descEn : descEs;
                while (*desc) *out++ = *desc++;
                *out++ = '0' + value / 1000;
                *out++ = '0' + (value / 100) % 10;
                *out++ = '0' + (value / 10) % 10;
                *out++ = '0' + value % 10;
                *out++ = '\n';
            }
            continue;
        }
        if (held && end - line >= 3 && line[2] == '%') continue;
        while (line < end) *out++ = *line++;
        *out++ = '\n';
    }
    if (groundThrow) {
        static const u16 es[] = {'L','a','n','z','a','m','i','e','n','t','o',' ','G','i','g','a','n','t','e',0};
        static const u16 en[] = {'G','i','a','n','t',' ','T','h','r','o','w',0};
        out = Bt4Balance_AppendControl(out, 3, es, en, 1);
    }
    if (expulsion) {
        static const u16 es[] = {'A','t','a','q','u','e',' ','d','e',' ','E','x','p','u','l','s','i',0xF3,'n',0};
        static const u16 en[] = {'V','a','n','i','s','h','i','n','g',' ','A','t','t','a','c','k',0};
        out = Bt4Balance_AppendControl(out, 108, es, en, 0);
    }
    *out = 0;
    sBalanceText[chara] = data;
    return data;
}

/* The rush camera tracks live in entry 8 of the movement effects pack. */
void *Bt4Balance_TechCamera(s32 chara, s32 slot) {
    Bt4BalanceMoveEdits edit;
    void *pack;
    if (chara < 0 || chara >= 250 || slot < 0 || slot >= 3) return NULL;
    edit = Bt4Balance_MoveEdits(chara);
    if (!edit.superDonor || edit.superSlot != slot) return NULL;
    pack = Bt4_PackEntry(Bt4_CharaPackById(edit.superDonor - 1, 4), 6);
    pack = Bt4_PackEntry(pack, 8);
    return Bt4_PackEntry(pack, slot + 1);
}

static void Bt4Balance_BindMoves(BObjMdl *mdl, s32 chara, u8 *tables) {

    Bt4BalanceMoveEdits editData;
    const Bt4BalanceMoveEdits *edit = &editData;
    const Bt4BalanceRule *rule;
    u8 *dst, *src, *pack;
    s32 i, donation, slot;
    if (chara < 0 || chara >= 250) return;
    editData = Bt4Balance_MoveEdits(chara);
    rule = Bt4Balance_Rule(chara);
    mdl->unkA4 = Bt4Balance_ListText(chara, mdl->unkA4);
    const s32 online = Bt4Balance_UsesOnlineRules();
    s32 activeCharge;
    memcpy(&activeCharge, tables + 0x30, sizeof(activeCharge));
    const s32 gohanGroundThrow = online && (chara == 18 || chara == 19);
    if ((online && activeCharge > 0) || gohanGroundThrow || edit->skillDonor || edit->superDonor || edit->throwDonor || edit->step || rule->speed ||
        edit->combo[0] || edit->combo[1] || edit->combo[2] || edit->combo[3] ||
        chara == 4 || chara == 25 || chara == 167 || chara == 1) {
        dst = sBalanceTables[chara];
        if (dst == NULL) {
            dst = (void *)Port_GameBigAlloc(0x2D50);
            if (dst == NULL) return;
            memcpy(dst, tables, 0x2D50);
            /* Use the native charge columns of the comparison fighter, including underwater charge. */
            if (online && activeCharge > 0) {
                const s32 reference = rule->classFlags & (BT4_BALANCE_GIANT | BT4_BALANCE_TANK) ? 209 : 0;
                src = Bt4Balance_Parameters(reference);
                if (src != NULL) memcpy(dst + 0x30, src + 0x30, 2 * sizeof(s32));
            }
            if (rule->speed) Bt4Balance_Movement((void *)(dst + 0x2500), rule->speed);
            if (edit->step == 1) dst[0x8A] &= ~1;
            else if (edit->step > 1) dst[0x8A] = edit->step - 2;
            for (i = 0; i < 4; i++) if (edit->combo[i]) dst[0x84+i] = edit->combo[i]-1;
            if (edit->skillDonor) Bt4Balance_Skill(dst + 0x2C50, chara, edit->skillDonor - 1);
            if (chara == 167) {
                u32 flags;
                memcpy(&flags, dst + 0x2C5C, sizeof(flags));
                flags |= 0x200;
                memcpy(dst + 0x2C5C, &flags, sizeof(flags));
            }
            if (chara == 1) dst[0x29DC + 8] |= 1;
            if (edit->superDonor) {
                src = Bt4Balance_Parameters(edit->superDonor - 1);
                if (src != NULL) Bt4Balance_Super(dst + 0x29D0, edit->superSlot, src + 0x29D0);
            }
            if (gohanGroundThrow) {
                src = Bt4Balance_Parameters(3);
                if (src != NULL) memcpy(dst + 0x100 + 0x56 * 0x30, src + 0x100 + 0x56 * 0x30, 0x30);
            }
            if (chara == 25) ((f32 *)(dst + 0x29D0 + 0x60))[1] = 100.0f;
            if (chara == 70) {
                ((f32 *)(dst + 0x29D0 + 0x1FC))[1] *= 1.15f;
                ((f32 *)(dst + 0x29D0 + 0x24))[1] *= 1.15f;
            }
            if (edit->throwDonor) {
                src = Bt4Balance_Parameters(edit->throwDonor - 1);
                if (src != NULL) {
                    memcpy(dst + 0x100 + 0x55 * 0x30, src + 0x100 + 0x55 * 0x30, 0x30);
                    if (chara == 183) {
                        /* Use Broly's release path rather than the knock-to-ground throw. */
                        const u32 slam = 0x200;
                        u32 flags, donorFlags;
                        memcpy(&flags, dst + 0x10, sizeof(flags));
                        memcpy(&donorFlags, src + 0x10, sizeof(donorFlags));
                        flags = (flags & ~slam) | (donorFlags & slam);
                        memcpy(dst + 0x10, &flags, sizeof(flags));
                    }
                }
            }
            if (chara == 4) {
                src = Bt4Balance_Parameters(2);
                if (src != NULL) memcpy(dst + 0x100, src + 0x100, 0x30);
            }
            sBalanceTables[chara] = dst;
        }
        mdl->unk904 = (s8 *)dst;
        mdl->unk908 = dst + 0x100;
        mdl->unk910 = dst + 0x2500;
        mdl->unk914 = dst + 0x29D0;
        mdl->unk918 = dst + 0x2C50;
    }
    if (edit->skillDonor) {
        donation = edit->skillDonor - 1;
        Bt4Balance_Animation(mdl, donation, 0x103, 1);
        mdl->unk84[0] = Bt4_PackEntry(Bt4_CharaPackById(donation, 4), 1);
    }
    if (edit->blastAnimDonor) {
        Bt4Balance_Animation(mdl, edit->blastAnimDonor - 1, 0x71, chara == 87 ? 0x1F : 6);
    }
    if (chara == 183) {
        Bt4Balance_Animation(mdl, edit->throwDonor - 1, 0x94, 5);
    } else if (edit->throwDonor) {
        Bt4Balance_Animation(mdl, edit->throwDonor - 1, 0x96, 3);
    }
    if (chara == 4) Bt4Balance_Animation(mdl, 2, 0x37, 1);
    if (gohanGroundThrow) {
        Bt4Balance_Animation(mdl, 3, 0x186, 3);
        Bt4Balance_Animation(mdl, 3, 0x18A, 1);
    }
    /* Rolling Hammer uses its own wind-up, hold, release and follow-through motions. */
    if (chara == 35) Bt4Balance_Animation(mdl, online ? 38 : 36, 0x199, 4);
    if (online) {
        if (chara == 3) Bt4Balance_Animation(mdl, 31, 0x90, 3);
        if (chara == 203) Bt4Balance_Animation(mdl, 3, 0x90, 3);
        if (chara == 73) Bt4Balance_Animation(mdl, 59, 0x93, 1);
        if (chara == 77 || chara == 151) Bt4Balance_Animation(mdl, 29, 0x93, 1);
        if (chara == 126 || chara == 230) Bt4Balance_Animation(mdl, 94, 0x93, 1);
        if (chara == 124 || chara == 201 || chara == 202 || chara == 204 || chara == 216) {
            Bt4Balance_Animation(mdl, 108, 0x47, 3);
        }
    }
    if (chara == 68) Bt4Balance_Animation(mdl, 36, 0x56, 6);
    if (chara == 134) Bt4Balance_Animation(mdl, 15, 0x93, 1);
    if (chara == 158) Bt4Balance_Animation(mdl, 54, 0x93, 1);
    donation = edit->superDonor - 1;
    slot = edit->superSlot;
    if (donation >= 0) {
        Bt4Balance_Animation(mdl, donation, 0x105 + 0x22 * slot, 0x22);
        pack = Bt4_CharaPackById(donation, 4);
        mdl->unk84[slot + 2] = Bt4_PackEntry(pack, slot + 3);
        pack = Bt4_CharaPackById(donation, 0);
        ((void **)mdl->unk98)[slot] = Bt4_PackEntry(Bt4_PackEntry(pack, 3), slot + 1);
    }
}
