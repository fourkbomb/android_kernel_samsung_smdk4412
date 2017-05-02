#include "mdnie.h"

#ifdef CONFIG_FB_MDNIE_RGB_ADJUST
#if defined(CONFIG_CPU_EXYNOS4210)
// kb R	SCR - from mdnie_color_tone_4210.h
#define MDNIE_SCR_START 0x00c8
// yw B
#define MDNIE_SCR_END 0x00d3

#define MDNIE_EFFECT_MASTER 0x0001
#define MDNIE_SCR_SHIFT 6
#define MDNIE_SCR_MASK (1 << MDNIE_SCR_SHIFT)

#define IS_SCR_RED(x) (x >= 0x00c8 && x <= 0x00cb)
#define IS_SCR_GREEN(x) (x >= 0x00cc && x <= 0x00cf)
#define IS_SCR_BLUE(x) (x >= 0x00d0 && x <= 0x00d3)
#else // CONFIG_CPU_EXYNOS4210
// SCR RrCr - from mdnie_color_tone.h
#define MDNIE_SCR_START 0x00e1
// SCR KbWb
#define MDNIE_SCR_END 0x00ec

#define MDNIE_EFFECT_MASTER 0x008

#define MDNIE_SCR_SHIFT 5
#define MDNIE_SCR_MASK (1 << MDNIE_SCR_SHIFT)

#define IS_SCR_RED(x) (x % 3 == 0)
#define IS_SCR_GREEN(x) ((x - 1) % 3 == 0)
#define IS_SCR_BLUE(x) ((x - 2) % 3 == 0)
#endif // CONFIG_CPU_EXYNOS4210
#endif //CONFIG_FB_MDNIE_RGB_ADJ

#ifdef CONFIG_FB_MDNIE_OUTDOOR
// "UC" from mdnie_table_c1m0.h
// no outdoor = 0x0000
// w/ outdoor = 0x0400
#define MDNIE_OUTDOOR_SHIFT 10
#define MDNIE_OUTDOOR_MASK (1 << MDNIE_OUTDOOR_SHIFT)

static const unsigned short mdnie_outdoor_default[] = {
    0x00d0, 0x01c0, /*UC y */
    0x00d1, 0x01ff, /*UC cs*/
    END_SEQ, 0x0000,
};

#define MDNIE_OUTDOOR_START 0x00d0
#define MDNIE_OUTDOOR_END 0x00d1

#define GET_OUTDOOR_VALUE(reg) mdnie_outdoor[reg - 0xd0 + 1]
#endif

struct mdnie_adj {
#ifdef CONFIG_FB_MDNIE_RGB_ADJ
    bool applied_rgb;
#endif
#ifdef CONFIG_FB_MDNIE_OUTDOOR
    bool applied_outdoor;
#endif
};

unsigned short mdnie_hook(struct mdnie_info *mdnie, struct mdnie_adjust *adj, unsigned short reg, unsigned short val);
