#pragma once

#include <OfficeTypes.h>

#include <cstddef>

// Localized names for Rotunda screens (weekdays, months, offices).
namespace RotundaText {

// weekday: 0 = Sunday.
const char* weekdayName(int weekday, bool shortForm = false);
// month: 1-12.
const char* monthName(int month, bool shortForm = false);
const char* officeName(rotunda::Office office);       // "Morning Prayer"
const char* officeShortName(rotunda::Office office);  // "Morning"

// "Saturday 17 October"
void formatLongDate(const rotunda::Date& d, char* out, size_t cap);
// "Fri 16 Oct"
void formatShortDate(const rotunda::Date& d, char* out, size_t cap);
// "Thu 22 Oct"-style meeting day.
void formatMeetingDay(const rotunda::Date& d, char* out, size_t cap);

}  // namespace RotundaText
