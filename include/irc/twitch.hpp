#pragma once

namespace irc {
namespace twitch {
namespace tags {
// PRIVMSG
constexpr const char* BADGE_INFO = "badge-info";
constexpr const char* BADGES = "badges";
constexpr const char* BITS = "bits";
constexpr const char* COLOR = "color";
constexpr const char* DISPLAY_NAME = "display-name";
constexpr const char* EMOTES = "emotes";
constexpr const char* ID = "id";
constexpr const char* MESSAGE = "message";
constexpr const char* MOD = "mod";
constexpr const char* ROOM_ID = "room-id";
constexpr const char* SUBSCRIBER = "subscriber";
constexpr const char* TMI_SENT_TS = "tmi-sent-ts";
constexpr const char* TURBO = "turbo";
constexpr const char* USER_ID = "user-id";
constexpr const char* USER_TYPE = "user-type";

// CLEARCHAT
constexpr const char* BAN_DURATION = "ban-duration";
// constexpr const char* ROOM_ID = "room-id";
constexpr const char* TARGET_USER_ID = "target-user-id";
// constexpr const char* TMI_SENT_TS = "tmi-sent-ts";

// USERNOTICE
// constexpr const char* BADGE_INFO = "badge-info";
// constexpr const char* BADGES = "badges";
// constexpr const char* COLOR = "color";
// constexpr const char* DISPLAY_NAME = "display-name";
// constexpr const char* EMOTES = "emotes";
constexpr const char* FLAGS = "flags";
// constexpr const char* ID = "id";
constexpr const char* LOGIN = "login";
// constexpr const char* MOD = "mod";
constexpr const char* MSG_ID = "msg-id";
constexpr const char* MSG_PARAM_CUMULATIVE_MONTHS = "msg-param-cumulative-months";
constexpr const char* MSG_PARAM_MONTHS = "msg-param-months";
constexpr const char* MSG_PARAM_MULTIMONTH_DURATION = "msg-param-multimonth-duration";
constexpr const char* MSG_PARAM_MULTIMONTH_TENURE = "msg-param-multimonth-tenure";
constexpr const char* MSG_PARAM_SHOULD_SHARE_STREAK = "msg-param-should-share-streak";
constexpr const char* MSG_PARAM_SUB_PLAN_NAME = "msg-param-sub-plan-name";
constexpr const char* MSG_PARAM_SUB_PLAN = "msg-param-sub-plan";
constexpr const char* MSG_PARAM_WAS_GIFTED = "msg-param-was-gifted";
// constexpr const char* ROOM_ID = "room-id";
// constexpr const char* SUBSCRIBER = "subscriber";
constexpr const char* SYSTEM_MSG = "system-msg";
// constexpr const char* TMI_SENT_TS = "tmi-sent-ts";
// constexpr const char* USER_ID = "user-id";
// constexpr const char* USER_TYPE = "user-type";

// ROOMSTATE
constexpr const char* EMOTE_ONLY = "emote-only";
constexpr const char* FOLLOWERS_ONLY = "followers-only";
constexpr const char* R9K = "r9k";
constexpr const char* RITUALS = "rituals";
// constexpr const char* ROOM_ID = "room-id";
constexpr const char* SLOW = "slow";
constexpr const char* SUBS_ONLY = "subs-only";

// NOTICE
// constexpr const char* MSG_ID = "msg-id";
} // namespace tags
} // namespace twitch

namespace out {
namespace twitch {
std::string nickAnon() {
  return nick("justinfan93434586");
}

std::string authAnon() {
  return nickAnon();
}

std::string whisper(std::string_view chn, std::string_view usr, std::string_view msg) {
  return privmsg(chn, "/w "s + (std::string)usr + " "s + (std::string)msg);
}

std::string capreqMembership() {
  return capreq("twitch.tv/membership");
}

std::string capreqTags() {
  return capreq("twitch.tv/tags");
}

std::string capreqCommands() {
  return capreq("twitch.tv/commands");
}
} // namespace twitch
} // namespace out
} // namespace irc
