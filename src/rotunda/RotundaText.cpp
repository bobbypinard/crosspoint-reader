#include "RotundaText.h"

#include <I18n.h>

#include <cstdio>

namespace RotundaText {

namespace {
constexpr StrId WEEKDAYS[7] = {StrId::STR_ROT_SUNDAY,    StrId::STR_ROT_MONDAY,   StrId::STR_ROT_TUESDAY,
                               StrId::STR_ROT_WEDNESDAY, StrId::STR_ROT_THURSDAY, StrId::STR_ROT_FRIDAY_LONG,
                               StrId::STR_ROT_SATURDAY};
constexpr StrId WEEKDAYS_SHORT[7] = {StrId::STR_ROT_SUN, StrId::STR_ROT_MON, StrId::STR_ROT_TUE, StrId::STR_ROT_WED,
                                     StrId::STR_ROT_THU, StrId::STR_ROT_FRI, StrId::STR_ROT_SAT};
constexpr StrId MONTHS[12] = {StrId::STR_ROT_JANUARY, StrId::STR_ROT_FEBRUARY, StrId::STR_ROT_MARCH,
                              StrId::STR_ROT_APRIL,   StrId::STR_ROT_MAY,      StrId::STR_ROT_JUNE,
                              StrId::STR_ROT_JULY,    StrId::STR_ROT_AUGUST,   StrId::STR_ROT_SEPTEMBER,
                              StrId::STR_ROT_OCTOBER, StrId::STR_ROT_NOVEMBER, StrId::STR_ROT_DECEMBER};
constexpr StrId MONTHS_SHORT[12] = {StrId::STR_ROT_JAN, StrId::STR_ROT_FEB,       StrId::STR_ROT_MAR,
                                    StrId::STR_ROT_APR, StrId::STR_ROT_MAY_SHORT, StrId::STR_ROT_JUN,
                                    StrId::STR_ROT_JUL, StrId::STR_ROT_AUG,       StrId::STR_ROT_SEP,
                                    StrId::STR_ROT_OCT, StrId::STR_ROT_NOV,       StrId::STR_ROT_DEC};
constexpr StrId OFFICES[rotunda::OFFICE_COUNT] = {StrId::STR_ROT_MORNING_PRAYER, StrId::STR_ROT_NOONDAY_PRAYER,
                                                  StrId::STR_ROT_EVENING_PRAYER, StrId::STR_ROT_COMPLINE};
constexpr StrId OFFICES_SHORT[rotunda::OFFICE_COUNT] = {StrId::STR_ROT_MORNING, StrId::STR_ROT_NOONDAY,
                                                        StrId::STR_ROT_EVENING, StrId::STR_ROT_COMPLINE};
}  // namespace

const char* weekdayName(const int weekday, const bool shortForm) {
  if (weekday < 0 || weekday > 6) return "";
  return I18N.get(shortForm ? WEEKDAYS_SHORT[weekday] : WEEKDAYS[weekday]);
}

const char* monthName(const int month, const bool shortForm) {
  if (month < 1 || month > 12) return "";
  return I18N.get(shortForm ? MONTHS_SHORT[month - 1] : MONTHS[month - 1]);
}

const char* officeName(const rotunda::Office office) {
  const int i = static_cast<int>(office);
  return i >= 0 && i < rotunda::OFFICE_COUNT ? I18N.get(OFFICES[i]) : "";
}

const char* officeShortName(const rotunda::Office office) {
  const int i = static_cast<int>(office);
  return i >= 0 && i < rotunda::OFFICE_COUNT ? I18N.get(OFFICES_SHORT[i]) : "";
}

void formatLongDate(const rotunda::Date& d, char* out, const size_t cap) {
  snprintf(out, cap, "%s %d %s", weekdayName(rotunda::weekday(d)), d.day, monthName(d.month));
}

void formatShortDate(const rotunda::Date& d, char* out, const size_t cap) {
  snprintf(out, cap, "%s %d %s", weekdayName(rotunda::weekday(d), true), d.day, monthName(d.month, true));
}

void formatMeetingDay(const rotunda::Date& d, char* out, const size_t cap) { formatShortDate(d, out, cap); }

}  // namespace RotundaText
