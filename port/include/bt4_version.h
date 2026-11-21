#ifndef BT4_VERSION_H
#define BT4_VERSION_H
/* Upstream engine versions and BT4 revisions are independent release axes. */
#define BT4_UPSTREAM_VERSION "0.1.39.5"
#define BT4_PROJECT_VERSION "0.1.39.6"
#define BT4_REVISION 1
#define BT4_REVISION_PATCH 19
#define BT4_DISPLAY_VERSION "0.1.39.6 R1 1.11"
#if defined(BT4_TAG_TEAM_PREVIEW)
#undef BT4_DISPLAY_VERSION
#define BT4_DISPLAY_VERSION "0.1.39.6 R1 1.11 Test"
#elif defined(BT4_DEVELOPMENT_BUILD)
#undef BT4_DISPLAY_VERSION
#define BT4_DISPLAY_VERSION "0.1.39.6 R1 1.11 Dev"
#endif
#define BT4_RELEASE_TITLE "0.1.39.6 R1 1.11"
#define BT4_CANONICAL_VERSION "0.1.39.6-R1.19"
#define BT4_LEGACY_BRIDGE_VERSION "0.1.39.5"
#endif
