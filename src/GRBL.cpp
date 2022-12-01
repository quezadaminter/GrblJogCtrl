#include "GRBL.h"

const char *grblStatusImage[eStatusRange] =
{
#define ADD_ENUM(eVal) #eVal,
   GRBL_STATUS_ENUM
#undef ADD_ENUM
};